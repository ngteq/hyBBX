# HyBBX v1.1.0

Multi-transport session daemon: **telnet** (`:2323`), **SSH** (`:3232`), **WebSocket** (loopback `:4591`). Shared session core — `[auth]`, mail, chat, `/` commands.

**Demo:** [README — hybbx.un1t.me](../README.md#live-hybbxun1tme).

## Transports

| Transport | Detail |
|-----------|--------|
| Telnet | Primary user path `:2323` |
| SSH | libssh; Ed25519 keys in `hostkey_dir`; wire auth ≠ HyBBX accounts |
| WebSocket | RFC6455 forward-proxy; public TLS via reverse proxy — [WEBSOCKET.md](WEBSOCKET.md) |
| Packet radio | Multi-instance TNC edge — [TNCS.md](TNCS.md) |
| BayCom PR-Stack | Opt-in plugin (`HYBBX_PLUGIN_BAYCOM=OFF`) — [BAYCOM.md](BAYCOM.md) |
| ARDOP / CRDOP | External ARDOPC / CRDOPC |

## RF / edge

| Item | Detail |
|------|--------|
| `packet_radio` | Up to 8 TNCs; `kiss_entry` / `kiss_exit`; TNC2C 7E1; profiles in [TNCS.md](TNCS.md) |
| `baycom` | Linux kernel SER12/PAR96 or serial KISS; not built by default |
| HBX circuit | Secondary → Main AX.25 bridge |

## Session

| Item | Detail |
|------|--------|
| Line input (SSH) | ANSI filter; arrows, Home/End, backspace/delete |
| WebSocket TX | UTF-8-safe buffering |
| `max_connections` | WebSocket slot limit (default `10`) |
| One session per account | `/login` rejected if already online |

## Status

| Area | Status |
|------|--------|
| Telnet, mail, chat, commands | **Verified** |
| SSH, WebSocket | **Built** — verify in your deployment |
| HBX, packet_radio, ARDOP, CRDOP | **Built** — not live RF verified |
| BayCom PR-Stack | **Built** (opt-in compile) |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME"
```

Full-featured (incl. baycom): `-DHYBBX_PLUGIN_BAYCOM=ON`. **libssh** required for SSH. AmigaOS 3.9+ cross-build: [BUILD.md](BUILD.md). Options: [BUILD.md](BUILD.md).

## Install layout

`<prefix>/hybbx/` — `hybbx`, `hybbx-start`, `hybbx.ini`, `keys/`, `data/`, `text/`, `logs/`, `lib/`.

Operator documentation assumes **Linux** — `HTTPD_DOCROOT`, `systemctl`, `ss`. [WEBSOCKET.md](WEBSOCKET.md).

## See also

[RELEASE-NOTES-1.1.0.txt](RELEASE-NOTES-1.1.0.txt) · [FEATURES.md](FEATURES.md) · [MANUAL.md](MANUAL.md) · [ROADMAP.md](ROADMAP.md)
