const fs = require('fs');
const path = require('path');

const SYSTEM_PROMPT = `You are SmartRouteX AI, a friendly travel assistant for an Indian route-planning app.
Backend uses C++ (DFS, BFS, Dijkstra) on coords.txt; trips stored as email|source|destination|date|distance|cost in trips.txt.

Help users with:
- Route planning between Indian cities (use lowercase city names from the graph when possible, e.g. delhi, mumbai, dehradun)
- Explaining DFS, BFS, and Dijkstra
- Login, trips, and how to use the app

When the user clearly wants you to RUN something on the backend, add exactly one line at the end:
ACTION:{"type":"<type>","source":"...","destination":"...","algorithm":"dijkstra|bfs|dfs","email":"..."}

Action types:
- find_path — needs source, destination, algorithm (default dijkstra)
- view_trips — needs email (logged-in user)
- save_trip — needs email, source, destination (uses last route stats if available)

Do NOT use ACTION for general questions or explanations. Keep replies concise (under 120 words unless listing trips).`;

function loadEnvFile(root) {
  const envPath = path.join(root, '.env');
  if (!fs.existsSync(envPath)) return;
  for (const line of fs.readFileSync(envPath, 'utf8').split('\n')) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith('#')) continue;
    const eq = trimmed.indexOf('=');
    if (eq === -1) continue;
    const key = trimmed.slice(0, eq).trim();
    let val = trimmed.slice(eq + 1).trim();
    if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith("'") && val.endsWith("'"))) {
      val = val.slice(1, -1);
    }
    if (!process.env[key]) process.env[key] = val;
  }
}

function getAiStatus() {
  if (process.env.OPENAI_API_KEY) {
    return { available: true, provider: 'openai', model: process.env.OPENAI_MODEL || 'gpt-4o-mini' };
  }
  const ollama = process.env.OLLAMA_URL || process.env.OLLAMA_BASE_URL;
  if (ollama) {
    return { available: true, provider: 'ollama', model: process.env.OLLAMA_MODEL || 'llama3.2' };
  }
  return { available: true, provider: 'local', model: 'keyword-assistant' };
}

function parseAction(text) {
  const match = text.match(/ACTION:\s*(\{[\s\S]*?\})\s*$/m);
  if (!match) return { cleanReply: text.trim(), action: null };
  try {
    const action = JSON.parse(match[1]);
    const cleanReply = text.replace(match[0], '').trim();
    return { cleanReply, action };
  } catch {
    return { cleanReply: text.trim(), action: null };
  }
}

async function callOpenAI(messages) {
  const base = (process.env.OPENAI_BASE_URL || 'https://api.openai.com/v1').replace(/\/$/, '');
  const model = process.env.OPENAI_MODEL || 'gpt-4o-mini';
  const res = await fetch(`${base}/chat/completions`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      Authorization: `Bearer ${process.env.OPENAI_API_KEY}`,
    },
    body: JSON.stringify({ model, messages, temperature: 0.6, max_tokens: 500 }),
  });
  if (!res.ok) {
    const err = await res.text();
    throw new Error(`OpenAI error (${res.status}): ${err.slice(0, 200)}`);
  }
  const data = await res.json();
  return data.choices[0].message.content;
}

async function callOllama(messages) {
  const base = (process.env.OLLAMA_URL || process.env.OLLAMA_BASE_URL || 'http://localhost:11434').replace(/\/$/, '');
  const model = process.env.OLLAMA_MODEL || 'llama3.2';
  const res = await fetch(`${base}/api/chat`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ model, messages, stream: false }),
  });
  if (!res.ok) {
    const err = await res.text();
    throw new Error(`Ollama error (${res.status}): ${err.slice(0, 200)}`);
  }
  const data = await res.json();
  return data.message?.content || '';
}

function localAiReply(userMessage, ctx) {
  const msg = userMessage.toLowerCase();
  let action = null;
  let reply = '';

  const routeMatch = msg.match(/\bfrom\s+([a-z][a-z\s]*?)\s+to\s+([a-z][a-z\s]*?)(?:\s+using|\s+with|\s+by|\?|$)/i)
    || msg.match(/(?:route|path|travel|go)\s+(?:from\s+)?([a-z][a-z\s]*?)\s+(?:to|→)\s+([a-z][a-z\s]*?)(?:\s+using|\s+with|\s+by|\?|$)/i)
    || msg.match(/\b([a-z]+)\s+to\s+([a-z]+)\b/i);

  let algo = 'dijkstra';
  if (msg.includes('dfs')) algo = 'dfs';
  else if (msg.includes('bfs')) algo = 'bfs';

  if (msg.includes('view trip') || msg.includes('my trips') || msg.includes('show trips')) {
    if (!ctx.email) {
      reply = 'Please log in first — I can then load your trips from trips.txt.';
    } else {
      action = { type: 'view_trips', email: ctx.email };
      reply = 'Let me fetch your saved trips from the backend.';
    }
  } else if (msg.includes('save trip') || msg.includes('save my route')) {
    if (!ctx.email) reply = 'Log in first to save trips.';
    else if (!ctx.lastRoute) reply = 'Find a route first, then ask me to save it.';
    else {
      action = {
        type: 'save_trip',
        email: ctx.email,
        source: ctx.lastRoute.source,
        destination: ctx.lastRoute.destination,
      };
      reply = 'Saving your last route to trips.txt.';
    }
  } else if (routeMatch) {
    const source = routeMatch[1].trim().replace(/\s+/g, ' ');
    const dest = routeMatch[2].trim().replace(/\s+/g, ' ');
    if (!ctx.email) {
      reply = `I can plan ${source} → ${dest} with ${algo.toUpperCase()} after you log in. Use the login form or say "login email password" in Rules mode.`;
    } else {
      action = { type: 'find_path', source, destination: dest, algorithm: algo, email: ctx.email };
      reply = `Planning ${source} → ${dest} using ${algo.toUpperCase()} on the C++ graph…`;
    }
  } else if (msg.includes('dijkstra')) {
    reply = 'Dijkstra finds the shortest path by total distance (km). Best for minimizing travel distance. Want a route? Say e.g. "Find route from delhi to dehradun".';
  } else if (msg.includes('dfs')) {
    reply = 'DFS explores deeply along one branch before backtracking — useful for exploring possible routes, not always shortest.';
  } else if (msg.includes('bfs')) {
    reply = 'BFS explores cities layer by layer — finds paths with fewer intermediate stops.';
  } else if (msg.includes('hello') || msg.includes('hi')) {
    reply = `Hello! I'm SmartRouteX AI (local mode). Ask naturally: "Route from delhi to mumbai", "Explain dijkstra", or "Show my trips".${ctx.email ? ` Logged in as ${ctx.email}.` : ' Log in to plan routes.'}`;
  } else {
    reply = `I'm running in local AI mode (no API key). I can help with routes, algorithms, and trips.\n\nTry: "Find route from dehradun to delhi using dijkstra"\nOr switch to **Rules** mode and type \`help\`.\n\nFor full AI, add OPENAI_API_KEY or OLLAMA_URL to .env — see .env.example.`;
  }

  if (action) reply += `\nACTION:${JSON.stringify(action)}`;
  return reply;
}

async function generateReply(userMessage, history, ctx) {
  const system = `${SYSTEM_PROMPT}\n\nContext: user=${ctx.email || 'guest'}, loggedIn=${!!ctx.email}, lastRoute=${ctx.lastRoute ? JSON.stringify(ctx.lastRoute) : 'none'}`;

  const messages = [
    { role: 'system', content: system },
    ...history.slice(-10).map((m) => ({ role: m.role, content: m.content })),
    { role: 'user', content: userMessage },
  ];

  if (process.env.OPENAI_API_KEY) {
    const text = await callOpenAI(messages);
    return { text, provider: 'openai' };
  }

  const ollama = process.env.OLLAMA_URL || process.env.OLLAMA_BASE_URL;
  if (ollama) {
    const text = await callOllama(messages);
    return { text, provider: 'ollama' };
  }

  return { text: localAiReply(userMessage, ctx), provider: 'local' };
}

async function executeAction(action, helpers) {
  const { runCli, parseTrips, tripsFile, email } = helpers;
  const userEmail = action.email || email;

  if (action.type === 'find_path') {
    const algo = (action.algorithm || 'dijkstra').toLowerCase();
    const source = action.source;
    const destination = action.destination;
    if (!source || !destination) {
      return { success: false, message: 'Missing source or destination' };
    }
    try {
      await runCli(['geocode', source, destination]);
      const data = await runCli(['path', algo, source, destination]);
      return { success: true, ...data, source, destination, algorithm: algo };
    } catch (e) {
      return { success: false, message: e.message };
    }
  }

  if (action.type === 'view_trips') {
    if (!userEmail) return { success: false, message: 'Login required' };
    try {
      const raw = fs.readFileSync(tripsFile, 'utf8');
      const trips = parseTrips(raw, userEmail);
      return { success: true, trips };
    } catch (e) {
      return { success: false, message: e.message, trips: [] };
    }
  }

  if (action.type === 'save_trip') {
    if (!userEmail) return { success: false, message: 'Login required' };
    const lr = helpers.lastRoute;
    if (!lr && (!action.source || !action.destination)) {
      return { success: false, message: 'No route to save' };
    }
    const source = action.source || lr.source;
    const destination = action.destination || lr.destination;
    const distance = lr?.distance ?? 0;
    const cost = lr?.cost ?? 0;
    const date = new Date().toLocaleDateString('en-GB').replace(/\//g, '-');
    try {
      await runCli(['addtrip', userEmail, source, destination, date, String(distance), String(cost)]);
      return { success: true, message: 'Trip saved', trip: { source, destination, date, distance, cost } };
    } catch (e) {
      return { success: false, message: e.message };
    }
  }

  return null;
}

module.exports = {
  loadEnvFile,
  getAiStatus,
  parseAction,
  generateReply,
  executeAction,
};
