# Main-to-Main proxy (`mains_proxy`)

**Status:** stub — mesh API and plugin skeleton; no live relay yet. Topology: [TOPOLOGY.md](TOPOLOGY.md).

Link two or more HyBBX **Main** instances. All peer traffic uses **HBX/Circuit** (`circuit_host`, `link_id`, `link_password`) — the same model as Secondary edge links. Never open a raw TCP socket between Main processes.

A **Secondary** on each site is recommended for RF paths but optional. Peers may attach Main ↔ Main directly (full or half duplex).

**Opt-in:** `-DHYBBX_PLUGIN_MAINS_PROXY=ON` and `[networks] mains_proxy=yes`.

## Topology

```
Main-A  <--- HBX :7323 (LINK_AUTH) --->  Main-B
   ^                                        ^
   | optional Secondary / AX.25             |
   +----------------------------------------+
```

Configure reciprocal peers on both sides for bidirectional mesh.

## INI

```ini
[networks]
mains_proxy = yes

[transport.mains_proxy1]
peer_id = main-east
circuit_host = 192.0.2.10
circuit_port = 7323
link_id = main-east
link_password = changeme
wire = circuit
duplex = full
use_secondary = yes
```

| Key | Default | Notes |
|-----|---------|-------|
| `peer_id` | (empty) | Logical peer name in logs |
| `circuit_host` | — | Remote Main HBX hub hostname or IP |
| `circuit_port` | `7323` | Remote HBX hub port |
| `link_id` | — | Must match peer's bridge registry |
| `link_password` | — | `LINK_AUTH` secret (match Main `[circuit]`) |
| `wire` | `circuit` | `circuit` or `ax25` (reserved) |
| `duplex` | `full` | `full` or `half` |
| `use_secondary` | `yes` | Prefer Secondary HBX path when available |

Legacy keys `host` / `port` are deprecated (mapped to `circuit_host` only when unset); mesh connect never uses a separate mesh TCP port.

## User commands (stubs)

| Command | Role |
|---------|------|
| `/proxymail` or `/mail proxymail` | Inter-Main mailbox sub-area |
| `/proxymail send user@remote-main subject` | Compose; `/proxymail done` to send (stub) |
| `/proxychat` or `/chat proxychat` | Inter-Main chat sub-area (stub) |

Local `/mail` and `/chat` stay on this Main only.

## Build

```bash
cmake -B build -DHYBBX_PLUGIN_MAINS_PROXY=ON
cmake --build build
```

## Code

`include/hybbx/mains_proxy.h` · `src/core/mains_proxy.c` · `plugins/mains_proxy/mains_proxy.c`
