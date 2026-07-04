# HyBBX
###### telnet un1t.me:2323 for auto-guest login full-serviced HyBBX.<br>
C99 **multi-transport session daemon** for bandwidth-constrained networks. One session core (mail, chat, `/` commands) over **plugin-only** link adapters (host-client bridges). Modems, TNCs, and sound-card software are **always external** — not part of HyBBX.

**v0.9.0** is the first **testing release** before **v1.0.0** — feature freeze; validation only until the public release.

**Default architecture:** telnet plugin, `packet_radio` → serial TNC, **`ardop`** → ARDOP (ARDOPC/ardopcf), **`crdop`** → CRDOP (CRDOPC). Both use the same host-TCP bridge model; modem DSP stays external.

**Main** hosts users, storage, telnet :2323, HBX hub :7323. **Secondary** nodes attach local TNC/RF and bridge to Main. Same `hybbx` binary; role from INI. Default traffic: 2400 baud pacing, 80-column ASCII.

| Role | Default | Purpose |
|------|---------|---------|
| Main | `ax25=no`, `circuit=yes` | Central services + HBX hub |
| Secondary | `ax25=yes`, `circuit=no` | RF edge → `circuit_host` |
| Standalone Main | `ax25=yes` locally | Single-box; no Secondary |

Config: [`share/hybbx.ini.example`](share/hybbx.ini.example) · [`share/hybbx-secondary.ini.example`](share/hybbx-secondary.ini.example) · **0.9.0** (testing release before v1.0.0)

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
```

Clients: `build/src/clients/hybbx-telnet`, `hybbx-terminal` — [docs/CLIENTS.md](docs/CLIENTS.md)

## Docs

| Doc | Contents |
|-----|----------|
| [FEATURES.md](docs/FEATURES.md) | Implemented vs planned |
| [MANUAL.md](docs/MANUAL.md) | INI, transports, commands |
| [ARDOP.md](docs/ARDOP.md) | ARDOP plugin + ARDOPC/ardopcf |
| [CRDOP.md](docs/CRDOP.md) | CRDOP plugin + CRDOPC |
| [QUICKSTART.md](docs/QUICKSTART.md) | First run |
| [ROADMAP.md](docs/ROADMAP.md) | Topology and release plan |
| [INDEX.md](docs/INDEX.md) | Full index |

GPL-3.0 — [LICENSE.txt](LICENSE.txt)
