# HyBBX

Plugin-based C99 transport service: **centralized `hybbx` daemon**, **link/repeater edge daemons**, text-only mailbox/BBS-like sessions.

Config: [`share/hybbx.ini.example`](share/hybbx.ini.example) · Arch: [`docs/ROADMAP.md`](docs/ROADMAP.md) · **0.5.0** (stability line: [docs/RELEASE-0.5.0.md](docs/RELEASE-0.5.0.md))

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
```

Clients: `build/src/clients/hybbx-telnet`, `hybbx-terminal` ([docs/CLIENTS.md](docs/CLIENTS.md))

## Docs

| Doc | |
|-----|---|
| [FEATURES.md](docs/FEATURES.md) | Status |
| [MANUAL.md](docs/MANUAL.md) | INI, transports, commands |
| [QUICKSTART.md](docs/QUICKSTART.md) | First run |
| [ROADMAP.md](docs/ROADMAP.md) | Central + edge daemon layout |
| [PLATFORMS.md](docs/PLATFORMS.md) | GCC/Clang targets |
| [INDEX.md](docs/INDEX.md) | Full index |

GPL-3.0 — [LICENSE.txt](LICENSE.txt)
