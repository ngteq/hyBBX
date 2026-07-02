# HyBBX roadmap

Planned work on the **0.7.x** line. Per-version `docs/RELEASE-*` freeze/scope files are introduced at **v1.0.0** (generated then, not maintained before).

## Architecture

**Main** and **Secondary** instances over **TCP/IP**. Same `hybbx` binary; role is defined by INI.

### Default deployment model (datacenter Main)

HyBBX ships a **datacenter-oriented standard configuration**. Every switch below is **overrideable** — Main can run AX.25 and all adapters locally without remote HBX when desired.

| | **Main** (default) | **Secondary** (default) |
|---|-------------------|-------------------------|
| **Purpose** | Users, storage, mail, chat, TCP/IP services | TNC, modem, RF, other link hardware |
| **`[networks] ax25`** | `no` | `yes` |
| **`[networks] websocket`** | `no` | `no` (until used) |
| **`[networks] circuit`** | `yes` (HBX hub, loopback bind) | `no` (HBX client via `circuit_host`) |
| **Telnet** | static-enabled (user access) | loopback maintenance only in template |
| **Typical wire** | TCP telnet + HBX TCP :7323 | AX.25/RF → HBX/TCP → Main |

On the default path, **Main exposes TCP/IP only** (telnet + circuit hub). **AX.25 and further connection types run on Secondary instance(s)** that bridge to Main. For a **standalone Main**, set `ax25=yes`, configure `[transport.packet_radio]` with local TNC settings, and omit Secondary nodes — all connection types are then managed on one host with no remote HBX link.

| Role | Purpose |
|------|---------|
| **Main** | Users, storage, mail, chat, telnet, HBX circuit hub (TCP/IP default) |
| **Secondary** | TNC/modem/RF locally; bridges non-TCP adapters to Main via HBX/TCP |

HyBBX does **not** integrate VPNs, firewalls, tunnels, or link proxies. Secondaries connect to a **Main** `circuit_host`:`circuit_port` or a **custom proxy** you operate — no HyBBX proxy is shipped before **v1.0**. Links use plain TCP/IP. HyBBX/HBX auth ships **`link_id`** and **`link_password`** only — configure the same keys on Main bridge sections and on each Secondary. Use **system firewall**, **system VPN**, **SSH tunnels**, reverse proxies, or TLS wrappers externally for advanced security.

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
- Main circuit hub IPv4+IPv6 (`[circuit]`, default port **7323**, loopback **127.0.0.1** / **::1**)
- Secondary `packet_radio` client (`circuit_host`, `link_password`, `link_id`)
- `LINK_AUTH` + link registry + stale prune (`src/core/link.c`)
- AX.25 uplink → session; session output → terminal downlink
- Standalone test client: `hybbx-terminal`

**Current limit:** the Main hub accepts **one active circuit link** at a time (one Secondary RF path → one BBS session on Main). Multi-secondary fan-in is planned below.

### Bridge sections (`transport.<plugin>N`)

Each Secondary with its own RF path or link adapter gets a **matching bridge section** on Main. Increment **N** per Secondary:

| Side | Example section | HyBBX keys (same on both) |
|------|-----------------|---------------------------|
| **Main** | `[transport.packet_radio1]` | `link_id`, `link_password`, `link_role` |
| **Secondary** | `[transport.packet_radio1]` | same `link_id`, `link_password`, `link_role` + TNC/RF tuning |

Second Secondary on Main: `[transport.packet_radio2]` with its own `link_id` / `link_password`; Secondary INI uses the same section name and credentials.

HyBBX does **not** configure firewall rules, VPN endpoints, or tunnels — operators handle those at the OS/network layer (e.g. restrict TCP **7323** to known Secondary IPs, WireGuard/OpenVPN to a private `circuit_host`, `ssh -L` port forward). HBX stays plain TCP on the chosen address/port.

**Today:** bridge sections document the registry convention; runtime still uses one `[circuit]` hub and one active link. Multi-link code will read numbered bridge sections — see below.

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
| **1** | Hub accepts **N** concurrent TCP links; one session per `link_id`; bridge registry from `[transport.<plugin>N]` on Main | Planned |
| **2** | Link registry drives routing; `link_role` affects hub behavior | Planned |
| **3** | Optional fan-out (broadcast/maintenance to all links) | Planned |
| **4** | Resilience: reconnect policy, optional link heartbeat (operator choice) | Planned |

**Implementation notes (draft):**

- Replace single `link_fd` in `circuit_tcp.c` with a per-link table keyed by `link_id`
- Main `[transport.packet_radio1]`, `[transport.packet_radio2]`, … register expected links (`link_id`, `link_password`)
- Cap concurrent links (`max_links` in `[circuit]`, default TBD)
- Main storage remains authoritative; Secondaries do not host mail/user DB
- No VPN/TLS/firewall inside HyBBX — system-level only

Operator doc: [MANUAL.md — Main and secondary deployment](MANUAL.md#main-and-secondary-deployment-tcpip-bridge).

---

## Mail-Area

Mailbox on **Main** only (`data/mail/<user>/inbox/`). Secondaries forward RF sessions; `enabled = no` in secondary INI.

---

## Secondary modes — **partial**

Relay HBX/AX.25 to Main `[circuit]`. INI: `link_role`, `circuit_host`, `link_*`. Secondary template ships in 0.7.x.

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
