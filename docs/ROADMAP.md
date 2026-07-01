# HyBBX roadmap

Planned work on the **0.5.x** line. Per-version `docs/RELEASE-*` freeze/scope files are introduced at **v1.0.0** (generated then, not maintained before).

## Architecture

Centralized **main** daemon + **secondary** (edge) nodes over **TCP/IP**. Same `hybbx` binary; role is defined by INI.

| Role | Also called | Purpose |
|------|-------------|---------|
| **Main** | centralized, primary | Users, storage, mail, chat, telnet, HBX circuit hub |
| **Secondary** | edge, link, shack node | TNC/modem/RF locally; bridges to main via HBX/TCP |

HyBBX does **not** integrate VPNs. Links use plain TCP/IP (`circuit_host`:`circuit_port`). Admins and sysops may add VPN, SSH tunnels, or reverse proxies externally.

Config templates:

| Template | Use |
|----------|-----|
| `share/hybbx.ini.example` | Main / datacenter instance |
| `share/hybbx-secondary.ini.example` | Secondary with TNC (no local mail hub) |

### Main / secondary bridge (HBX over TCP) — **done (single link)**

```
  Secondary (TNC + RF)                 Main (datacenter)
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

**Current limit:** the hub accepts **one active circuit link** at a time (one RF site → one BBS session on main). Multi-secondary fan-in is planned below.

### Edge roles (packet radio)

| Term | Secondary role |
|------|----------------|
| **Link** | P2P RF hop → main (`link_role = link`) |
| **Gateway** | IP/RF edge → main |
| **Digipeater** | AX.25 `via` relay on RF |
| **Repeater** | RF range extension |

`link_role` is recorded in the link registry today; routing semantics per role are **planned**.

```
                         ┌──────────────────────────────────┐
                         │   main instance (hybbx)          │
                         │   telnet + circuit + mail        │
                         └────────────────┬─────────────────┘
                                          │ HBX/TCP
              secondary nodes (link · gateway · digi · repeater)
                                          │
         ┌────────────────────────────────┼────────────────────────────────┐
         │                                │                                │
  ┌──────▼──────┐                 ┌───────▼───────┐                 ┌──────▼──────┐
  │  gateway    │                 │ digipeater /  │                 │    link     │
  │  (IP edge)  │                 │   repeater    │◄───────────────►│  (shack)    │
  └──────┬──────┘                 └───────┬───────┘   via RELAY…    └──────┬──────┘
         │                                │                                │
    telnet clients                 expanded RF reach                  TNC2C + RF
```

---

## Multi-link (several secondaries → one main) — **planned**

Goal: multiple secondary nodes connected to one main instance simultaneously, each with its own authenticated link and session (or shared pool — TBD).

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
- Main storage remains authoritative; secondaries do not host mail/user DB
- No VPN/TLS inside HyBBX — document external wrapping only

Operator doc: [MANUAL.md — Main and secondary deployment](MANUAL.md#main-and-secondary-deployment-tcpip-bridge).

---

## Mail-Area

Mailbox on **main** only (`data/mail/<user>/inbox/`). Secondaries forward RF sessions; `enabled = no` in secondary INI.

---

## Link/repeater edge modes — **partial**

Relay HBX/AX.25 to `[circuit]`. INI: `link_role`, `circuit_host`, `link_*`. Packet-radio secondary template ships in 0.5.x.

Dedicated minimal edge-only binary mode (no local telnet hub) — optional future simplification; today the same `hybbx` process with secondary INI is enough.

---

## Link codes — **done**

Short token after successful secondary→main `LINK_AUTH` handshake (`LINK_AUTH_ACK`). Registry: `data/links/<id>.ini` and `[link.<id>]` in hybbx.ini. Not used for user login.

---

## Other planned items

| Item | Notes |
|------|-------|
| Multi-link hub | See table above |
| SSH transport | Gateway secondary → main session core |
| WebSocket | Gateway secondary behind reverse-proxy |
| SQL storage | On **main** only |
| HBX protocols | APRS, NETROM reserved IDs |
| BayCom `ser12` | Use `kissattach` + KISS until documented |

See also [FEATURES.md — Roadmap](FEATURES.md#roadmap-not-yet-implemented).
