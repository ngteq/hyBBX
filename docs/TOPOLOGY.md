# HyBBX topology

**v1.5.0** — Main, Secondary, and mains-proxy mesh. INI: [MANUAL.md](MANUAL.md). Mesh detail: [MAINS_PROXY.md](MAINS_PROXY.md).

## Roles

| Role | Process | Typical `[networks]` | Hosts |
|------|---------|----------------------|-------|
| **Main** | `hybbx` | `circuit=yes`, `mains_proxy` optional | Users (telnet/SSH/WebSocket), storage, HBX hub `:7323` |
| **Secondary** | `hybbx` (separate host) | `circuit=no`, `ax25=yes` | RF edge; HBX client to Main |
| **Standalone Main** | `hybbx` | `ax25=yes` on same box | Lab / single-host |

**Secondary** is infrastructure (link adapter), not a telnet user. Multiple Secondaries need unique `link_id` per active link; `max_links` on Main (default 8, max 16).

### RF attachment (both valid)

| Layout | TNC host | Main `[transport.packet_radioN]` |
|--------|----------|--------------------------------|
| **Remote Secondary** | Secondary (`device`, `tnc`, `circuit_host`) | Bridge registry only (`link_id`, password) |
| **Local TNC on Main** | Same box as users | Full TNC keys (`device`, `tnc`, …) |

Bridge-registry rows on Main are skipped at start (no serial open). A missing or unplugged TNC logs a warning and that edge instance does not start; other transports keep running.

## Default layout

```
Users (telnet :2323, SSH :3232, WebSocket) ──► Main (storage, mail, chat)
                                                      ▲
                                                      │ HBX/TCP :7323
                                                 Secondary (packet_radio / ardop / crdop / baycom)
```

Remote RF: run Secondary near the TNC; point `circuit_host` at Main. Main holds user sessions and the HBX hub; Secondary translates serial/KISS/AX.25 ↔ HBX frames.

## HBX/Circuit — sole inter-node transport

All paths between HyBBX processes use **HBX v1** on the internal circuit hub. The application core never sees KISS, AX.25 on-air framing, or serial — only typed HBX frames on TCP (loopback or routed).

| Path | Attachment |
|------|------------|
| Localhost Secondary | TCP → Main `:7323`, `LINK_AUTH` |
| Remote Secondary | TCP → Main `:7323` |
| Packet radio / BayCom / ARDOP / CRDOP | Plugin edge → HBX client |
| AX.25 auto-beacon (INI) | Main → HBX → Secondary extenders |
| `/broadcast` (Sysop) | All online users on local Main only |
| mains_proxy mesh (stub) | Main ↔ Main via HBX circuit client |

Edge daemons authenticate with per-link `link_password`. User wire auth (telnet/SSH) is separate from HBX link auth.

Protocol: `include/hybbx/circuit.h` — default port `7323`, max 16 concurrent links.

## Main-to-Main mesh (`mains_proxy`)

Link two or more Main instances. **Status: stub** — API and plugin skeleton; no live relay yet.

```
Main-A  <--- HBX circuit :7323 + LINK_AUTH --->  Main-B
   ^                                              ^
   | optional Secondary / AX.25                   |
   +----------------------------------------------+
```

- **Opt-in:** `-DHYBBX_PLUGIN_MAINS_PROXY=ON` and `[networks] mains_proxy=yes`
- Peers use `circuit_host`, `circuit_port`, `link_id`, `link_password` (not a separate mesh TCP port)
- `wire=circuit` (default); `wire=ax25` reserved for RF-carried mesh
- Secondary edge recommended for RF but optional — configure reciprocal `[transport.mains_proxyN]` on each Main

Inter-Main mail and chat: `/proxymail` and `/proxychat` (stubs). INI keys: [MAINS_PROXY.md](MAINS_PROXY.md).

## Choosing a layout

| Goal | Layout |
|------|--------|
| Users + one local TNC | Standalone Main |
| Users in DC, RF at remote site | Main + Secondary |
| Two BBS communities linked | Two Mains + `mains_proxy` (when implemented) |
| High RF fan-out | Main + multiple Secondaries (`link_id` each) |

## See also

[MANUAL.md](MANUAL.md) · [FEATURES.md](FEATURES.md) · [TNCS.md](TNCS.md) · [BUILD.md](BUILD.md)
