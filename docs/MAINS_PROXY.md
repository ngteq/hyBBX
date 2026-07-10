# Main-to-Main proxy (`mains_proxy`)

Topology: [TOPOLOGY.md](TOPOLOGY.md).

The **proxy network** links HyBBX **Main** or **Secondary** instances for **pure service linking** — proxymail and proxychat only. All peer traffic uses **HBX/Circuit** (`circuit_host`, `link_id`, `link_password`). Never open a raw TCP socket between HyBBX processes.

**No user accounts, rights, passwords, or other Main data cross proxy links.** Each node keeps its own users and permissions. Only proxymail and proxychat service payloads are relayed.

**No Sysop, Admin, or Mod actions cross proxy links** — no remote administration, `/broadcast`, shutdown, or account management over proxy.

Local `/mail`, `/chat`, and `/broadcast` stay on each node only.

A **Secondary** may run `mains_proxy` as an outbound HBX client to a remote Main (same pattern as packet-radio edge links). On a **Main**, peers may attach directly (full or half duplex).

**Opt-in:** `-DHYBBX_PLUGIN_MAINS_PROXY=ON` and `[networks] mains_proxy=yes`.

## Topology

```
Main-A  <--- HBX :7323 (LINK_AUTH, role=proxy) --->  Main-B
   ^                                                    ^
   | optional Secondary / AX.25                         |
   +----------------------------------------------------+
```

Configure reciprocal peers on both sides:

- **Outbound:** `circuit_host` + `link_id` + `link_password` on the connecting node.
- **Inbound:** matching `link_id` + `link_password` in `[transport.mains_proxyN]` on the receiving node (circuit bridge registry). The remote party’s `link_id` in `LINK_AUTH` must match.

Addresses use `user@peer_id` where `peer_id` matches the remote service name (`[service] name=` on that Main) or the configured `peer_id` / `link_id` on this side.

## INI

```ini
[networks]
mains_proxy = yes

[service]
name = main-west

[transport.mains_proxy1]
peer_id = main-east
circuit_host = 192.0.2.10
circuit_port = 7323
link_id = main-west
link_password = changeme
wire = circuit
duplex = full
use_secondary = yes
```

On **main-east**, register the inbound peer (no `circuit_host` required when only receiving):

```ini
[transport.mains_proxy1]
peer_id = main-west
link_id = main-west
link_password = changeme
```

| Key | Default | Notes |
|-----|---------|-------|
| `peer_id` | (empty) | Logical peer / remote service name |
| `circuit_host` | — | Remote HBX hub hostname or IP (outbound) |
| `circuit_port` | `7323` | Remote HBX hub port |
| `link_id` | — | This node’s identity in `LINK_AUTH` |
| `link_password` | — | `LINK_AUTH` secret (match peer `[circuit]`) |
| `wire` | `circuit` | `circuit` (active) or `ax25` (reserved) |
| `duplex` | `full` | `full` or `half` (reserved) |
| `use_secondary` | `yes` | Reserved — prefer Secondary HBX path |

Legacy keys `host` / `port` are deprecated.

## User commands

| Command | Role |
|---------|------|
| `/proxymail` or `/mail proxymail` | Inter-Main mailbox sub-area |
| `/proxymail send user@remote-main subject` | Compose; `/proxymail done` to send |
| `/proxychat` or `/chat proxychat` | Inter-Main chat sub-area |

Local `/mail` and `/chat` stay on this node only.

## Build

```bash
cmake -B build -DHYBBX_PLUGIN_MAINS_PROXY=ON
cmake --build build
```

## Code

`include/hybbx/mains_proxy.h` · `src/core/mains_proxy.c` · `plugins/mains_proxy/mains_proxy.c`
