# Standalone clients

No INI — flags and environment only. Sources: `src/clients/`.

Build: `-DHYBBX_CLIENTS_ONLY=ON` or `./scripts/build-clients.sh`.

## hybbx-telnet

```bash
hybbx-telnet -H 127.0.0.1 -p 2323
hybbx-telnet -u user -P pass --baud 2400
```

Env: `HYBBX_HOST`, `HYBBX_PORT`

### AmigaOS 3.9+

Cross-build with `./scripts/build-amiga-telnet.sh` (bebbo-gcc at
`/opt/amiga`). NDK trees under `NDKs/32` and `NDKs/39` are kit snapshots, not
per-OS NDKs — see [PLATFORMS.md](PLATFORMS.md#amigaos-39).

## hybbx-ssh

Encrypted transport to HyBBX `[transport.ssh]` (default port 3232). SSH wire
username/password satisfy the libssh handshake only — HyBBX login is guest
auto-login or `/login` like telnet. Host keys are not verified (no
`known_hosts` prompt).

```bash
hybbx-ssh -H 127.0.0.1 -p 3232
hybbx-ssh -u user -P pass --baud 2400
```

Env: `HYBBX_HOST`, `HYBBX_PORT`, `HYBBX_SSH_WIRE_USER`, `HYBBX_SSH_WIRE_PASS`

Requires libssh at build time (`HYBBX_BUILD_CLIENT_SSH=ON`, default when
libssh is installed).

## hybbx-terminal

HBX circuit client (AX.25 UI optional).

```bash
hybbx-terminal -H 127.0.0.1 -p 7323
hybbx-terminal --link-id ID --link-password SECRET
hybbx-terminal --mycall CALL --dest CALL --ax25-ui
```

Env: `HYBBX_CIRCUIT_HOST`, `HYBBX_CIRCUIT_PORT`

| Direction | Mode |
|-----------|------|
| TX | `--ax25-ui` off → HBX TERMINAL; on → AX25_UI |
| RX | TERMINAL, AX25_UI, AX25 |

Hub settings: [MANUAL.md — circuit](MANUAL.md#circuit-main).

## Web browser (WebSocket)

Browser UI: copy `~/hybbx/reverse-proxy/docroot/hybbx-websocket/` to httpd www root.
WebSocket data: `wss://host/hybbx-websocket/ws` → hybbx. See [WEBSOCKET.md](WEBSOCKET.md).
