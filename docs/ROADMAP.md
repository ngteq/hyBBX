# HyBBX roadmap

Planned capabilities not yet in the codebase. Update [FEATURES.md](FEATURES.md) when an item ships.

## Deployment layout (planned)

**Rule: exactly one central hyBBX.** Every other node is **only** an edge relay toward that central instance — using the usual packet-radio terms: **gateway**, **digipeater**, **repeater**, **link**, and similar. Never a second full hyBBX with its own users, mail, or session core.

| Count | Role | What it runs |
|-------|------|----------------|
| **1×** | **Central hyBBX** | Full service: users, storage, mail-area (later), chat, commands, HBX hub |
| **N×** | **Gateway / digipeater / repeater / link** | Edge transport only: forward IP or RF (AX.25) traffic to the central instance |

There is no multi-master layout. Edge nodes **repeat, digipeat, or bridge** toward the one core; they do not host a parallel BBS.

### Common terms (packet radio)

HyBBX uses familiar amateur-packet wording alongside its own role names:

| Term | Typical meaning | HyBBX edge role (planned) |
|------|-----------------|---------------------------|
| **Gateway** | IP ↔ packet or network entry point | Telnet, SSH, WebSocket, or HBX circuit attachment to **central** |
| **Digipeater** (digi) | AX.25 store-and-forward; appears in `via` path (`RELAY`, `WIDE`, …) | RF relay forwarding frames toward **central** (configured digi / `via` on air) |
| **Repeater** | RF relay extending range (often spoken on 11 m / VHF packet) | RF edge site repeating toward **central** |
| **Link** | Dedicated point-to-point RF or circuit bridge | Station-to-station hop on the path to **central** |
| **`via` / path** | Comma-separated digipeater list in AX.25 headers | Already in `[transport.packet_radio]` as `via=` (e.g. `RELAY-7,WIDE1-1`) |

On-air paths use normal AX.25 digipeater calls; hyBBX edge nodes are the infrastructure that carries those paths back to the **single** central instance.

```
                         ┌──────────────────────────────────┐
                         │  1× central hyBBX (only core)    │
                         │  users · mail · chat · storage   │
                         └────────────────┬─────────────────┘
                                          │
              gateway / digipeater / repeater / link → central only
                                          │
         ┌────────────────────────────────┼────────────────────────────────┐
         │                                │                                │
  ┌──────▼──────┐                 ┌───────▼───────┐                 ┌──────▼──────┐
  │  gateway    │                 │  digipeater   │                 │    link     │
  │  (IP edge)  │                 │  / repeater   │◄───────────────►│  (station)  │
  │             │                 │  (RF / AX.25) │   via RELAY…    │             │
  └──────┬──────┘                 └───────┬───────┘                 └─────────────┘
         │                                │
    telnet / WS clients            expanded RF reach (digipeated paths)
```

| Edge role | Purpose |
|-----------|---------|
| **Gateway** | IP edge (telnet, SSH, WebSocket, …) — clients or HBX circuits to **central** |
| **Digipeater** | AX.25 digi — store-and-forward; `via` path toward **central** |
| **Repeater** | RF relay site — extends coverage toward **central** (often colocated with a digi) |
| **Link** | Point-to-point bridge between stations or networks on the path to **central** |

All edge nodes are configured with an upstream target (`core_host`, `circuit_host`, or equivalent) pointing at the **single** central instance.

Today: telnet and `packet_radio` on the central host; `packet_radio` already acts as an HBX **link client** to `[circuit]` and supports AX.25 **`via`** digipeater lists. Dedicated gateway / digipeater / repeater / link **roles** and multi-site topology are planned.

---

## Mail-Area (planned)

BBS/mailbox-inspired **mail area** on the **central instance only**:

- User-to-user or system messages (persistent mailbox — not chat)
- Areas/folders configured on core
- Reached via gateways, digipeaters, repeaters, and links — storage stays on central

Edge nodes **never** hold mail databases.

---

## Gateway / digipeater / repeater / link solution (planned)

Modes for nodes that are **not** the central hyBBX:

- Transparent or semi-transparent relay of HBX and/or AX.25 toward the central `circuit` / core
- RF: expand range of the one central instance across **digipeater** sites, **repeater** pairs, and **link** stations
- On-air: normal AX.25 paths with digipeater aliases (`RELAY`, `WIDE`, …) in `via`
- INI roles such as `role=gateway`, `role=digipeater`, `role=repeater`, `role=link` plus mandatory `core_host` / upstream

Deploy pattern: **1× central hyBBX + N× gateways / digipeaters / repeaters / links** — one user database, one mail-area, one authority.

---

## Auto-generated link codes (planned)

For gateway / digipeater / repeater / link **completion** only — when pairing to central finishes successfully:

| Property | Rule |
|----------|------|
| **When** | Only after **successful** handshake with the central instance |
| **What** | Short auto-generated token identifying the edge node to central |
| **Use** | Central acknowledges the edge; optional operator log reference |
| **Not for** | User login, mail, or chat |

No extra prompts during normal operation; codes appear when setup completes.

---

## Other planned items

| Item | Notes |
|------|-------|
| SSH transport | Gateway to central session core |
| WebSocket | Gateway behind reverse-proxy |
| SQL storage | On **central** only |
| HBX protocols | APRS, NETROM reserved IDs |
| BayCom `ser12` | Use `kissattach` + KISS until documented |

See also [FEATURES.md — Roadmap](FEATURES.md#roadmap-not-yet-implemented).
