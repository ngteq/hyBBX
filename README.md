# HyBBX

C99 plugin transport service with text-only BBS-like sessions and personal mail.

## Default deployment (datacenter Main)

HyBBX ships a **datacenter-oriented standard configuration** that is **fully overrideable** in INI.

**Main** holds users, storage, mail, chat, and the circuit hub (TCP/IP + HBX by default; `ax25=no`). **AX.25 and other non-core connection types are off on Main by default** — they run on one or more **Secondary** instances that attach over TCP/IP (port 7323).

**Standalone Main:** set `ax25=yes`, add `[transport.packet_radio]`, and connect TNC hardware locally. Main then manages every connection type itself with **no remote HBX** — useful for labs or single-box sites.

Same `hybbx` binary; role is set by INI. Link auth: `link_id` + `link_password` on Main bridge sections (`[transport.packet_radio1]`, …) and on each Secondary. Firewall, VPN, and tunnels are **system-level** — not in HyBBX.

Config: `[share/hybbx.ini.example](share/hybbx.ini.example)` (Main) · `[share/hybbx-secondary.ini.example](share/hybbx-secondary.ini.example)` (Secondary) · **0.5.0**  
Arch: `[docs/ROADMAP.md](docs/ROADMAP.md)`

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
```

Clients: `build/src/clients/hybbx-telnet`, `hybbx-terminal` ([docs/CLIENTS.md](docs/CLIENTS.md))

## Docs


| Doc                                 |                           |
| ----------------------------------- | ------------------------- |
| [FEATURES.md](docs/FEATURES.md)     | Status                    |
| [MANUAL.md](docs/MANUAL.md)         | INI, transports, commands |
| [QUICKSTART.md](docs/QUICKSTART.md) | First run                 |
| [ROADMAP.md](docs/ROADMAP.md)       | Main + Secondary layout   |
| [PLATFORMS.md](docs/PLATFORMS.md)   | GCC/Clang targets         |
| [INDEX.md](docs/INDEX.md)           | Full index                |


GPL-3.0 — [LICENSE.txt](LICENSE.txt)