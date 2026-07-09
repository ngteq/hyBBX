# Documentation index

[README.md](../README.md) · **v1.5.0** (testing) [RELEASE-1.5.0.md](RELEASE-1.5.0.md)

## Operators

| Doc | Contents |
|-----|----------|
| [QUICKSTART.md](QUICKSTART.md) | Build, run, first login |
| [TOPOLOGY.md](TOPOLOGY.md) | Main, Secondary, mains-proxy, HBX |
| [MANUAL.md](MANUAL.md) | INI (`[security]`, storage, commands) |
| [WEBSOCKET.md](WEBSOCKET.md) | WebSocket proxy + browser UI |
| [CLIENTS.md](CLIENTS.md) | `hybbx-telnet`, `hybbx-ssh`, `hybbx-terminal` |
| [FEATURES.md](FEATURES.md) | Shipped vs partial |

## Transports

| Doc | Contents |
|-----|----------|
| [MAINS_PROXY.md](MAINS_PROXY.md) | Main-to-Main mesh (stub) |
| [TNCS.md](TNCS.md) | Packet radio TNC profiles |
| [BAYCOM.md](BAYCOM.md) | BayCom PR-Stack |
| [ARDOP.md](ARDOP.md) | ARDOP + ARDOPC |
| [CRDOP.md](CRDOP.md) | CRDOP + CRDOPC |

## Developers

| Doc | Contents |
|-----|----------|
| [BUILD.md](BUILD.md) | CMake options |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Code rules, layout |
| [REPOSITORY.md](REPOSITORY.md) | Source tree |
| [PLATFORMS.md](PLATFORMS.md) | Linux build requirements |

## Other

| Doc | Contents |
|-----|----------|
| [ROADMAP.md](ROADMAP.md) | Planned work |
| [LICENSING.md](LICENSING.md) | GPL + third-party |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | PR flow |
| [AGENTS.md](../AGENTS.md) | AI agent rules |

## Config

| File | Role |
|------|------|
| [share/hybbx.ini.example](../share/hybbx.ini.example) | Main |
| [share/hybbx-secondary.ini.example](../share/hybbx-secondary.ini.example) | Secondary |
| [share/fail2ban/](../share/fail2ban/) | Optional external fail2ban filters |

INI comments are short; full keys → [MANUAL.md](MANUAL.md).
