# HyBBX — agent guide

Humans: [CONTRIBUTING.md](CONTRIBUTING.md). **v2.0.0**.

## Product

Plugin-only session daemon. **Main** = users + telnet + HBX hub. **Secondary** = remote RF host (HBX client to Main). No modem DSP inside `hybbx`.

**Text-first:** users work in plain-text terminals (`/` commands, mail, chat, line output). Core and operator docs assume text sessions on low-bandwidth links. **Extensible:** plugins and transports may add ANSI, menus, or client graphics (WebSocket/httpd) without moving wire or game logic into `src/core/`. **Portable:** POSIX+ C99 shared code — Linux, *BSD, AmigaOS 3.9+, MacOS, Windows ([PLATFORMS.md](docs/PLATFORMS.md)).

| In tree | External |
|---------|----------|
| `src/core/`, `plugins/telnet` | TNC, KISS, sound-card apps |
| `plugins/ssh`, `plugins/websocket` | libssh; httpd for browser UI |
| Built-in `[security]` (network + abuse) | Optional external fail2ban filters in `share/fail2ban/` |
| `plugins/packet_radio` | Serial/USB TNC — RF prep: **MainAX25-Stack (MAX25)** `hardware/tncs` |
| `plugins/baycom` | BayCom modem path — prep: MAX25 `hardware/modems` (replaces standalone BayCom PR-Stack) |
| `plugins/ardop`, `plugins/crdop` | ARDOPC; CRDOPC — soft-modem prep: MAX25 `hardware/soft-modems` (replaces standalone CRDOP tree) |
| `plugins/entertain_*` (e.g. chess) | Entertain Area — games/apps on Main only |

MAX25 owns TNC boot-wait, BayCom kernel lifecycle, and CRDOPC start; HyBBX plugins open serial/TCP **after** prep. Contract: MainAX25-Stack (MAX25) `docs/HYBBX.md`.

## Rules

1. Core = sessions + HBX/TCP only — no KISS/AX.25/telnet wire parsing in `src/core/`
2. **Inter-node transport:** Secondary→Main, Main↔Main (`mains_proxy`), and proxy relay (`proxymail`/`proxychat`) MUST use HBX/Circuit (`hybbx_circuit_link_connect` + `LINK_AUTH`) — never raw TCP/AX.25 sockets between HyBBX processes. User sessions (telnet/SSH/WebSocket) are separate. Proxy carries user services only — no admin actions.
3. Plugins: `hybbx_transport_plugin_t` in `plugins/`; **Entertain Area apps are plugins only** — see [ENTERTAIN.md](docs/ENTERTAIN.md)
4. Booleans: `hybbx_parse_bool()` — `yes`/`no`
5. Buffers: [limits.h](include/hybbx/limits.h)
6. **Portability:** POSIX+ friendly — C99 and portable POSIX in shared code; keep `*BSD` (FreeBSD, NetBSD, OpenBSD, …) and **AmigaOS 3.9+** compatible; isolate platform code (`_WIN32`, `__AMIGA__`, …). New core changes should stay easy to port.
7. **Logging:** `[log] level` = `debug` \| `stats` \| `info` \| `warn` (default). `hybbx_log_*` writes console + daily file when enabled. See [MANUAL.md](docs/MANUAL.md) `[log]`.
8. **Docs — technical only.** Operator/INI → [MANUAL.md](docs/MANUAL.md) + `share/*.ini.example`. Commands → `share/areas.yaml` + [commands.yaml](share/commands.yaml) + [COMMANDS.md](docs/COMMANDS.md). Build → [BUILD.md](docs/BUILD.md). No planning, roadmap, or feature-status docs.
9. **Commands:** user groups are Sysop, Admin, Mod, User, Guest only (no “Staff”). Help: two lines (`/<verb> …` + `Help: …`); headers use `/help <cmd> for more`. Output: one blank line before `/command` reply, none after (not chat/mail compose). `/broadcast` (alias `/announce`) = Sysop → all online users on **local Main** only.
10. **Proxy network** (`mains_proxy`): user services (`proxymail`, `proxychat`) only — no Sysop/Admin/Mod actions across proxy links.
11. **Version:** current `HYBBX_VERSION_STRING` only in code and brief doc headers. As-is software — compact text, no upgrade/history bloat.
12. **Documentation — Linux-based.** Operator docs in `docs/`, `text/`, README, and share examples assume Linux (`HTTPD_DOCROOT`, `systemctl`, `ss`). Platform targets belong in [PLATFORMS.md](docs/PLATFORMS.md) only — each OS in its own section (Linux, AmigaOS 3.9+, *BSD [FreeBSD, NetBSD, OpenBSD, …], **MacOS X+**, **Windows 10+**; spell **MacOS**, not macOS). Do not mix MacOS into *BSD rubrics elsewhere.
13. **Git** — no commit/push unless the user asks. **Only** `ngteq <info@un1t.me>` for author/committer (`.git/config`). Push **only** via `~/.ssh/id_ed25519_ngteq` → `git@github.com:ngteq/hyBBX.git`.

## Doc map

| File | Use |
|------|-----|
| [MANUAL.md](docs/MANUAL.md) | INI + operator |
| [COMMANDS.md](docs/COMMANDS.md) | Command registry, help layout |
| [commands.yaml](share/commands.yaml) | Command registry (help, aliases, access) |
| [areas.yaml](share/areas.yaml) | Menu/index area layout |
| [BUILD.md](docs/BUILD.md) | CMake |
| [DEVELOPMENT.md](docs/DEVELOPMENT.md) | Code rules |
| [TOPOLOGY.md](docs/TOPOLOGY.md) | Main, Secondary, mains-proxy; **HBX** (Hybrid Bridge eXchange v1) |
| [SECURITY.md](docs/SECURITY.md) | Security + spam policy |
| [WEBSOCKET.md](docs/WEBSOCKET.md) | WebSocket deploy |
| [MAINS_PROXY.md](docs/MAINS_PROXY.md) | Proxy network INI |
| [TNCS.md](docs/TNCS.md) | Packet radio TNC profiles |
| [BAYCOM.md](docs/BAYCOM.md) | BayCom PR-Stack |
| [ARDOP.md](docs/ARDOP.md) · [CRDOP.md](docs/CRDOP.md) | Modem plugins |
| [CLIENTS.md](docs/CLIENTS.md) | CLI clients |
| [PLATFORMS.md](docs/PLATFORMS.md) | Per-OS build notes (MacOS, Windows, …) |
| [ENTERTAIN.md](docs/ENTERTAIN.md) | Entertain Area — plugin-only apps (chess, …) |

## Build

```bash
./scripts/dev-setup.sh
ctest --test-dir build   # -DHYBBX_BUILD_TESTS=ON
```
