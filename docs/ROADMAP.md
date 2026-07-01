# HyBBX roadmap

Planned work on the **0.5.x** line. Per-version `docs/RELEASE-*` freeze/scope files are introduced at **v1.0.0** (generated then, not maintained before).

## Architecture

Centralized `hybbx` daemon + link/repeater edge daemons → expand networks/range/features.

| Daemon | Role |
|--------|------|
| **Centralized** (`hybbx`) | Users, storage, mail (later), chat, commands, HBX hub |
| **Edge** (link/repeater) | Relay IP/RF (AX.25) to centralized daemon |

Edge daemons: no parallel user DB or mail store.

### Edge roles (packet radio)

| Term | Edge role |
|------|-----------|
| Gateway | IP → `[circuit]` |
| Digipeater | AX.25 `via` relay |
| Repeater | RF range extension |
| Link | P2P hop |
| `via` | `[transport.packet_radio] via=` |

```
                         ┌──────────────────────────────────┐
                         │   centralized daemon (hybbx)     │
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

| Role | Purpose |
|------|---------|
| Gateway | IP edge → central |
| Digipeater | AX.25 digi → central |
| Repeater | RF relay → central |
| Link | P2P bridge → central |

Link/repeater edge daemons → `circuit_host` / `[circuit]`. Today: telnet + `packet_radio` HBX client; multi-site roles planned.

---

## Mail-Area

Mailbox on centralized daemon only (`data/mail/<user>/inbox/`). Edge daemons forward sessions; no local mail DB.

---

## Link/repeater edge daemons (planned)

Relay HBX/AX.25 to `[circuit]`. INI: `role=`, `circuit_host`, `link_*`. One central DB/mail authority.

---

## Link codes (planned)

Short token after successful edge→central LINK_AUTH handshake. Not for user login.

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
