# HyBBX v1.0.2

Multi-transport session daemon: **telnet** (`:2323`), **SSH** (`:3232`), **WebSocket** (loopback `:4591`). Shared session core — `[auth]`, mail, chat, `/` commands.

**Demo:** [README — un1t.me](../README.md#live-un1tme).

## Transports

| Transport | Detail |
|-----------|--------|
| Telnet | Primary user path `:2323` |
| SSH | libssh; Ed25519 keys in `hostkey_dir`; wire auth ≠ HyBBX accounts |
| WebSocket | RFC6455 forward-proxy; public TLS via reverse proxy — [WEBSOCKET.md](WEBSOCKET.md) |
| Browser UI | PHP/JS in httpd docroot (not served by `hybbx`) |

## Session

| Item | Detail |
|------|--------|
| Line input (SSH) | ANSI filter; arrows, Home/End, backspace/delete |
| WebSocket TX | UTF-8-safe buffering |
| `max_connections` | WebSocket slot limit (default `10`) |
| One session per account | `/login` rejected if already online |
| fail2ban | `hybbx-ssh` examples (port 3232) |

## Status

| Area | Status |
|------|--------|
| Telnet, mail, chat, commands | **Verified** |
| SSH, WebSocket | **Built** — verify in your deployment |
| HBX, packet_radio, ARDOP, CRDOP | **Built** — not live RF verified |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME"
```

**libssh** required for SSH (`HYBBX_PLUGIN_SSH=ON`, default). Options: [BUILD.md](BUILD.md). LGPL notice: [share/THIRD_PARTY_NOTICES.txt](../share/THIRD_PARTY_NOTICES.txt).

## Install layout

`<prefix>/hybbx/` — `hybbx`, `hybbx-start`, `hybbx.ini`, `keys/`, `reverse-proxy/`, `data/`, `text/`, `logs/`, `lib/`.

Documentation assumes **Linux** — `HTTPD_DOCROOT`, `systemctl`, `ss`. [WEBSOCKET.md](WEBSOCKET.md).

## See also

[RELEASE-NOTES-1.0.2.txt](RELEASE-NOTES-1.0.2.txt) · [FEATURES.md](FEATURES.md) · [MANUAL.md](MANUAL.md) · [ROADMAP.md](ROADMAP.md)
