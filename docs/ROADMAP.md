# Roadmap

Current status: [FEATURES.md](FEATURES.md). Topology: [TOPOLOGY.md](TOPOLOGY.md).

## Priority

| Item | Notes |
|------|-------|
| v1.5.0 field testing | Mesh stubs, proxymail delivery, live RF |
| AX.25 field validation | Secondary→Main HBX — live RF TBD |
| ARDOP / CRDOP live modem tests | External ARDOPC/CRDOPC |
| mains_proxy live relay | HBX circuit mesh I/O |
| `/proxymail` delivery | Inter-Main mail over mesh |
| `/broadcast` local announce | **Built** — Sysop, all online sessions on Main |
| Circuit reconnect / heartbeat | Planned |
| `link_role` routing | Metadata today; semantics later |

## Later

| Item | Notes |
|------|-------|
| MySQL/MariaDB storage | v2.0.0 |
| WebSocket line editing | Browser UI |
| User-files / public-files areas | Planned |
| HBX APRS / NETROM protos | Reserved IDs |
| Content moderation design | Non-social / illegal behaviour — sysop tools, reports, policy; see [SECURITY.md](SECURITY.md) |
| Abuse flood call sites | Wire `hybbx_security_ban_abuse_report` for chat/register spam |
| Register/guest abuse rules | `[security]` extension |
