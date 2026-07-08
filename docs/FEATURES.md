# Feature status

**v1.1.1** · [RELEASE-1.1.1.md](RELEASE-1.1.1.md) · INI: [MANUAL.md](MANUAL.md)

| Status | Meaning |
|--------|---------|
| **Verified** | Tested in release |
| **Built** | Shipped; verify in your deployment |
| **Partial** | Known limits |
| **Planned** | Not implemented |

## Sessions

| Feature | Status |
|---------|--------|
| Telnet `:2323` | **Verified** |
| Guest + registered auth | **Verified** |
| Mail, chat, conference | **Verified** |
| `/` commands | **Verified** |
| Flat-file storage | **Verified** |
| Traffic pacing 2400/80 | **Verified** |
| Texts (`text/*.txt`) | **Verified** |
| Text tokens `%time%`, `%date%` | **Verified** |
| SSH `:3232` | **Built** |
| WebSocket forward-proxy | **Built** |
| Browser terminal UI | **Built** |
| `max_connections` (WebSocket) | **Built** |
| One session per registered user | **Built** |
| SSH line editor | **Built** |
| TCP `/broadcast` | Partial |
| SQL storage | Planned |

## RF / HBX

| Feature | Status |
|---------|--------|
| HBX circuit hub `:7323` | **Built** |
| Packet radio / AX.25 | **Built** — [TNCS.md](TNCS.md); not live RF verified |
| BayCom PR-Stack (`baycom`) | **Built** (opt-in) — [BAYCOM.md](BAYCOM.md); `HYBBX_PLUGIN_BAYCOM=OFF` default |
| Multi-link hub | **Built** |
| ARDOP / CRDOP plugins | **Built** — [ARDOP.md](ARDOP.md), [CRDOP.md](CRDOP.md) |

## Build

CMake install, unit tests, GitHub Actions CI — **Built**.
