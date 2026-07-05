# Roadmap

**v1.0.0 shipped** — telnet-session release ([RELEASE-1.0.0.md](RELEASE-1.0.0.md)).

## After v1.0.0 (priority)

| Item | Notes |
|------|-------|
| AX.25 field validation | Secondary→Main HBX, packet_radio — local tests exist; live RF TBD |
| ARDOP / CRDOP live modem tests | Plugins built; external ARDOPC/CRDOPC |
| SQL storage | SQLite / MySQL on Main |
| `link_role` routing | Metadata today; semantics later |
| Circuit reconnect / heartbeat | Planned |
| TCP `/broadcast` fan-out | Stub today |
| WebSocket transport | Behind reverse proxy |

## Later

| Item | Notes |
|------|-------|
| User-files / public-files areas | After SSH/WebSocket |
| HBX APRS / NETROM protos | Reserved IDs |
| BayCom `ser12` | Via KISS until native path |

Topology reference: [MANUAL.md — Topology](MANUAL.md#topology).

Feature table: [FEATURES.md](FEATURES.md).
