# HyBBX roadmap

**0.7.x** → **v1.0.0** (first GitHub release). Per-version `docs/RELEASE-*` starts at v1.0.0.

**Post–v1.0.0:** SSH, WebSocket transports → then user-files, public-files areas.

## Topology

| | Main (default) | Secondary (default) |
|---|----------------|---------------------|
| Role | Users, storage, telnet, HBX hub | TNC/RF, HBX client |
| `ax25` | `no` | `yes` |
| `circuit` | `yes` (hub, loopback) | `no` (`circuit_host` → Main) |
| Wire | TCP telnet + HBX :7323 | AX.25 → HBX/TCP → Main |

Templates: `share/hybbx.ini.example`, `share/hybbx-secondary.ini.example`. Standalone Main: `ax25=yes` locally, no Secondary.

```
  Secondary                    Main
  ax25=yes ──HBX/TCP:7323──►  circuit hub, telnet :2323, mail, storage
```

**Done:** HBX v1 (`circuit.c`), hub IPv4+IPv6, `LINK_AUTH`, link registry, AX.25↔session bridge, `hybbx-terminal`.

**Limit:** one active circuit link per Main (multi-link planned below).

Bridge registry: `[transport.packet_radioN]` on Main matches Secondary section (`link_id`, `link_password`, `link_role`). VPN/firewall/proxy: operator OS layer.

| `link_role` | Use |
|-------------|-----|
| `link` | Default RF bridge |
| `gateway` | IP/RF access path |
| `digipeater` | AX.25 via relay |
| `repeater` | RF range extension |

## Multi-link (planned)

| Phase | Scope |
|-------|--------|
| 0 | Single link (today) — **Done** |
| 1 | N concurrent TCP links; one session per `link_id` |
| 2 | `link_role` routing semantics |
| 3 | Optional fan-out to all links |
| 4 | Reconnect policy, optional heartbeat |

Draft: per-link table in `circuit_tcp.c`; `max_links` in `[circuit]`; Main storage authoritative.

## v1.0.0 scope

| Item | Notes |
|------|-------|
| Multi-link hub | See above |
| SQL storage | Main only |
| HBX APRS/NETROM | Reserved proto IDs |
| BayCom `ser12` | Via `kissattach` + KISS until documented |

Mail on Main only (`data/mail/<user>/inbox/`). Link codes: issued on `LINK_AUTH_ACK` → `data/links/`.

## After v1.0.0

| Item | Notes |
|------|-------|
| SSH transport | Same session core as telnet |
| WebSocket transport | Local endpoint; reverse-proxy TLS |

## After SSH/WebSocket

| Item | Notes |
|------|-------|
| User-files | Per-user docs/uploads on Main |
| Public-files | Shared read-only library on Main |

Details: [FEATURES.md](FEATURES.md#roadmap-not-yet-implemented) · Operator: [MANUAL.md](MANUAL.md)
