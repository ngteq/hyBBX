# HyBBX roadmap

**v0.9.0** — first **testing release** before **v1.0.0**. From 0.9.0 onward the feature set is **frozen**: no new user-visible features until v1.0.0 — only fixes, documentation, and operator/integration testing.

**v1.0.0** — first public GitHub release. Per-version `docs/RELEASE-*` starts at v1.0.0.

**Post–v1.0.0:** SSH, WebSocket transports → then user-files, public-files areas.

## Release line

| Version | Role |
|---------|------|
| **0.8.x** | Multi-link hub, ARDOP/CRDOP host plugins shipped |
| **0.9.x** | Testing release — validate topology, INI, bridges before v1.0.0; **feature freeze** |
| **1.0.0** | Public release + SQL storage scope + integration-test push |

## Topology

**Secondaries** are **separate remote machines** — extenders, repeaters, gateways, and other next-hop edge devices that bridge RF or other transport **to** Main over HBX/TCP. Each runs its own HyBBX process (`circuit_host` → Main `[circuit]`); Main holds the session core. They are **infrastructure**, not telnet users or local `[transport.*]` on Main. `link_role` (`link`, `repeater`, `gateway`, `digipeater`, …) is per-bridge metadata; all are the same **Secondary** class relative to Main.

| | Main (default) | Secondary (default) |
|---|----------------|---------------------|
| Role | Users, storage, telnet, HBX hub | Remote edge host: TNC/RF, outbound HBX client to Main |
| `ax25` | `no` | `yes` |
| `circuit` | `yes` (hub, loopback) | `no` (`circuit_host` → Main) |
| Wire | TCP telnet (users) + HBX :7323 (Secondaries) | AX.25 → HBX/TCP → Main |

Templates: `share/hybbx.ini.example`, `share/hybbx-secondary.ini.example`. Standalone Main: `ax25=yes` locally, no remote Secondary.

**v0.8.0:** multiple remote Secondaries connected simultaneously to one Main (unique `link_id` per active link).

```
  Secondary (remote host)      Main
  ax25=yes ──HBX/TCP:7323──►  circuit hub, telnet :2323 (users), mail, storage
```

**Done:** HBX v1 (`circuit.c`), hub IPv4+IPv6, `LINK_AUTH`, link registry, AX.25↔session bridge, `hybbx-terminal`, auto load-balancing (`circuit_balance.c`, `FLOW_CTRL`), **multi-link hub** (N concurrent secondaries, `max_links`, bridge registry).

**v0.8.0:** Up to `HYBBX_CIRCUIT_MAX_LINKS` (16) concurrent TCP circuit links; one session per `link_id`. Same MHz on multiple secondaries allowed — HyBBX-QoS governs broadcast fan-out and load-balance. Under AX.25 pressure, telnet/RF **users** are paused/disconnected (LIFO) before a secondary link is cancelled.

Bridge registry: `[transport.packet_radioN]` on Main matches each remote Secondary's section (`link_id`, `link_password`, `link_role`). `link_role` labels bridge type; all entries are Secondary class relative to Main. VPN/firewall/proxy: operator OS layer.

| `link_role` | Use (all are Secondary class relative to Main) |
|-------------|-----|
| `link` | Default RF bridge |
| `gateway` | IP/RF access path |
| `digipeater` | AX.25 via relay |
| `repeater` | RF range extension |

## Multi-link (v0.8.0 — done)

| Phase | Scope |
|-------|--------|
| 0 | Single link — **Done** (pre-0.8.0) |
| 1 | N concurrent TCP links; one session per `link_id` — **Done** |
| 2 | `link_role` routing semantics — deferred (post–v1.0.0) |
| 3 | Fan-out to all links (broadcast AX.25 by MHz + QoS) — **Done** |
| 4 | Reconnect policy, optional heartbeat — deferred (post–v1.0.0) |

Per-link table in `circuit_tcp.c`; `max_links` in `[circuit]`; bridge registry from `[transport.packet_radioN]`, `[transport.ardopN]`, and `[transport.crdopN]`; Main storage authoritative.

## ARDOP and CRDOP (separate plugins)

**ARDOP** and **CRDOP** are **different protocols** with **standalone plugin source trees** (`plugins/ardop/`, `plugins/crdop/`). Each has its own transport kind, INI section, external modem, and documentation ([ARDOP.md](ARDOP.md), [CRDOP.md](CRDOP.md)). They will diverge further after v1.0.0. Today both may link `hybbx_ardop_common` for host-TCP wiring — an implementation detail, not a merged product.

### ARDOP (`plugins/ardop/`)

| Item | Status | Notes |
|------|--------|-------|
| External ARDOPC | Done | Operator runs g8bpq/ardop or MIT ardopcf separately; see [ARDOP.md](ARDOP.md) |
| ARQ host subset | Done | Init, listen, connect, `d:ARQ` data, CRC + RDY |
| HBX bridge | Done | `terminal` proto on circuit; parallel `link_id` to AX.25 Secondary |
| `radio_profile=cb` | Preview | Amateur plugin hint only — use **`crdop`** for CB |
| Live RF verification | **After v1.0.0** | Local mock host-TCP scripts only until post-release integration |
| FEC / OFDM / CAT | Deferred | Not required for first bridge |

### CRDOP (`plugins/crdop/`)

**CRDOP** modem: **[github.com/ngteq/CRDOP](https://github.com/ngteq/CRDOP)** **1.0.0** (MIT). HyBBX **`crdop`** plugin = host TCP bridge only.

| Item | Status | Notes |
|------|--------|-------|
| External CRDOPC | Done | [ngteq/CRDOP](https://github.com/ngteq/CRDOP) `crdopc` on **8515/8516** |
| HyBBX `transport.crdop` | Partial | CB bridge defaults; **not RF-tested pre–v1.0.0** |
| Global CB profiles | Done | `crdop` plugin; modem profiles in CRDOP INI |
| Live RF verification | **After v1.0.0** | After AX.25 integration tests |

## v1.0.0 scope

| Item | Notes |
|------|-------|
| Multi-link hub | Done (v0.8.0) |
| SQL storage | Main only |
| HBX APRS/NETROM | Reserved proto IDs |
| BayCom `ser12` | Via `kissattach` + KISS until documented |

Mail on Main only (`data/mail/<user>/inbox/`). Link codes: issued on `LINK_AUTH_ACK` → `data/links/`.

## Verification

**v0.9.x (testing release):** operators and contributors exercise real deployments; CI runs **build + unit tests** only (`ctest`). No automated **AX.25** RF integration tests and no **ARDOP/CRDOP** end-to-end tests against live modems in CI. Optional local scripts (`scripts/test-ardop-plugin.sh`, `scripts/test-crdop-plugin.sh`) use `scripts/mock-ardopc.py` to exercise host-TCP wiring only — not RF.

**After v1.0.0 (first integration push):** **AX.25** tests (TNC, Secondary→Main HBX, packet_radio) take priority. **ARDOP/CRDOP** live-modem verification follows in a later milestone.

## After v1.0.0

| Item | Notes |
|------|-------|
| SSH transport | Same session core as telnet |
| WebSocket transport | Local endpoint; reverse-proxy TLS |
| `link_role` routing | Phase 2 multi-link semantics |
| Circuit reconnect / heartbeat | Phase 4 |

## After SSH/WebSocket

| Item | Notes |
|------|-------|
| User-files | Per-user docs/uploads on Main |
| Public-files | Shared read-only library on Main |

Details: [FEATURES.md](FEATURES.md#roadmap-not-yet-implemented) · Operator: [MANUAL.md](MANUAL.md)
