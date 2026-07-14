# Operator manual · HyBBX 2.4.0

Telnet, SSH, WebSocket operator reference. INI templates in `share/` — copy to `./local/hybbx.ini`.

## Template matrix

| Template | Use |
|----------|-----|
| `hybbx-standalone.ini.example` | Main + local TNC (one host) |
| `hybbx-main.ini.example` | Main; RF on remote Secondary |
| `hybbx-mesh.ini.example` | Main + `mains_proxy` mesh |
| `hybbx-secondary.ini.example` | Secondary RF host |

## Topology matrix

| Role | `[networks]` typical | Hosts |
|------|----------------------|-------|
| **Main** | `circuit=yes`, `ax25=no` | Users, HBX hub |
| **Secondary** | `circuit=no`, `ax25=yes` | TNC/RF; `circuit_host` → Main |
| **Standalone Main** | `ax25=yes` on same box | Lab / single-host |

## Core INI sections matrix

| Section | Key keys |
|---------|----------|
| `[service]` | `name`, `max_online`, `prompt` |
| `[storage]` | `backend=flatfile\|sqlite` |
| `[networks]` | `ax25`, `baycom`, `crdop`, `circuit`, `ssh`, `websocket` |
| `[transport.telnet]` | `bind`, `port=2323` |
| `[transport.circuit]` | `port=7323`, `max_links` |
| `[transport.packet_radioN]` | `device`, `tnc`, `protocol=kiss`, `kiss_entry=none` |
| `[broadcast]` | `ax25_mycall`, `ax25_auto_message`, `ax25_dest` |
| `[max25]` | `check=yes`, `host`, `port=7325` |
| `[security]` | `bantime`, `maxretry`, `ban_backend` |

## v2.4.0 RF keys matrix

| Key | Value | Notes |
|-----|-------|-------|
| `kiss_entry` | `none` | MAX25 owns KISS entry |
| `kiss_exit` | `none` | No `kiss off` on shutdown |
| `[max25] check` | `yes` | Local TNC start fails if max25d down |
| `tnc=baycom\|pccom` | rejected | Use MAX25 + `baycom` plugin instead |

## Daemon matrix

| Item | Value |
|------|-------|
| Binary | `hybbxd` |
| Start | `hybbx-start` or `hybbxd -c hybbx.ini` |
| Detached | `hybbx-start --screen` / `--tmux` |
| First start | Sysop created with random password on console |

## Related

| Goal | Doc |
|------|-----|
| Topology | [TOPOLOGY.md](TOPOLOGY.md) |
| Security | [SECURITY.md](SECURITY.md) |
| Build | [BUILD.md](BUILD.md) |
