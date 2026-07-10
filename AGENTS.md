# HyBBX — agent guide

Humans: [CONTRIBUTING.md](CONTRIBUTING.md). **v2.0.0 (upcoming)**. This file is the primary handoff for AI-assisted forks and continued development.

## Product

Plugin-only session daemon. **Main** = users + telnet + HBX hub. **Secondary** = remote edge (HBX client to Main). No modem DSP inside `hybbx`.

| In tree | External |
|---------|----------|
| `src/core/`, `plugins/telnet` | TNC, KISS, sound-card apps |
| `plugins/ssh`, `plugins/websocket` | libssh; httpd for browser UI |
| Built-in `[security]` (network + abuse) | Optional external fail2ban filters in `share/fail2ban/` |
| `plugins/packet_radio` | Serial/USB TNC |
| `plugins/baycom` | BayCom PR-Stack (kernel SER12/PAR96) |
| `plugins/ardop`, `plugins/crdop` | ARDOPC, CRDOPC |

## Rules

1. Core = sessions + HBX/TCP only — no KISS/AX.25/telnet wire parsing in `src/core/`
2. **Inter-node transport:** Secondary→Main, Main↔Main (`mains_proxy`), and proxy relay (`proxymail`/`proxychat`) MUST use HBX/Circuit (`hybbx_circuit_link_connect` + `LINK_AUTH`) — never raw TCP/AX.25 sockets between HyBBX processes. User sessions (telnet/SSH/WebSocket) are separate. Proxy carries user services only — no admin actions.
3. Plugins: `hybbx_transport_plugin_t` in `plugins/`
4. Booleans: `hybbx_parse_bool()` — `yes`/`no`
5. Buffers: [limits.h](include/hybbx/limits.h)
6. **Docs — technical only.** Operator/INI → [MANUAL.md](docs/MANUAL.md) + `share/*.ini.example`. Commands → [commands.yaml](share/commands.yaml) + [COMMANDS.md](docs/COMMANDS.md). Build → [BUILD.md](docs/BUILD.md). No planning, roadmap, or feature-status docs.
7. **Commands:** user groups are Sysop, Admin, Mod, User, Guest only (no “Staff”). Help: two lines (`/<verb> …` + `Help: …`); headers use `/help <cmd> for more`. Output: one blank line before `/command` reply, none after (not chat/mail compose). `/broadcast` (alias `/announce`) = Sysop → all online users on **local Main** only.
8. **Proxy network** (`mains_proxy`): user services (`proxymail`, `proxychat`, future) only — no Sysop/Admin/Mod actions across proxy links.
9. **Version:** current `HYBBX_VERSION_STRING` only in code and brief doc headers. As-is software — compact text, no upgrade/history bloat.
10. **Documentation — Linux-based.** All docs in `docs/`, `text/`, README, and share examples assume Linux. Use `HTTPD_DOCROOT`, `systemctl`, `ss`. Do not name other OSes (BSD, macOS, Windows, …) or distro-specific package paths unless unavoidable in third-party license names.
11. **Fork-friendly.** No personal or custodial prose in docs. GPL-3.0 — preserve LICENSE.txt. New maintainers: CONTRIBUTING.md → this file → DEVELOPMENT.md → TOPOLOGY.md.

## Doc map

| File | Use |
|------|-----|
| [MANUAL.md](docs/MANUAL.md) | INI + operator |
| [COMMANDS.md](docs/COMMANDS.md) | Command registry, help layout |
| [commands.yaml](share/commands.yaml) | Command registry data |
| [BUILD.md](docs/BUILD.md) | CMake |
| [DEVELOPMENT.md](docs/DEVELOPMENT.md) | Code rules |
| [TOPOLOGY.md](docs/TOPOLOGY.md) | Main, Secondary, mains-proxy |
| [SECURITY.md](docs/SECURITY.md) | Security + spam policy |
| [WEBSOCKET.md](docs/WEBSOCKET.md) | WebSocket deploy |
| [MAINS_PROXY.md](docs/MAINS_PROXY.md) | Proxy network INI |
| [TNCS.md](docs/TNCS.md) | Packet radio TNC profiles |
| [BAYCOM.md](docs/BAYCOM.md) | BayCom PR-Stack |
| [ARDOP.md](docs/ARDOP.md) · [CRDOP.md](docs/CRDOP.md) | Modem plugins |
| [CLIENTS.md](docs/CLIENTS.md) | CLI clients |
| [PLATFORMS.md](docs/PLATFORMS.md) | Linux build deps |

## Build

```bash
./scripts/dev-setup.sh
ctest --test-dir build   # -DHYBBX_BUILD_TESTS=ON
```

Do not commit unless the maintainer asks.
