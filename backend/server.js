const express = require('express');
const fs = require('fs');
const path = require('path');
const cors = require('cors');
const { execFile } = require('child_process');
const {
  loadEnvFile,
  getAiStatus,
  parseAction,
  generateReply,
  executeAction,
} = require('./ai-chat');

const app = express();
const ROOT = path.join(__dirname, '..');
loadEnvFile(ROOT);
const USERS_FILE = path.join(ROOT, 'users.txt');
const TRIPS_FILE = path.join(ROOT, 'trips.txt');
const COORDS_JSON = path.join(ROOT, 'coords.json');
const CLI_NAME = process.platform === 'win32' ? 'route_cli.exe' : 'route_cli';
const CLI_PATH = path.join(ROOT, CLI_NAME);

app.use(cors());
app.use(express.json());
app.use(express.static(ROOT));

function runCli(args) {
  return new Promise((resolve, reject) => {
    execFile(CLI_PATH, args, { cwd: ROOT, maxBuffer: 10 * 1024 * 1024 }, (err, stdout, stderr) => {
      const text = (stdout || stderr || '').trim();
      try {
        const json = JSON.parse(text);
        if (err && !json.success) reject(new Error(json.message || stderr || 'CLI failed'));
        else resolve(json);
      } catch {
        if (err) reject(new Error(stderr || stdout || err.message));
        else reject(new Error('Invalid JSON from backend: ' + text.slice(0, 200)));
      }
    });
  });
}

function parseUsers(raw) {
  return raw
    .split('\n')
    .map((l) => l.trim())
    .filter(Boolean)
    .map((line) => {
      const [email, password] = line.split('|');
      return { email, password };
    });
}

function parseTrips(raw, emailFilter) {
  const trips = [];
  const lines = raw.split('\n').map((l) => l.trim()).filter(Boolean);
  for (const line of lines) {
    const parts = line.split('|');
    if (parts.length < 6) continue;
    const [email, source, destination, date, distance, cost] = parts;
    if (emailFilter && email !== emailFilter) continue;
    trips.push({
      email,
      source,
      destination,
      date,
      distance: parseFloat(distance),
      cost: parseFloat(cost),
    });
  }
  return trips;
}

// ---------- AUTH (C++ writes users.txt: email|password) ----------
app.post('/api/signup', async (req, res) => {
  const { email, password } = req.body;
  if (!email || !password) {
    return res.status(400).json({ success: false, message: 'Email and password required' });
  }
  try {
    const data = await runCli(['signup', email, password]);
    res.json(data);
  } catch (e) {
    res.status(500).json({ success: false, message: e.message });
  }
});

app.post('/api/login', async (req, res) => {
  const { email, password } = req.body;
  if (!email || !password) {
    return res.status(400).json({ success: false, message: 'Email and password required' });
  }
  try {
    const data = await runCli(['login', email, password]);
    res.status(data.success ? 200 : 401).json(data);
  } catch (e) {
    res.status(500).json({ success: false, message: e.message });
  }
});

// ---------- TRIPS (trips.txt: email|source|dest|date|distance|cost) ----------
app.get('/api/trips', (req, res) => {
  const email = req.query.email;
  if (!email) {
    return res.status(400).json({ success: false, message: 'email query required' });
  }
  fs.readFile(TRIPS_FILE, 'utf8', (err, data) => {
    if (err && err.code === 'ENOENT') {
      return res.json({ success: true, trips: [] });
    }
    if (err) return res.status(500).json({ success: false, message: 'Error reading trips' });
    res.json({ success: true, trips: parseTrips(data, email) });
  });
});

app.post('/api/trips', async (req, res) => {
  const { email, source, destination, date, distance, cost } = req.body;
  if (!email || !source || !destination) {
    return res.status(400).json({ success: false, message: 'Missing trip fields' });
  }
  const d = date || new Date().toISOString().slice(0, 10);
  const dist = distance != null ? distance : 0;
  const c = cost != null ? cost : 0;
  try {
    const data = await runCli([
      'addtrip',
      email,
      source,
      destination,
      d,
      String(dist),
      String(c),
    ]);
    res.json(data);
  } catch (e) {
    res.status(500).json({ success: false, message: e.message });
  }
});

// ---------- GEOCODE -> coords.json + coords.txt ----------
app.post('/api/geocode', async (req, res) => {
  const { source, destination } = req.body;
  if (!source || !destination) {
    return res.status(400).json({ success: false, message: 'source and destination required' });
  }
  try {
    const data = await runCli(['geocode', source, destination]);
    res.json(data);
  } catch (e) {
    res.status(500).json({ success: false, message: e.message });
  }
});

app.get('/api/coords', (req, res) => {
  fs.readFile(COORDS_JSON, 'utf8', (err, data) => {
    if (err) return res.status(404).json({ success: false, message: 'coords.json missing' });
    try {
      res.json(JSON.parse(data));
    } catch {
      res.status(500).json({ success: false, message: 'Invalid coords.json' });
    }
  });
});

// ---------- PATHFIND (DFS / BFS / Dijkstra) ----------
app.post('/api/path', async (req, res) => {
  const { source, destination, algorithm } = req.body;
  const algo = (algorithm || 'dijkstra').toLowerCase();
  if (!source || !destination) {
    return res.status(400).json({ success: false, message: 'source and destination required' });
  }
  try {
    const data = await runCli(['path', algo, source, destination]);
    res.json(data);
  } catch (e) {
    res.status(500).json({ success: false, message: e.message });
  }
});

app.get('/api/health', (req, res) => {
  const cliExists = fs.existsSync(CLI_PATH);
  const ai = getAiStatus();
  res.json({
    ok: true,
    cli: cliExists ? CLI_NAME : 'missing — compile pathfinder.cpp',
    ai,
    files: {
      users: fs.existsSync(USERS_FILE),
      trips: fs.existsSync(TRIPS_FILE),
      coords: fs.existsSync(path.join(ROOT, 'coords.txt')),
    },
  });
});

// ---------- AI CHAT (OpenAI / Ollama / local fallback) ----------
app.post('/api/chat', async (req, res) => {
  const { message, history = [], email, lastRoute } = req.body;
  if (!message || !String(message).trim()) {
    return res.status(400).json({ success: false, message: 'message required' });
  }

  const ctx = { email: email || '', lastRoute: lastRoute || null };

  try {
    const { text, provider } = await generateReply(String(message).trim(), history, ctx);
    const { cleanReply, action } = parseAction(text);

    let actionResult = null;
    if (action) {
      actionResult = await executeAction(action, {
        runCli,
        parseTrips,
        tripsFile: TRIPS_FILE,
        email: ctx.email,
        lastRoute: ctx.lastRoute,
      });
    }

    res.json({
      success: true,
      reply: cleanReply,
      provider,
      action,
      actionResult,
    });
  } catch (e) {
    res.status(500).json({ success: false, message: e.message });
  }
});

app.get('/', (req, res) => {
  res.sendFile(path.join(ROOT, 'app.html'));
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`SmartRouteX API: http://localhost:${PORT}`);
  console.log(`Project root: ${ROOT}`);
  if (!fs.existsSync(CLI_PATH)) {
    console.warn(`WARNING: ${CLI_NAME} not found. Run: g++ -o ${CLI_NAME} pathfinder.cpp`);
  }
});
