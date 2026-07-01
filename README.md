# HyBBX

C99 plugin transport service with text-only BBS sessions and personal mail.

**Main** — central instance: users, storage, mailbox, chat, telnet, and the HBX circuit hub (TCP).

**Secondary** — instance with TNC, modem, or radio hardware; bridges that link to Main over **TCP/IP** (HBX).

Same `hybbx` binary; role is set by INI. HyBBX uses plain TCP only — VPN or tunnels are external.

Config: [`share/hybbx.ini.example`](share/hybbx.ini.example) (Main) · [`share/hybbx-secondary.ini.example`](share/hybbx-secondary.ini.example) (Secondary) · **0.5.0**  
Arch: [`docs/ROADMAP.md`](docs/ROADMAP.md)

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
| [ROADMAP.md](docs/ROADMAP.md) | Main + Secondary layout |
| [PLATFORMS.md](docs/PLATFORMS.md) | GCC/Clang targets |
| [INDEX.md](docs/INDEX.md) | Full index |

GPL-3.0 — [LICENSE.txt](LICENSE.txt)
