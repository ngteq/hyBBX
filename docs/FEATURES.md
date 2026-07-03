# HyBBX feature list

**Version:** 0.8.0 · Config: `share/hybbx.ini.example` · Operator: [MANUAL.md](MANUAL.md) · Plan: [ROADMAP.md](ROADMAP.md)

C99 multi-transport session daemon: mail, chat, `/` commands over plugin link adapters (telnet, AX.25/HBX bridge).

| Status | Meaning |
|--------|---------|
| **Done** | Shipped and usable |
| **Partial** | Works with known limits |
| **Planned** | Documented, not implemented |

---

## Core platform

| Feature | Status | Description |
|---------|--------|-------------|
| C99 core | Done | GCC + LLVM/Clang; Windows 10+, macOS X+, Linux/BSD, AmigaOS 3.9+ — see [PLATFORMS.md](PLATFORMS.md) |
| Plugin transports | Done | `hybbx_transport_plugin_t` registry; telnet and packet radio built in |
| INI configuration | Done | `-c` / `--config`; sections for service, storage, auth, traffic, transports |
| `[networks]` switches | Done | `ax25`, `websocket`, `circuit`; datacenter Main defaults: TCP/IP + HBX only |
| Service lifecycle | Done | Load config, start circuit hub and enabled transports, run until shutdown |
| Sysop `/shutdown` / `/restart` | Done | Stop daemon or re-exec with same `-c` config |
| `security.log` | Done | fail2ban-compatible audit log in `[log] dir` (login/link auth failures, Sysop actions) |
| fail2ban examples | Done | `share/fail2ban/` — telnet, circuit; SSH/WebSocket stubs for after v1.0.0 |
| Session model | Done | Per-connection line-oriented session; node limit (`max_online` / `nodes`) |
| 2400 baud traffic layer | Done | 80-column wrap, paced 8N1 output, optional ANSI; tuned for slow links |
| Low-bandwidth session design | Done | Text-only wire format; multi-transport binding; minimal payload overhead |
| Boolean config standard | Done | Canonical `yes`/`no`; aliases `true`/`false`, `enable`/`disable`, `on`/`off`, `1`/`0` |
| Command routing | Done | `/` HyBBX commands; `;`/`#` comments ignored; other input local/mailbox (silent) |
| Input echo | Done | Off by default; `[traffic] input_echo` or `/echo yes\|no` per session |
| Areas | Done | `main` (default), `mail` (personal inbox), `chat` (registered users), `conference` (2-user invite) |
| Hardening | Done | Optional stack protector, FORTIFY, RELRO/PIE; bounded buffers in `limits.h` |

---

## Internal circuit (HBX)

**Secondaries** are **separate remote machines** — extenders, repeaters, gateways, and other next-hop edge devices that bridge RF or other transport **to** Main over HBX/TCP (`circuit_host` → Main `[circuit]` hub). Each runs its own HyBBX process; Main holds the session core. They are **infrastructure**, not end-user telnet sessions. **Not** Secondaries: telnet users, local `[transport.*]` on Main, or any adapter running inside the Main process. **v0.8.0:** multiple Secondaries may connect to one Main at once (unique `link_id` per active link). `link_role` (`link`, `repeater`, `gateway`, `digipeater`, …) is per-bridge metadata; all are the same **Secondary** class relative to Main.

| Feature | Status | Description |
|---------|--------|-------------|
| TCP-only core semantics | Done | Application never parses KISS/AX.25 on-air formats directly |
| Circuit hub | Done | `[circuit]` TCP hub IPv4+IPv6; `max_links` (default 8, max 16) |
| HBX v1 framing | Done | Magic `HBX\x01`, proto, flags, length, payload |
| Protocol `ax25` | Done | Raw AX.25 frame (incl. FCS) RF ↔ core |
| Protocol `ax25_ui` | Done | UI payload with optional path metadata |
| Protocol `terminal` | Done | Terminal byte stream for host-mode / UI traffic |
| Protocol `flow_ctrl` | Done | Load-balance pause / break / cancel / resume (`0x05`) |
| Auto load-balancing | Done | Pace downlink to low-bandwidth links; `LINK_AUTH` QoS (`baud`, `duplex`, `bandwidth`) |
| Bandwidth user policy | Done | AX.25 users before full-duplex TCP; LIFO within class; circuit secondaries spared until users exhausted |
| AX.25 broadcast (Main) | Done | `/broadcast ax25` — QST UI; low+half-duplex QoS only; MHz not channel numbers |
| TCP broadcast | Stub | `/broadcast tcp` — logged only until telnet fan-out |
| G3RUH flag on uplink | Done | `HYBBX_CIRCUIT_FLAG_G3RUH_FSK` when 9600 FSK active |
| Reserved protos | Planned | `0x20` APRS, `0x21` NETROM, … |
| Packet radio link client | Done | Secondary connects via `circuit_host` / `circuit_port` |
| Main/secondary TCP bridge | Done | HBX over TCP; main template + `hybbx-secondary.ini.example` |
| Multi-link hub (N secondaries) | Done | Per-link slot table; same MHz allowed; multicast broadcast; duplicate `link_id` rejected |

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
| SSH | After v1.0.0 | Same session core as telnet |
| WebSocket | After v1.0.0 | Local endpoint behind reverse-proxy only |

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
| Guest auto-login | Done | `Guest1`…`Guest25`; max 25 simultaneous; optional timeout disconnect |
| Guest sessions | Done | Ephemeral; auto-login on connect only (not via `/login`) |
| User registration | Done | `/register` (guests, password required); staff proof mail |
| Profile / password | Done | `/changeme` (own); `/userchange` (staff overwrite, 8–24 char password) |
| Staff create user | Done | `/createuser` (Sysop, Admin) |
| Login / logout | Done | `/login`; session level updated |
| User levels | Done | Sysop, Admin, Mod, User, Guest (privilege rules enforced) |
| Staff user management | Done | `/activate`, `/promote`, `/demote`, `/delete`, `/userdelete` (Sysop) |
| Self-delete | Done | `/deleteme yes\|no` (all boolean true/false aliases) |
| Username validation | Done | 4–12 chars, lowercase + limited digits/separators |
| Password formats | Done | `{sha256}`, legacy `{md5}`, plain auto-upgrade |
| Default Sysop bootstrap | Done | `Sysop` / `SysopPassword` created if no users exist |

---

## Storage

| Feature | Status | Description |
|---------|--------|-------------|
| Flat-file backend | Done | `backend=flatfile`; path-configurable data dir |
| INI user shards | Done | Up to 50 users per `users/usersN.ini` |
| Legacy `users.dat` migration | Done | Pipe-format migrated on first startup |
| Session / ID counters | Done | `sessions.dat`, `user.next`, `session.next` |
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
| `/rules` (`/legal`) | Done | Legal notice and acceptable use (`rules.txt`; all users) |
| `/who` | Done | Online users and connection type; alias `/online` |
| `/users` | Done | Registered user level breakdown (percentages sum to 100%) |
| `/session` (`/info`) | Done | Current session details |
| `/version` (`/ver`) | Done | HyBBX version and host OS name |
| `/login`, `/register` | Done | Login; guest self-registration (password required) |
| `/changeme` | Done | Update own profile and password (registered users) |
| `/userchange` | Done | Staff overwrite profile and password (Sysop/Admin) |
| `/userdelete` | Done | Sysop delete any account except Sysop (not self) |
| `/createuser` | Done | Sysop/Admin create pending user accounts |
| `/chat` | Done | List/join; `/chat show`, `/chat showall` (registered users) |
| `/conference` (`/meeting`) | Done | Invite: `/conference <topic> <user>`; accept y/n; 2/30 min limit |
| `/mail` | Done | Inbox list, read, delete, send (registered users) |
| `/leave` (`/back`) | Done | One level up in the area stack |
| `/main` (`/menu`) | Done | Return to main from any depth |
| `/activate`, `/promote`, `/demote`, `/delete` | Done | Staff administration |
| `/shutdown`, `/restart` | Done | Sysop stop or re-exec the daemon |
| `/deleteme` | Done | Delete own account (confirmed) |
| `/exit` (`/quit`, `/bye`, `/logout`) | Done | Disconnect |

---

## Chat

| Feature | Status | Description |
|---------|--------|-------------|
| Multi-channel chat | Done | Configurable channel count and names |
| Join by number or name | Done | `/chat <n>` or `/chat <name>` |
| `/chat show` / `showall` | Done | Current channel / all channels (80-col wrap) |
| Conference (`/meeting`) | Done | 2-user invite channel; y/n accept; 2/30 min rate limit |
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

## Text files

| Feature | Status | Description |
|---------|--------|-------------|
| `banner.txt` | Done | Shown at connect; tokens `@version@`, `@service@` |
| `motd.txt` | Done | `/motd` only |
| `news.txt` | Done | `/news` only |
| `rules.txt` | Done | `/rules` / `/legal` — legal notice and acceptable use |

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
| CMake install layout | Done | `<prefix>/hybbx/` — `hybbx`, `hybbx-start`, `hybbx.ini`, `text/`, `data/`, `logs/` |
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

See [ROADMAP.md](ROADMAP.md). Summary:

| Feature | Status | Description |
|---------|--------|-------------|
| Mail-Area | Done | Mailbox on **main** only (not chat) |
| Main/secondary bridge | Done | HBX/TCP `LINK_AUTH`; templates in `share/` |
| Multi-link hub | Done | Several remote secondaries → one Main; `[transport.packet_radioN]` bridge registry on Main |
| Secondary modes | Partial | Secondary INI + `link_role` metadata; role routing planned |
| SSH transport | After v1.0.0 | Same session core as telnet |
| WebSocket transport | After v1.0.0 | Reverse-proxy only |
| User-files area | After SSH/WebSocket | Per-user documentation/file area on Main (post–v1.0.0) |
| Public-files area | After SSH/WebSocket | Shared public documentation library on Main (post–v1.0.0) |
| SQL storage | Planned | SQLite, MySQL/MariaDB on core |
| HBX APRS / NETROM | Planned | Reserved protocol IDs on internal circuit |
| BayCom `ser12` path | Planned | Use `kissattach` + KISS until documented |

---

*Last aligned with codebase: HyBBX 0.8.0.*
