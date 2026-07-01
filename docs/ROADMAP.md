# HyBBX roadmap

Planned capabilities not yet in the codebase. Update [FEATURES.md](FEATURES.md) when an item ships.

## Architecture standard

**HyBBX uses a centralized daemon and link/repeater daemon technologies to expand networks, range, and features.**

| Daemon | Role |
|--------|------|
| **Centralized daemon** (`hybbx`) | Full service: users, storage, mail-area (later), chat, commands, HBX hub |
| **Link/repeater edge daemons** | Relay IP or RF (AX.25) traffic toward the centralized daemon |

Edge daemons extend network reach and features; they do not host a parallel user database, mail area, or session core.

### Common terms (packet radio)

HyBBX uses familiar amateur-packet wording for link/repeater edge daemon roles:

| Term | Typical meaning | HyBBX edge role (planned) |
|------|-----------------|---------------------------|
| **Gateway** | IP ↔ packet or network entry point | Telnet, SSH, WebSocket, or HBX circuit attachment to the **centralized daemon** |
| **Digipeater** (digi) | AX.25 store-and-forward; appears in `via` path (`RELAY`, `WIDE`, …) | RF relay forwarding frames toward the **centralized daemon** |
| **Repeater** | RF relay extending range (often spoken on 11 m / VHF packet) | RF edge site repeating toward the **centralized daemon** |
| **Link** | Dedicated point-to-point RF or circuit bridge | Station-to-station hop on the path to the **centralized daemon** |
| **`via` / path** | Comma-separated digipeater list in AX.25 headers | Already in `[transport.packet_radio]` as `via=` (e.g. `RELAY-7,WIDE1-1`) |

On-air paths use normal AX.25 digipeater calls; link/repeater edge daemons carry those paths back to the centralized daemon.

```
                         ┌──────────────────────────────────┐
                         │   centralized daemon (hybbx)     │
                         │   users · mail · chat · storage  │
                         └────────────────┬─────────────────┘
                                          │
              link/repeater edge daemons (gateway · digi · repeater · link)
                                          │
         ┌────────────────────────────────┼────────────────────────────────┐
         │                                │                                │
  ┌──────▼──────┐                 ┌───────▼───────┐                 ┌──────▼──────┐
  │  gateway    │                 │ digipeater /  │                 │    link     │
  │  (IP edge)  │                 │   repeater    │◄───────────────►│  (station)  │
  │             │                 │  (RF / AX.25) │   via RELAY…    │             │
  └──────┬──────┘                 └───────┬───────┘                 └─────────────┘
         │                                │
    telnet / WS clients            expanded RF reach (digipeated paths)
```

| Edge role | Purpose |
|-----------|---------|
| **Gateway** | IP edge (telnet, SSH, WebSocket, …) — clients or HBX circuits to the **centralized daemon** |
| **Digipeater** | AX.25 digi — store-and-forward; `via` path toward the **centralized daemon** |
| **Repeater** | RF relay site — extends coverage toward the **centralized daemon** |
| **Link** | Point-to-point bridge between stations or networks on the path to the **centralized daemon** |

Link/repeater edge daemons use an upstream target (`core_host`, `circuit_host`, or equivalent) pointing at the centralized daemon.

Today: telnet and `packet_radio` on the centralized daemon host; `packet_radio` already acts as an HBX **link client** to `[circuit]` and supports AX.25 **`via`** digipeater lists. Dedicated edge-daemon **roles** and multi-site expansion are planned.

---

## Mail-Area (planned)

BBS/mailbox-inspired **mail area** on the **centralized daemon only**:

- User-to-user or system messages (persistent mailbox — not chat)
- Areas/folders configured on core
- Reached via link/repeater edge daemons — storage stays on the centralized daemon

Edge daemons **never** hold mail databases.

---

## Link/repeater edge daemons (planned)

Modes for daemons that are **not** the centralized `hybbx` instance:

- Transparent or semi-transparent relay of HBX and/or AX.25 toward the centralized daemon `circuit` / core
- RF: expand networks, range, and features across digipeater sites, repeater pairs, and link stations
- On-air: normal AX.25 paths with digipeater aliases (`RELAY`, `WIDE`, …) in `via`
- INI roles such as `role=gateway`, `role=digipeater`, `role=repeater`, `role=link` plus mandatory `core_host` / upstream

Deploy pattern: **one centralized daemon + link/repeater edge daemons** — one user database, one mail-area, one authority.

---

## Auto-generated link codes (planned)

For link/repeater edge daemon **completion** only — when pairing to the centralized daemon finishes successfully:

| Property | Rule |
|----------|------|
| **When** | Only after **successful** handshake with the centralized daemon |
| **What** | Short auto-generated token identifying the edge daemon to the core |
| **Use** | Centralized daemon acknowledges the edge; optional operator log reference |
| **Not for** | User login, mail, or chat |

No extra prompts during normal operation; codes appear when setup completes.

---

## Other planned items

| Item | Notes |
|------|-------|
| SSH transport | Gateway edge daemon to centralized session core |
| WebSocket | Gateway edge daemon behind reverse-proxy |
| SQL storage | On **centralized daemon** only |
| HBX protocols | APRS, NETROM reserved IDs |
| BayCom `ser12` | Use `kissattach` + KISS until documented |

See also [FEATURES.md — Roadmap](FEATURES.md#roadmap-not-yet-implemented).
