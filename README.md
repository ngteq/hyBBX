# HyBBX

**HyBBX** is a text-only, BBS-like **C99 plugin transport service** — personal mail, chat and more on one shared session core. Link adapters join **different data networks** (TCP telnet, AX.25 packet radio, HBX over TCP, and planned stacks) so callers on any link reach the same HyBBX system.

One `hybbx` binary; **Main** and **Secondary** roles are chosen in INI.

## Default deployment (datacenter Main)

HyBBX ships a **datacenter-oriented standard configuration** that remains **fully overrideable**.

**Main** — users, storage, mail, chat, telnet, and the HBX circuit hub. TCP/IP on the host by default (`ax25=no`, `circuit=yes`).

**Secondary** — AX.25, TNC, and RF (and future adapters). Bridges to Main over HBX/TCP (port 7323) with `link_id` and `link_password`.

**Standalone Main** — set `ax25=yes` and local `[transport.packet_radio]` to run every connection type on one machine without remote HBX (lab or single-box).

Firewall, VPN, and tunnels are **system-level** — not built into HyBBX.

Config: [`share/hybbx.ini.example`](share/hybbx.ini.example) (Main) · [`share/hybbx-secondary.ini.example`](share/hybbx-secondary.ini.example) (Secondary) · **0.6.0**  
Architecture: [`docs/ROADMAP.md`](docs/ROADMAP.md)

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
```

Clients: `build/src/clients/hybbx-telnet`, `hybbx-terminal` — [docs/CLIENTS.md](docs/CLIENTS.md)

## Docs

| Doc | |
|-----|---|
| [FEATURES.md](docs/FEATURES.md)     | Status                    |
| [MANUAL.md](docs/MANUAL.md)         | INI, transports, commands |
| [QUICKSTART.md](docs/QUICKSTART.md) | First run                 |
| [ROADMAP.md](docs/ROADMAP.md)       | Main + Secondary layout   |
| [PLATFORMS.md](docs/PLATFORMS.md)   | GCC/Clang targets         |
| [INDEX.md](docs/INDEX.md)           | Full index                |


GPL-3.0 — [LICENSE.txt](LICENSE.txt)