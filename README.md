# SmartRouteX

Connected travel system: **login first**, then route planning with C++ backend and file storage.

## File formats (from program)

| File | Format |
|------|--------|
| `users.txt` | `email\|password` per line |
| `trips.txt` | `email\|source\|destination\|date\|distance\|cost` |
| `coords.txt` | `name lat lon` (space-separated) |
| `coords.json` | `{"src":[lat,lon],"dest":[lat,lon]}` |
| `temp.json` | Nominatim API response (geocoding cache) |

## Setup

```bash
npm install
npm run build:cpp
npm start
```

Open **http://localhost:3000** — login page appears first.

### Test accounts (users.txt)

- `m@gmail.com` / `1234`
- `tiya@gmail.com` / `2345`

## Architecture

```
app.html  →  Node server (backend/server.js)  →  route_cli.exe (pathfinder.cpp)
                    ↓                                    ↓
              users.txt, trips.txt              coords.txt, coords.json, temp.json
```

## API

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/login` | `{email, password}` |
| POST | `/api/signup` | `{email, password}` |
| GET | `/api/trips?email=` | List user trips |
| POST | `/api/trips` | Save trip |
| POST | `/api/geocode` | Geocode → `coords.json` |
| POST | `/api/path` | `{source, destination, algorithm}` — dfs/bfs/dijkstra |

## Console mode

Run without arguments for the original terminal UI:

```bash
route_cli.exe
```
