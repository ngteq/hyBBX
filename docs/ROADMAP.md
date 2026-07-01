# HyBBX roadmap

Planned work on the **0.5.x** line. Per-version `docs/RELEASE-*` freeze/scope files are introduced at **v1.0.0** (generated then, not maintained before).

## Architecture

**Main** and **Secondary** instances over **TCP/IP**. Same `hybbx` binary; role is defined by INI.

| Role | Purpose |
|------|---------|
| **Main** | Users, storage, mail, chat, telnet, HBX circuit hub |
| **Secondary** | TNC/modem/RF locally; bridges to Main via HBX/TCP |

HyBBX does **not** integrate VPNs. Links use plain TCP/IP (`circuit_host`:`circuit_port`). Admins and sysops may add VPN, SSH tunnels, or reverse proxies externally.

Config templates:

| Template | Role |
|----------|------|
| `share/hybbx.ini.example` | Main |
| `share/hybbx-secondary.ini.example` | Secondary |

### Main / secondary bridge (HBX over TCP) — **done (single link)**

```
  Secondary (TNC + RF)                 Main
  ┌────────────────────┐   TCP :7323   ┌─────────────────────────┐
  │ hybbx              │ ────────────► │ hybbx                   │
  │ [networks] ax25=yes│   HBX v1      │ [circuit] hub           │
  │ circuit_host=main  │   LINK_AUTH   │ telnet :2323 (users)    │
  └────────────────────┘               │ storage, mail, chat     │
                                       └─────────────────────────┘
```

**Done today:**

- HBX v1 framing on TCP (`src/core/circuit.c`, `circuit_tcp.c`)
- Main circuit hub IPv4+IPv6 (`[circuit]`, default port **7323**)
- Secondary `packet_radio` client (`circuit_host`, `link_password`, `link_id`)
- `LINK_AUTH` + link registry + stale prune (`src/core/link.c`)
- AX.25 uplink → session; session output → terminal downlink
- Standalone test client: `hybbx-terminal`

**Current limit:** the Main hub accepts **one active circuit link** at a time (one Secondary RF path → one BBS session on Main). Multi-secondary fan-in is planned below.

### Secondary `link_role` (INI)

Recorded in the link registry on successful `LINK_AUTH`. Routing semantics per role are **planned**.

| `link_role` | Typical Secondary use |
|-------------|------------------------|
| `link` | Default RF bridge to Main |
| `gateway` | IP/RF access path to Main |
| `digipeater` | AX.25 `via` relay on RF |
| `repeater` | RF range extension |

```
                         ┌──────────────────────────────────┐
                         │   Main                           │
                         │   telnet + circuit + mail        │
                         └────────────────┬─────────────────┘
                                          │ HBX/TCP
                                    Secondary nodes
                                          │
         ┌────────────────────────────────┼────────────────────────────────┐
         │                                │                                │
  ┌──────▼──────┐                 ┌───────▼───────┐                 ┌──────▼──────┐
  │ Secondary A │                 │ Secondary B   │◄───────────────►│ Secondary C │
  │ (gateway)   │                 │ (digipeater)  │   via RELAY…    │ (link)      │
  └──────┬──────┘                 └───────┬───────┘                 └──────┬──────┘
         │                                │                                │
    telnet users                   expanded RF reach                  TNC + RF
```

---

## Multi-link (several secondaries → one Main) — **planned**

Goal: multiple Secondary nodes connected to one Main instance simultaneously, each with its own authenticated link and session (or shared pool — TBD).

| Phase | Scope | Status |
|-------|--------|--------|
| **0** | Single link, single session (today) | **Done** |
| **1** | Hub accepts **N** concurrent TCP links; one session per `link_id` | Planned |
| **2** | Link registry drives routing; `link_role` affects hub behavior | Planned |
| **3** | Optional fan-out (broadcast/maintenance to all links) | Planned |
| **4** | Resilience: reconnect policy, optional link heartbeat (operator choice) | Planned |

**Implementation notes (draft):**

- Replace single `link_fd` in `circuit_tcp.c` with a per-link table keyed by `link_id`
- Cap concurrent links (`max_links` in `[circuit]`, default TBD)
- Main storage remains authoritative; Secondaries do not host mail/user DB
- No VPN/TLS inside HyBBX — external wrapping only

Operator doc: [MANUAL.md — Main and secondary deployment](MANUAL.md#main-and-secondary-deployment-tcpip-bridge).

---

## Mail-Area

Mailbox on **Main** only (`data/mail/<user>/inbox/`). Secondaries forward RF sessions; `enabled = no` in secondary INI.

---

## Secondary modes — **partial**

Relay HBX/AX.25 to Main `[circuit]`. INI: `link_role`, `circuit_host`, `link_*`. Secondary template ships in 0.5.x.

Optional future: minimal Secondary-only process layout; today the same `hybbx` binary with secondary INI is enough.

---

## Link codes — **done**

Short token after successful Secondary→Main `LINK_AUTH` handshake (`LINK_AUTH_ACK`). Registry: `data/links/<id>.ini` and `[link.<id>]` in hybbx.ini. Not used for user login.

---

## Other planned items

| Item | Notes |
|------|-------|
| Multi-link hub | See table above |
| SSH transport | Secondary or Main session access |
| WebSocket | Behind reverse-proxy |
| SQL storage | On **Main** only |
| HBX protocols | APRS, NETROM reserved IDs |
| BayCom `ser12` | Use `kissattach` + KISS until documented |

See also [FEATURES.md — Roadmap](FEATURES.md#roadmap-not-yet-implemented).
