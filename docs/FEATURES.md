# Feature status

**Version:** 1.0.1 · Releases: [RELEASE-1.0.1.md](RELEASE-1.0.1.md), [RELEASE-1.0.0.md](RELEASE-1.0.0.md) · INI: [MANUAL.md](MANUAL.md)

| Status | Meaning |
|--------|---------|
| **Verified** | Tested for v1.0.0 release |
| **Built** | Shipped; not verified in live RF/service |
| **Partial** | Known limits |
| **Planned** | Not implemented |

---

## v1.0.0 scope (telnet-session release)

| Feature | Status | Notes |
|---------|--------|-------|
| Telnet `:2323` | **Verified** | Primary v1.0.0 delivery |
| Guest + registered auth | **Verified** | `/login`, `/register`, Sysop; one registered session per account |
| Mail, chat, conference | **Verified** | Via telnet |
| `/` command set | **Verified** | See [MANUAL.md](MANUAL.md#commands) |
| Flat-file storage | **Verified** | `backend=flatfile` |
| Traffic pacing 2400/80 | **Verified** | `[traffic]` |
| Texts (banner, motd, news, rules) | **Verified** | `text/*.txt` |
| HBX circuit hub | **Built** | Main `:7323`; field TBD |
| Packet radio / AX.25 | **Built** | Local tests only; **not live RF verified** |
| Multi-link hub | **Built** | `max_links`, bridge registry |
| ARDOP plugin | **Built** | External ARDOPC; local host-TCP script |
| CRDOP plugin | **Built** | External CRDOPC; local host-TCP script |
| TCP `/broadcast` | Partial | Log stub only |
| SSH transport | Built | libssh; `[networks] ssh=yes` |
| WebSocket transport | Built | Forward-proxy; `[networks] websocket=yes` |
| SQL storage | Planned | Post–v1.0.0 |

---

## Core

| Feature | Status |
|---------|--------|
| C99, GCC/Clang | Built |
| INI config `-c` | Verified |
| `[networks]` switches | Built |
| Plugin registry | Built |
| `max_online` / guest timeout | Verified |
| `security.log` + fail2ban examples | Built |
| Optional OpenSSL/libsodium crypto | Built |
| Hardening (`HYBBX_HARDENING`) | Built |

---

## HBX circuit

| Feature | Status |
|---------|--------|
| HBX v1 framing | Built |
| `terminal`, `ax25`, `ax25_ui`, `flow_ctrl` protos | Built |
| `LINK_AUTH` | Built |
| Load balance / `FLOW_CTRL` | Built |
| AX.25 broadcast (Main) | Built |
| Bridge registry (`transport.*N`) | Built |

---

## Transports

| Plugin | Status | External |
|--------|--------|----------|
| telnet | **Verified** | — |
| ssh | **Built** | libssh; transport-only wire auth |
| websocket | **Built** | RFC6455 forward-proxy |
| packet_radio | Built | TNC serial/KISS |
| ardop | Built | ARDOPC / ardopcf — [ARDOP.md](ARDOP.md) |
| crdop | Built | CRDOPC — [CRDOP.md](CRDOP.md) |

Clients: `hybbx-telnet`, `hybbx-terminal` — [CLIENTS.md](CLIENTS.md).

---

## Build

| Item | Status |
|------|--------|
| CMake install `<prefix>/hybbx/` | Built |
| Unit tests (`HYBBX_BUILD_TESTS`) | Built; CI |
| GitHub Actions build+test | Verified |

*Aligned with HyBBX 1.0.1.*
