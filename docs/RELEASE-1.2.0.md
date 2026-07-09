# HyBBX v1.2.0

Multi-transport session daemon: **telnet** (`:2323`), **SSH** (`:3232`), **WebSocket** (loopback `:4591`). Standalone clients: **hybbx-telnet**, **hybbx-ssh**, **hybbx-terminal**, **hybbx-telnet** (AmigaOS cross-build).

**Demo:** [README — hybbx.un1t.me](../README.md#live-hybbxun1tme).

## What's new

| Item | Detail |
|------|--------|
| `hybbx-ssh` | libssh client; encrypted transport; no host-key prompt |
| `hybbx-telnet` AmigaOS | bebbo-gcc / clib2 cross-build — **Verified** on A1200, AmigaOS 3.2 |
| Command history | Last 25 commands, up/down arrows — sessions, telnet, SSH, terminal clients |
| AX.25 auto-broadcast | 1200 baud UI beacons; max 48 chars; min 5 min interval; `@service@` token |
| WebSocket keepalive | Server ping; browser close diagnostics + auto-reconnect |

## Transports

| Transport | Detail |
|-----------|--------|
| Telnet | Primary user path `:2323` |
| SSH | libssh; Ed25519 keys in `hostkey_dir`; wire auth ≠ HyBBX accounts |
| WebSocket | RFC6455 forward-proxy; public TLS via reverse proxy — [WEBSOCKET.md](WEBSOCKET.md) |
| Packet radio | Multi-instance TNC edge; AX.25 broadcast — [TNCS.md](TNCS.md) |
| BayCom PR-Stack | Opt-in plugin (`HYBBX_PLUGIN_BAYCOM=OFF`) — [BAYCOM.md](BAYCOM.md) |
| ARDOP / CRDOP | External ARDOPC / CRDOPC |

## Clients

| Client | Detail |
|--------|--------|
| `hybbx-telnet` | Linux/POSIX; optional AmigaOS 3.9+ binary — [CLIENTS.md](CLIENTS.md) |
| `hybbx-ssh` | Linux/POSIX; libssh |
| `hybbx-terminal` | HBX circuit client |

## Status

| Area | Status |
|------|--------|
| Telnet, mail, chat, commands | **Verified** |
| WebSocket | **Verified** |
| `hybbx-telnet` AmigaOS | **Verified** — A1200, AmigaOS 3.2 |
| AX.25 auto-broadcast | **Built** |
| SSH transport + `hybbx-ssh` | **Built** |
| TCP `/broadcast` fan-out | **Partial** — stub |
| HBX, packet_radio, ARDOP, CRDOP | **Built** — not live RF verified |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME"
```

Clients only: `./scripts/build-clients.sh`. Amiga telnet: `./scripts/build-amiga-telnet.sh`. Options: [BUILD.md](BUILD.md).

## See also

[RELEASE-NOTES-1.2.0.txt](RELEASE-NOTES-1.2.0.txt) · [FEATURES.md](FEATURES.md) · [MANUAL.md](MANUAL.md) · [ROADMAP.md](ROADMAP.md)
