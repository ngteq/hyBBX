# HyBBX feature list

**Version:** 0.1.0 (early development)

**Positioning:** HyBBX is a service solution for multiple connection types, **oriented on and inspired by** classic BBS/mailbox experiences. It is **not** intended as a new BBS or mailbox replacement — rather as a link-agnostic platform (telnet, packet radio, …) that reuses familiar session patterns.

Living inventory of functional and important HyBBX capabilities. **Update this file whenever a feature is added, removed, or materially changed.** Operator details: [MANUAL.md](MANUAL.md). Quick start: [QUICKSTART.md](QUICKSTART.md). Doc index: [INDEX.md](INDEX.md).

| Status | Meaning |
|--------|---------|
| **Done** | Implemented and usable |
| **Partial** | Works with known limits |
| **Planned** | Designed or documented, not implemented |

---

## Core platform

| Feature | Status | Description |
|---------|--------|-------------|
| C99 core | Done | Plain C, CMake build, POSIX (Linux/BSD); AmigaOS cross-build toolchain |
| Plugin transports | Done | `hybbx_transport_plugin_t` registry; telnet and packet radio built in |
| INI configuration | Done | `-c` / `--config`; sections for service, storage, auth, traffic, transports |
| Service lifecycle | Done | Load config, start circuit hub and enabled transports, run until shutdown |
| Session model | Done | Per-connection BBS-inspired session; node limit (`max_online` / `nodes`) |
| 2400 baud traffic layer | Done | 40-column wrap, paced 8N1 output, optional ANSI; tuned for slow links |
| Boolean config standard | Done | Canonical `yes`/`no`; aliases `true`/`false`, `enable`/`disable`, `on`/`off`, `1`/`0` |
| Command routing | Done | `/` HyBBX commands; `;`/`#` comments ignored; other input local/mailbox (silent) |
| Areas | Done | `main` (default), `mail` (local), `chat` (registered users) |
| Hardening | Done | Optional stack protector, FORTIFY, RELRO/PIE; bounded buffers in `limits.h` |

---

## Internal circuit (HBX)

| Feature | Status | Description |
|---------|--------|-------------|
| TCP-only core semantics | Done | Application never parses KISS/AX.25 on-air formats directly |
| Circuit hub | Done | `[circuit]` TCP listen on IPv4+IPv6 loopback (default port 7323) |
| HBX v1 framing | Done | Magic `HBX\x01`, proto, flags, length, payload |
| Protocol `ax25` | Done | Raw AX.25 frame (incl. FCS) RF ↔ core |
| Protocol `ax25_ui` | Done | UI payload with optional path metadata |
| Protocol `terminal` | Done | BBS byte stream for host-mode / UI traffic |
| G3RUH flag on uplink | Done | `HYBBX_CIRCUIT_FLAG_G3RUH_FSK` when 9600 FSK active |
| Reserved protos | Planned | `0x20` APRS, `0x21` NETROM, … |
| Packet radio link client | Done | Connects to hub via `circuit_host` / `circuit_port` |

---

## Transports (link adapters)

| Feature | Status | Description |
|---------|--------|-------------|
| Telnet (TCP) | Done | IPv4 + IPv6, default port 2323; minimal RFC telnet (ECHO, SGA) |
| Telnet dual-stack bind | Done | `bind`, `bind6`, per-family `ipv4`/`ipv6` toggles |
| Packet radio | Done | AX.25 link adapter over internal HBX/TCP circuit |
| SSH | Planned | Same session core as telnet |
| WebSocket | Planned | Local endpoint behind reverse-proxy only |

---

## Packet radio & AX.25

| Feature | Status | Description |
|---------|--------|-------------|
| AX.25 UI layer | Done | Addresses, path, CRC-CCITT, UI encode/decode |
| KISS protocol | Done | Framing, parameters (TXDELAY, PERSIST, slot, TXTAIL, full-duplex) |
| TNC2 host mode | Done | Command prompt, connected converse, MYCALL/TXDELAY/FULLDUP |
| 6PACK protocol | Done | Alternative serial framing (DF6BU) |
| TNC profile: TNC2C | Done | Landolt TNC2C; primary test device |
| TNC profile: BayCom | Done | TCM3105 AFSK @ 1200; CB-oriented defaults |
| TNC profile: PC-COM | Done | Albrecht CB modem; host mode @ 2400 baud |
| TNC profile: generic | Done | User-supplied parameters for any KISS/TNC2 TNC |
| USB / serial devices | Done | `device_type` usb/serial; configurable host `baud` |
| Radio band `cb` / `amateur` | Done | Sets safe defaults; CB forces half-duplex |
| Duplex half / full | Done | KISS full-duplex param + TNC `FULLDUP`; TX guard after RX in half-duplex |
| AFSK 1200 (TCM3105) | Done | Default over-the-air modulation |
| G3RUH FSK 9600 | Done | `g3ruh_fsk=yes`; modem/radio/timing + TNC `MODEM 9600` / `HB 9600` |
| AX.25 addressing | Done | `mycall`, `dest`, `via` digipeater list |
| Host connect on start | Done | Auto `C <dest>` in TNC2 host mode when configured |
| Raw + UI callbacks | Done | TNC delivers full frames and parsed UI to packet_radio plugin |

---

## Authentication & users

| Feature | Status | Description |
|---------|--------|-------------|
| Guest auto-login | Done | `Guest1`… prefix; optional timeout disconnect |
| User registration | Done | `/register`; inactive until staff `/activate` |
| Login / logout | Done | `/login`; session level updated |
| User levels | Done | Sysop, Admin, Mod, User, Guest (privilege rules enforced) |
| Staff user management | Done | `/activate`, `/promote`, `/demote`, `/delete` |
| Self-delete | Done | `/deleteme yes` (all boolean true aliases) |
| Username validation | Done | 4–12 chars, lowercase + limited digits/separators |
| Password formats | Done | `{sha256}`, legacy `{md5}`, plain auto-upgrade |
| Default Sysop bootstrap | Done | `Sysop`/`Sysop` created if no users exist |

---

## Storage

| Feature | Status | Description |
|---------|--------|-------------|
| Flat-file backend | Done | `backend=flatfile`; path-configurable data dir |
| INI user shards | Done | Up to 50 users per `users/usersN.ini` |
| Legacy `users.dat` migration | Done | Pipe-format migrated on first startup |
| Session / ID counters | Done | `sessions.dat`, `guest.next`, `user.next`, `session.next` |
| SQLite backend | Planned | |
| MySQL / MariaDB | Planned | |

---

## Commands (runtime)

| Command | Status | Description |
|---------|--------|-------------|
| `/help` | Done | Level-aware list and per-command help |
| `/motd`, `/news` | Done | Text files from `[texts]` path |
| `/who` | Done | Online users |
| `/session` (`/info`) | Done | Current session details |
| `/version` (`/ver`) | Done | HyBBX version string |
| `/login`, `/register` | Done | Authentication and signup |
| `/chat` | Done | List/join channels (registered users) |
| `/leave` | Done | Return to main area from chat |
| `/activate`, `/promote`, `/demote`, `/delete` | Done | Staff administration |
| `/deleteme` | Done | Delete own account (confirmed) |
| `/exit` (`/quit`, `/bye`, `/logout`) | Done | Disconnect |

---

## Chat

| Feature | Status | Description |
|---------|--------|-------------|
| Multi-channel chat | Done | Configurable channel count and names |
| Join by number or name | Done | `/chat <n>` or `/chat <name>` |
| Message length limit | Done | `message_max` (default 72) |
| 40-column chat output | Done | Wraps with traffic profile |

---

## Text files & BBS-inspired content

| Feature | Status | Description |
|---------|--------|-------------|
| `banner.txt` | Done | Shown at connect; tokens `@version@`, `@service@` |
| `motd.txt` | Done | After guest login; `/motd` |
| `news.txt` | Done | `/news` |

---

## Cryptography

| Feature | Status | Description |
|---------|--------|-------------|
| SHA-256 passwords | Done | Default `{sha256}` via tinysha256; optional OpenSSL |
| AES-256-GCM | Done | tinyaes bundled; optional OpenSSL |
| XChaCha20-Poly1305 | Done | Monocypher bundled; optional libsodium |
| X25519 sealed boxes | Done | ECIES-style sealed messages |
| Configurable crypto backends | Done | `[crypto]` section; per-algorithm backend selection |
| System random | Done | Default; optional OpenSSL RAND |

---

## Build & deployment

| Feature | Status | Description |
|---------|--------|-------------|
| CMake install layout | Done | `bin/hybbx`, `bin/hybbx-start`, `etc/hybbx.ini`, `text/`, `data/` |
| Dev helper script | Done | `scripts/hybbx.sh` → `local/hybbx.ini` |
| Optional OpenSSL | Done | `-DHYBBX_CRYPTO_OPENSSL=ON` |
| Optional libsodium | Done | `-DHYBBX_CRYPTO_LIBSODIUM=ON` |
| Plugin build toggles | Done | Telnet / packet_radio on by default |
| GPL-3.0 license | Done | See `LICENSE.txt`; third-party table in MANUAL |

---

## Roadmap (not yet implemented)

- SSH transport plugin
- WebSocket transport (reverse-proxy only)
- SQL storage backends (SQLite, MySQL/MariaDB)
- Additional HBX protocols (APRS, NETROM)
- True BayCom `ser12` kernel path (use `kissattach` + KISS today)

---

*Last aligned with codebase: HyBBX 0.1.0 — telnet, packet radio (TNC2C/BayCom/PC-COM), HBX circuit, G3RUH FSK switch, HyBBX boolean standard.*
