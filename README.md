# HyBBX

**HyBBX** is a C99 multi-transport session daemon for **bandwidth-constrained networks**. A unified session core (mail, chat, command interface) runs over heterogeneous link adapters: TCP telnet, AX.25 packet radio, and HBX/TCP circuit bridging.

**Main** and **Secondary** topology extends RF and limited-throughput TCP coverage: the Main hosts users, storage, and the HBX circuit hub; Secondary nodes attach local TNC/RF paths and bridge to Main over HBX v1 on TCP (default port 7323).

Output uses the configurable traffic profile (default **2400 baud**, 8N1 pacing, 80-column plain ASCII). One `hybbx` binary; instance role is defined in INI.

## Default deployment (datacenter Main)

HyBBX ships a **datacenter-oriented standard configuration** that remains **fully overrideable**.

**Main** — users, storage, mail, chat, telnet, and the HBX circuit hub (loopback by default; widen `bind` in `hybbx.ini` for remote Secondaries). TCP/IP on the host (`ax25=no`, `circuit=yes`).

**Secondary** — AX.25, TNC, and RF (and future adapters). Bridges to Main over HBX/TCP with `link_id` and `link_password`.

**Standalone Main** — set `ax25=yes` and local `[transport.packet_radio]` to run every connection type on one machine without remote HBX (lab or single-box).

Firewall, VPN, and tunnels are **system-level** — not built into HyBBX.

Config: [`share/hybbx.ini.example`](share/hybbx.ini.example) (Main) · [`share/hybbx-secondary.ini.example`](share/hybbx-secondary.ini.example) (Secondary) · **0.7.0**  
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
