# HyBBX v1.0.1

Adds the **SSH transport** (`[networks] ssh=yes`, `[transport.ssh]`, port **3232**) using **libssh** (LGPL, dynamic link).

## What changed

| Area | Status |
|------|--------|
| SSH transport plugin | **Built** — libssh server; dual-stack bind |
| Session auth over SSH | Same as telnet — `[auth]` in `hybbx.ini` only |
| Host keys | Auto-generated Ed25519 under `hostkey_dir` (default `keys/`) |
| fail2ban | `hybbx-ssh` filter/jail examples (port 3232) |
| WebSocket transport | **Built** — RFC6455 forward-proxy on `:4591` |
| PHP / proxy examples | `share/web/`, `share/nginx/`, `share/apache2/`, `share/lighttpd/` |

SSH username/password complete the wire handshake only; HyBBX guest auto-login or `/login` follows INI settings.

WebSocket is forward-proxy only (no HyBBX auth on the wire); use TLS at the reverse proxy. See [WEBSOCKET.md](WEBSOCKET.md).

## Build

Requires **libssh** (pkg-config) for SSH. CMake: `HYBBX_PLUGIN_SSH=ON`, `HYBBX_PLUGIN_WEBSOCKET=ON` (defaults).

See [MANUAL.md](MANUAL.md#transportssh-requires-libssh-at-build-time) and `share/THIRD_PARTY_NOTICES.txt`.

## Unchanged from v1.0.0

Telnet (`:2323`) remains the primary verified user path. v1.0.0 scope and verification: [RELEASE-1.0.0.md](RELEASE-1.0.0.md).
