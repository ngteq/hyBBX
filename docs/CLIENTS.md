# HyBBX standalone clients

CLI-only: `hybbx-telnet`, `hybbx-terminal` (flags/env — no INI). See source: `src/clients/`.

## Build (clients only)

```bash
cmake -B build-clients -DHYBBX_CLIENTS_ONLY=ON
cmake --build build-clients
```

Or `./scripts/build-clients.sh` · Binaries: `build-clients/src/clients/hybbx-{telnet,terminal}`

## hybbx-telnet

```bash
hybbx-telnet -H 127.0.0.1 -p 2323
hybbx-telnet -u user -P pass --baud 2400 --line-width 80
```

Env: `HYBBX_HOST`, `HYBBX_PORT`

## hybbx-terminal

HBX circuit client (AX.25 UI optional). Env: `HYBBX_CIRCUIT_HOST`, `HYBBX_CIRCUIT_PORT`.

```bash
hybbx-terminal -H 127.0.0.1 -p 7323
hybbx-terminal --link-id ID --link-password SECRET --link-role gateway
hybbx-terminal --mycall CALL --dest CALL --ax25-ui
```

| TX | `--ax25-ui` off → HBX TERMINAL; on → AX25_UI |
| RX | TERMINAL, AX25_UI, AX25 |
| Auth | HBX LINK_AUTH (password only) |

Server `[circuit]`: [MANUAL.md](MANUAL.md).
