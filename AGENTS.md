# HyBBX — agent guide

Humans: [CONTRIBUTING.md](CONTRIBUTING.md). **v1.5.0** — [RELEASE-1.5.0.md](docs/RELEASE-1.5.0.md).

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
6. Doc changes: feature → FEATURES.md; INI → MANUAL.md + `share/*.ini.example`
7. **Commands:** user groups are Sysop, Admin, Mod, User, Guest only (no “Staff”). Registry: [share/commands.yaml](share/commands.yaml), layout: [docs/COMMANDS.md](docs/COMMANDS.md). Help: two lines (`/<verb> …` + `Help: …`); headers use `/help <cmd> for more`. `/broadcast` (alias `/announce`) = Sysop → all online users on **local Main** only.
8. **Proxy network** (`mains_proxy`): user services (`proxymail`, `proxychat`, future) only — no Sysop/Admin/Mod actions across proxy links.
9. **Version docs:** current release only (`HYBBX_VERSION_STRING`). No prior-version references. As-is software — compact text, no upgrade/history bloat. On bump: replace release docs; remove old `RELEASE-*.md` from tree.
10. **Documentation — Linux-based.** All docs in `docs/`, `text/`, README, and share examples assume Linux. Use `HTTPD_DOCROOT`, `systemctl`, `ss`. Do not name other OSes (BSD, macOS, Windows, …) or distro-specific package paths unless unavoidable in third-party license names.

## Doc map

| File | Use |
|------|-----|
| [RELEASE-1.5.0.md](docs/RELEASE-1.5.0.md) | Current release |
| [FEATURES.md](docs/FEATURES.md) | Shipped vs partial |
| [MANUAL.md](docs/MANUAL.md) | INI + commands |
| [COMMANDS.md](docs/COMMANDS.md) | Command registry, help layout |
| [commands.yaml](share/commands.yaml) | Command registry data |
| [SECURITY.md](docs/SECURITY.md) | Security + spam policy |
| [TNCS.md](docs/TNCS.md) | Supported TNC profiles |
| [BAYCOM.md](docs/BAYCOM.md) | BayCom PR-Stack plugin |
| [TOPOLOGY.md](docs/TOPOLOGY.md) | Main, Secondary, mains-proxy |
| [MAINS_PROXY.md](docs/MAINS_PROXY.md) | Proxy network INI |
| [WEBSOCKET.md](docs/WEBSOCKET.md) | WebSocket deploy |
| [ROADMAP.md](docs/ROADMAP.md) | Planned work |
| [BUILD.md](docs/BUILD.md) | CMake |

## Build

```bash
./scripts/dev-setup.sh
ctest --test-dir build   # -DHYBBX_BUILD_TESTS=ON
```

Do not commit unless the user asks. Git: `ngteq`, empty email.
