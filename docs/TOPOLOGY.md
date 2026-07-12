# HyBBX topology

**v2.0.0** — Main, Secondary, and mains-proxy mesh. INI: [MANUAL.md](MANUAL.md). Mesh detail: [MAINS_PROXY.md](MAINS_PROXY.md).

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

**HBX — Hybrid Bridge eXchange (v1)** — HyBBX-internal framed protocol on the circuit TCP hub. Multiplexes link-layer payloads (AX.25, …) and application streams (terminal, proxymail, proxychat) between Main, Secondary, and proxy peers. Header magic: `H` `B` `X`.

In running text: **HBX** or **HBX/Circuit** — **Circuit** = TCP hub (`:7323`), **HBX** = framing on top.

All paths between HyBBX processes use HBX v1 on the internal circuit hub. The application core never sees KISS, AX.25 on-air framing, or serial — only typed HBX frames on TCP (loopback or routed).

| Path | Attachment |
|------|------------|
| Localhost Secondary | TCP → Main `:7323`, `LINK_AUTH` |
| Remote Secondary | TCP → Main `:7323` |
| Packet radio / BayCom / ARDOP / CRDOP | Plugin edge → HBX client |
| AX.25 auto-beacon (INI) | Main → HBX → Secondary extenders (staggered) |
| `/broadcast <msg>` (Sysop) | Logged-in local Main users (telnet/SSH/WebSocket) |
| `/broadcast ax25` (Sysop) | Instant sequential RF beacon (`ax25_auto_message`) |
| Entertain Area apps | Main plugins only — [ENTERTAIN.md](ENTERTAIN.md) |
| Proxy network (`mains_proxy`) | Main ↔ Main via HBX circuit client |

Edge daemons authenticate with per-link `link_password`. User wire auth (telnet/SSH) is separate from HBX link auth.

Protocol: `include/hybbx/circuit.h` — default port `7323`, max 16 concurrent links.

## TNC2C validation notes

- Verified in field setup: TNC2C (`protocol=kiss`, 19200 8N1 host) links to HBX
  with `LINK_AUTH` (`link_id=packet-radio1`) and stays online.
- AX.25 broadcast TX path is verified end-to-end (HyBBX log + on-air audio TX).
- Broadcast TX participates in the circuit balancer as low-priority traffic:
  send only while link reserve is available for user/mesh traffic.
- Further validation still required: long-run stability (hours/days), multi-link
  contention behavior, and dual-station RX/TX under sustained traffic.

## Proxy network (`mains_proxy`)

Link Main or Secondary instances for **pure service linking** — proxymail and proxychat only. **No user accounts, rights, or other Main data cross proxy links.** **No Sysop, Admin, or Mod actions cross proxy links.**

```
Main-A  <--- HBX circuit :7323 + LINK_AUTH (role=proxy) --->  Main-B
   ^                                                          ^
   | optional Secondary / AX.25                             |
   +----------------------------------------------------------+
```

- **Opt-in:** `-DHYBBX_PLUGIN_MAINS_PROXY=ON` and `[networks] mains_proxy=yes`
- Peers use `circuit_host`, `circuit_port`, `link_id`, `link_password` (not a separate mesh TCP port)
- `wire=circuit` (default); `wire=ax25` reserved
- Secondary may run outbound `mains_proxy`; configure reciprocal `[transport.mains_proxyN]` on each peer

Inter-Main mail and chat: `/proxymail` and `/proxychat`. INI keys: [MAINS_PROXY.md](MAINS_PROXY.md). `/broadcast` stays on each local node only.

## Choosing a layout

| Goal | Layout |
|------|--------|
| Users + one local TNC | Standalone Main |
| Users in DC, RF at remote site | Main + Secondary |
| Two BBS communities linked | Two Mains + `mains_proxy` |
| High RF fan-out | Main + multiple Secondaries (`link_id` each) |

## See also

[MANUAL.md](MANUAL.md) · [TNCS.md](TNCS.md) · [BUILD.md](BUILD.md)
