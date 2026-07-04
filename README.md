# HyBBX

C99 **session daemon** for low-bandwidth links: mail, chat, and `/` commands over line-oriented transports.

**v1.0.0** — first official release. Scope: **telnet sessions** on Main (tested in operation). AX.25 and RF bridges are built in but **not yet verified in live service** — local tests only. Details: [docs/RELEASE-1.0.0.md](docs/RELEASE-1.0.0.md).

Live demo: `telnet un1t.me 2323` (guest auto-login).

## What it is

| Piece | Role |
|-------|------|
| **Main** | Users, storage, telnet `:2323`, HBX hub `:7323` |
| **Secondary** | Remote RF edge → HBX client to Main |
| **Plugins** | Telnet (default), packet radio, ARDOP, CRDOP — modems/TNCs stay **external** |

Same `hybbx` binary; role from INI. Templates: [`share/hybbx.ini.example`](share/hybbx.ini.example), [`share/hybbx-secondary.ini.example`](share/hybbx-secondary.ini.example).

## Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

Full steps: [docs/QUICKSTART.md](docs/QUICKSTART.md).

## Documentation

| Audience | Start here |
|----------|------------|
| Operator / admin | [docs/QUICKSTART.md](docs/QUICKSTART.md) → [docs/MANUAL.md](docs/MANUAL.md) |
| Feature status | [docs/FEATURES.md](docs/FEATURES.md) |
| Developer | [docs/BUILD.md](docs/BUILD.md) → [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) |
| Index | [docs/INDEX.md](docs/INDEX.md) |

GPL-3.0 — [LICENSE.txt](LICENSE.txt)
