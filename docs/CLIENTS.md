# Standalone clients

No INI — flags and environment only. Sources: `src/clients/`.

Build: `-DHYBBX_CLIENTS_ONLY=ON` or `./scripts/build-clients.sh`.

## hybbx-telnet

```bash
hybbx-telnet -H 127.0.0.1 -p 2323
hybbx-telnet -u user -P pass --baud 2400
```

Env: `HYBBX_HOST`, `HYBBX_PORT`

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

Browser UI: `~/hybbx/hybbx-websocket/` + `~/hybbx/reverse-proxy/`. WebSocket: `wss://host/hybbx-websocket/ws`.
See [WEBSOCKET.md](WEBSOCKET.md).
