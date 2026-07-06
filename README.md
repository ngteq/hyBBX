# HyBBX

C99 **session daemon** for low-bandwidth links: mail, chat, and `/` commands over line-oriented transports.

**v1.0.2** — telnet, SSH, and WebSocket. [docs/RELEASE-1.0.2.md](docs/RELEASE-1.0.2.md).

## Live (un1t.me)

| Access | How |
|--------|-----|
| **Browser** | [https://un1t.me/hybbx-websocket/](https://un1t.me/hybbx-websocket/) |
| **Telnet** | `telnet un1t.me 2323` — guest auto-login |
| **SSH** | `ssh un1t.me -p 3232` — wire username/password only; guest or `/login` per INI |

## What it is

| Piece | Role |
|-------|------|
| **Main** | Users, storage, telnet `:2323`, SSH `:3232`, HBX hub `:7323` |
| **Secondary** | Remote RF edge → HBX client to Main |
| **Plugins** | Telnet, SSH, WebSocket, packet radio, ARDOP, CRDOP — modems/TNCs stay **external** |

Templates: [share/hybbx.ini.example](share/hybbx.ini.example), [share/hybbx-secondary.ini.example](share/hybbx-secondary.ini.example).

## Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

[docs/QUICKSTART.md](docs/QUICKSTART.md).

## Documentation

| Audience | Start here |
|----------|------------|
| Operator | [docs/QUICKSTART.md](docs/QUICKSTART.md) → [docs/MANUAL.md](docs/MANUAL.md) |
| WebSocket | [docs/WEBSOCKET.md](docs/WEBSOCKET.md) |
| Features | [docs/FEATURES.md](docs/FEATURES.md) |
| Developer | [docs/BUILD.md](docs/BUILD.md) → [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) |
| Index | [docs/INDEX.md](docs/INDEX.md) |

GPL-3.0 — [LICENSE.txt](LICENSE.txt)
