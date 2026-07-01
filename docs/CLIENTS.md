# HyBBX standalone clients

`hybbx-telnet` and `hybbx-terminal` are **pure CLI clients** for the **centralized daemon** and **link/repeater** circuit paths — command-line parameters and environment variables only. No INI files, no configuration files, no GUI.

They connect to a running HyBBX server (or any compatible endpoint) and do not embed the daemon, transport plugins, or server storage.

Both clients share a minimal static library (`hybbx_client`) with HBX circuit helpers, AX.25 framing, link password auth, and the 2400 baud / 40-column traffic display profile.

## Build (clients only)

No server, no plugins, no crypto third-party bundles:

```bash
cmake -B build-clients -DHYBBX_CLIENTS_ONLY=ON
cmake --build build-clients
```

Equivalent explicit options:

```bash
cmake -B build-clients \
  -DHYBBX_BUILD_DAEMON=OFF \
  -DHYBBX_BUILD_PLUGINS=OFF \
  -DHYBBX_BUILD_CLIENTS=ON
cmake --build build-clients
```

Binaries:

- `build-clients/src/clients/hybbx-telnet`
- `build-clients/src/clients/hybbx-terminal`

Install:

```bash
cmake --install build-clients --prefix "$HOME/hybbx-clients"
```

Or: `./scripts/build-clients.sh`

## Build (full tree)

Default CMake also builds both clients alongside `hybbx`:

```bash
cmake -B build && cmake --build build
```

Disable individual clients:

```bash
cmake -B build -DHYBBX_BUILD_CLIENT_TELNET=OFF
cmake -B build -DHYBBX_BUILD_CLIENT_TERMINAL=OFF
```

## hybbx-telnet

Official HyBBX telnet client for testing and operator sessions.

```bash
hybbx-telnet -H 127.0.0.1 -p 2323
hybbx-telnet --baud 2400 --line-width 40
hybbx-telnet -u myuser -P secret
hybbx-telnet 192.0.2.1 2323
```

| Parameter | Environment | Default |
|-----------|-------------|---------|
| `-H`, `--host` | `HYBBX_HOST` | `127.0.0.1` |
| `-p`, `--port` | `HYBBX_PORT` | `2323` |
| `-u`, `--user` | — | — |
| `-P`, `--password` | — | — |
| `--baud` | — | `2400` |
| `--line-width` | — | `40` |
| `--pace yes\|no` | — | `yes` |
| `--ansi yes\|no` | — | `no` |

## hybbx-terminal

AX.25 / HBX circuit terminal client for packet-radio and link testing over the internal circuit hub.

```bash
hybbx-terminal -H 127.0.0.1 -p 7323
hybbx-terminal --link-id edge-1 --link-password changeme --link-role gateway
hybbx-terminal --mycall DL1ABC-0 --dest DL9XYZ-0 --via RELAY-7 --ax25-ui
```

| Parameter | Environment | Default |
|-----------|-------------|---------|
| `-H`, `--host` | `HYBBX_CIRCUIT_HOST` | `127.0.0.1` |
| `-p`, `--port` | `HYBBX_CIRCUIT_PORT` | `7323` |
| `--link-id` | — | `terminal-1` |
| `--link-password` | — | — |
| `--link-role` | — | `gateway` |
| `--mycall`, `--dest`, `--via` | — | — |
| `--ax25-ui` | — | off |
| `--baud`, `--line-width`, `--pace`, `--ansi` | — | HyBBX traffic defaults |

### Protocol support

| Direction | HBX proto | Notes |
|-----------|-----------|-------|
| TX (default) | `TERMINAL` | Line-oriented stdin → HBX terminal frames |
| TX (`--ax25-ui`) | `AX25_UI` | Requires `--mycall` and `--dest` |
| RX | `TERMINAL`, `AX25_UI`, `AX25` | UI payload extracted and paced to stdout |

Link authentication uses HBX `LINK_AUTH` / `LINK_AUTH_ACK` (link/repeater edge daemon toward centralized daemon). Password only — no ping/pong health checks.

Server-side circuit and packet-radio setup: [MANUAL.md](MANUAL.md).
