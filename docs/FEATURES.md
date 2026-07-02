# HyBBX feature list

**Version:** 0.6.0 — see [FEATURES.md](FEATURES.md).

Feature inventory — update when behavior changes. Operator INI: `share/hybbx.ini.example`, [MANUAL.md](MANUAL.md). Arch: [ROADMAP.md](ROADMAP.md).

| Status | Meaning |
|--------|---------|
| **Done** | Implemented and usable |
| **Partial** | Works with known limits |
| **Planned** | Designed or documented, not implemented |

---

## Core platform

| Feature | Status | Description |
|---------|--------|-------------|
| C99 core | Done | GCC + LLVM/Clang; Windows 10+, macOS X+, Linux/BSD, AmigaOS 3.9+ — see [PLATFORMS.md](PLATFORMS.md) |
| Plugin transports | Done | `hybbx_transport_plugin_t` registry; telnet and packet radio built in |
| INI configuration | Done | `-c` / `--config`; sections for service, storage, auth, traffic, transports |
| `[networks]` switches | Done | `ax25`, `websocket`, `circuit`; datacenter Main defaults: TCP/IP + HBX only |
| Service lifecycle | Done | Load config, start circuit hub and enabled transports, run until shutdown |
| Session model | Done | Per-connection BBS-inspired session; node limit (`max_online` / `nodes`) |
| 2400 baud traffic layer | Done | 80-column wrap, paced 8N1 output, optional ANSI; tuned for slow links |
| Boolean config standard | Done | Canonical `yes`/`no`; aliases `true`/`false`, `enable`/`disable`, `on`/`off`, `1`/`0` |
| Command routing | Done | `/` HyBBX commands; `;`/`#` comments ignored; other input local/mailbox (silent) |
| Input echo | Done | `[traffic] input_echo`; `/echo yes\|no` per session |
| Areas | Done | `main` (default), `mail` (personal inbox), `chat` (registered users) |
| Hardening | Done | Optional stack protector, FORTIFY, RELRO/PIE; bounded buffers in `limits.h` |

---

## Internal circuit (HBX)

| Feature | Status | Description |
|---------|--------|-------------|
| TCP-only core semantics | Done | Application never parses KISS/AX.25 on-air formats directly |
| Circuit hub | Done | `[circuit]` TCP hub IPv4+IPv6 (default port 7323) |
| HBX v1 framing | Done | Magic `HBX\x01`, proto, flags, length, payload |
| Protocol `ax25` | Done | Raw AX.25 frame (incl. FCS) RF ↔ core |
| Protocol `ax25_ui` | Done | UI payload with optional path metadata |
| Protocol `terminal` | Done | BBS byte stream for host-mode / UI traffic |
| G3RUH flag on uplink | Done | `HYBBX_CIRCUIT_FLAG_G3RUH_FSK` when 9600 FSK active |
| Reserved protos | Planned | `0x20` APRS, `0x21` NETROM, … |
| Packet radio link client | Done | Secondary connects via `circuit_host` / `circuit_port` |
| Main/secondary TCP bridge | Done | HBX over TCP; main template + `hybbx-secondary.ini.example` |
| Multi-link hub (N secondaries) | Planned | Several concurrent circuit links on one main |

---

## Transports (link adapters)

| Feature | Status | Description |
|---------|--------|-------------|
| Telnet (TCP) | Done | IPv4 + IPv6 dual-stack, port 2323; **static-enabled** |
| **hybbx-telnet** client | Done | Pure CLI telnet client — parameters/env only (`build/src/clients/hybbx-telnet`) |
| **hybbx-terminal** client | Done | Pure CLI AX.25 / HBX circuit terminal (`build/src/clients/hybbx-terminal`) |
| Link password auth | Done | HBX `LINK_AUTH` on circuit attach — **no** ping/pong health checks |
| Stale link removal | Done | Links with no password auth for `link_stale_days` (default 10) auto-removed |
| Auto-generated link codes | Done | Issued on successful Secondary→Main `LINK_AUTH` |
| Telnet dual-stack bind | Done | `ipv4`/`ipv6` toggles; `bind` / `bind6`; `IPV6_V6ONLY` |
| Packet radio | Done | AX.25 link adapter; `[networks] ax25 = yes` + `[transport.packet_radio]` |
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
| USB / serial devices | Done | POSIX `/dev/*`, Windows `COMx`, AmigaOS `serial.device` — see [PLATFORMS.md](PLATFORMS.md) |
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
| User registration | Done | `/register` (guests, no password); staff proof mail |
| Profile / password | Done | `/changeme` (registered users; old + new password) |
| Staff create user | Done | `/createuser` (Sysop, Admin) |
| Login / logout | Done | `/login`; session level updated |
| User levels | Done | Sysop, Admin, Mod, User, Guest (privilege rules enforced) |
| Staff user management | Done | `/activate`, `/promote`, `/demote`, `/delete` (Sysop, Admin) |
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
| `/clear` | Done | Clear screen and input line (`/cls`, `/reset`) |
| `/echo` | Done | Toggle typed-character echo |
| `/motd`, `/news` | Done | Text files from `[texts]` path |
| `/who` | Done | Online users |
| `/session` (`/info`) | Done | Current session details |
| `/version` (`/ver`) | Done | HyBBX version string |
| `/login`, `/register` | Done | Login; guest self-registration (no password) |
| `/changeme` | Done | Update own profile and password (registered users) |
| `/createuser` | Done | Sysop/Admin create pending user accounts |
| `/chat` | Done | List/join channels (registered users) |
| `/mail` | Done | Inbox list, read, delete, send (registered users) |
| `/leave` (`/back`) | Done | One level up in the area stack |
| `/main` (`/menu`) | Done | Return to main from any depth |
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
| 80-column output | Done | Wraps with `[traffic] line_width` (default 80) |

---

## Mail

| Feature | Status | Description |
|---------|--------|-------------|
| Flat-file inbox | Done | `data/mail/<user>/inbox/*.msg` on Main |
| Recycle bin | Done | `.../recycle/*.msg`; auto-purge after `recycle_days` (default 10) |
| `/mail list` | Done | Range `1-15` or `5-20` (newest first); `*` = unread |
| `/mail read` / `delete` | Done | By index or range (`delete 5-20` → recycle) |
| `/mail recycle` | Done | Permanently empty recycle bin |
| `/mail send` | Done | Multi-line compose; `/mail done` to deliver |
| Guest restriction | Done | Registered active users only |
| INI `[mail]` | Done | `enabled`, `max_messages`, `subject_max`, `body_max`, `recycle_days` |

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
| `hybbx_result_name()` | Done | Short `hybbx_result_t` name for logs/tests ([util.h](include/hybbx/util.h)) |
| Unit tests (`HYBBX_BUILD_TESTS`) | Done | `tests/test_util.c` — boolean parser + result names; CI on push/PR |
| `scripts/dev-setup.sh` | Done | Configure, build, `compile_commands.json` symlink |
| GPL-3.0 license | Done | See `LICENSE.txt`; third-party table in MANUAL |
| GitHub CI | Done | `.github/workflows/ci.yml` — build + tests on push/PR |

## Roadmap (not yet implemented)

**Architecture:** HyBBX uses a **datacenter-oriented Main** (TCP/IP + HBX by default) and **Secondary** instances for AX.25 and other adapters. Fully overrideable — Main can run all connection types locally. Details: [ROADMAP.md](ROADMAP.md).

| Feature | Status | Description |
|---------|--------|-------------|
| Mail-Area | Done | Mailbox on **main** only (not chat) |
| Main/secondary bridge | Done | HBX/TCP `LINK_AUTH`; templates in `share/` |
| Multi-link hub | Planned | Several secondaries → one main; `[transport.packet_radioN]` bridge registry on Main |
| Secondary modes | Partial | Secondary INI + `link_role` metadata; role routing planned |
| SSH transport | Planned | Same session core as telnet |
| WebSocket transport | Planned | Reverse-proxy only |
| SQL storage | Planned | SQLite, MySQL/MariaDB on core |
| HBX APRS / NETROM | Planned | Reserved protocol IDs on internal circuit |
| BayCom `ser12` path | Planned | Use `kissattach` + KISS until documented |

---

*Last aligned with codebase: HyBBX 0.6.0.*
