# Operator manual

**v2.0.0** — telnet, SSH, WebSocket. Templates: `share/hybbx.ini.example`, `share/hybbx-secondary.ini.example`.

Booleans: `yes`/`no` (+ `true`/`false`, `on`/`off`, `1`/`0`).

---

## Topology

Full guide: [TOPOLOGY.md](TOPOLOGY.md).

| Role | Purpose |
|------|---------|
| **Main** | Users, storage, HBX hub `:7323`, optional `mains_proxy` |
| **Secondary** | RF edge — HBX client to Main; no public logins |
| **mains_proxy** | Main ↔ Main mesh (not active yet); HBX circuit peers only |

Inter-node traffic (Secondary, RF plugins, mesh) uses **HBX/Circuit** only. User sessions use telnet, SSH, or WebSocket on Main.

---

## INI sections

### `[service]`

| Key | Default | Description |
|-----|---------|-------------|
| `name` | `hyBBX` | Service name in banners |
| `max_online` / `nodes` | `35` | Session limit |
| `prompt` | empty | Input prompt (e.g. `hybbx>`) |

### `[time]`

| Key | Default | Description |
|-----|---------|-------------|
| `clock` | `24h` | `24h` \| `12h` \| `am_pm` |
| `seconds` | `yes` | Append `:SS` to `%time%` and log stamps when `yes` |
| `date` | `iso` | `iso` (`YYYY/MM/DD`) \| `iso_short` (`YY/MM/DD`) \| `us` (`MM/DD/YYYY`) \| `eu` (`DD/MM/YYYY`) |

### `[log]`

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | File logging (`yyyymmdd-hybbx.log`) |
| `dir` | `logs` | Log directory under install root |
| `level` | `warn` | `debug` \| `info` \| `stats` \| `warn` |

`security.log` is always written to the same directory (independent of `enabled`).

### `[security]`

Built-in protection for **network abuse** and **excessive spam** — one subsystem (`security.log`, `src/core/security_ban.c`). See [SECURITY.md](SECURITY.md).

**Policy:** short cool-down bans (default 10 minutes). **Normal** chat/mail traffic uses soft limits in `[traffic]` / `[chat]` / `[mail]` — no bans. **Bans** apply to brute-force, connection flood, and repeated abuse above `abuse_maxretry`.

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | Master switch |
| `maxretry` | `5` | Login / link-auth failures before ban |
| `findtime` | `600` | Login failure window (seconds) |
| `bantime` | `600` | Ban duration (seconds) |
| `abuse_maxretry` | `30` | Excessive abuse events before ban |
| `abuse_findtime` | `600` | Abuse event window (seconds) |
| `telnet` | `yes` | Track telnet login failures |
| `ssh` | `yes` | Track SSH login failures |
| `websocket` | `yes` | Track WebSocket login failures |
| `circuit` | `yes` | Track HBX `link_auth` failures |
| `rate_limit` | `30` | Max new connections per IP per window |
| `rate_window` | `60` | Connection flood window (seconds) |
| `ban_backend` | `internal` | `internal` \| `log` \| `iptables` \| `nftables` \| `hosts` |
| `ban_callid` | *(empty)* | Comma-separated AX.25 callsigns or HBX `link_id` values (permanent until removed from INI) |

Events: `login_fail`, `link_auth_fail`, `rate_limit`, `abuse:*` (hook for excessive flood — not normal messages). **CALLID bans** apply to AX.25 source addresses and circuit `link_id`; logged as `ban callid=…` (internal only — no firewall rule). Optional `share/fail2ban/` for site-wide firewall integration.

### `[storage]`

Default **`backend = flatfile`** — no host packages; works out of the box. Built-in crypto (see `[crypto]`) matches this: zero extra dependencies.

**Recommended when available:** `backend = sqlite` (requires libsqlite3 at build time) for better query performance and automatic DB backups. Set `HYBBX_STORAGE_SQLITE=ON` (default) and install `libsqlite3-dev` (or distro equivalent) before building.

MySQL/MariaDB backends are planned for **v2.0.0** only.

| Key | Default | Description |
|-----|---------|-------------|
| `backend` | `flatfile` | `flatfile` or `sqlite` |
| `path` | `data` | Data root (`users/` for flatfile) |
| `user_db` | `users.db` | SQLite users/sessions file (under `path`) |
| `mail_db` | `mail.db` | SQLite mail file (under `path`) |
| `backup_interval` | `300` | Seconds between SQLite DB copies |
| `backup_path` | *(empty)* | Backup directory; empty = same dir as DB, `.flb` suffix |

SQLite creates `user_db` / `mail_db` on first start only; existing files are never overwritten on reinstall. Delete or move them to recreate. Backups copy to `users.db.flb` and `mail.db.flb` by default.

Flatfile: first start creates Sysop in `users/users.ini` with a random password (10–14 chars, `a-z` `0-9`); printed once on the console. SQLite: same Sysop bootstrap in `users.db`.

### `[auth]`

| Key | Default | Description |
|-----|---------|-------------|
| `auto_login` | Main: `yes` | Guest slots when `yes` |
| `guest_prefix` | `Guest` | Guest nickname prefix |
| `guest_timeout_minutes` | `30` | Guest disconnect |

### `[crypto]`

Defaults use **built-in** backends (`tinysha256`, `tinyaes`, `monocypher`, system random) — no host package dependencies.

**Recommended when available:** OpenSSL (`-DHYBBX_CRYPTO_OPENSSL=ON`, requires libssl) for `password_hash` and `aes_gcm`; libsodium for `chacha` / `x25519`.

| Key | Default | Options |
|-----|---------|---------|
| `password_hash` | `tinysha256` | `openssl` if built |
| `aes_gcm` | `tinyaes` | `openssl` |
| `chacha`, `x25519` | `monocypher` | `libsodium` |
| `random` | `system` | `openssl` |

### `[traffic]`

| Key | Default | Description |
|-----|---------|-------------|
| `baud` | `2400` | Output pacing |
| `line_width` | `80` | Wrap |
| `pace_output` | `yes` | 8N1 timing |
| `ansi` | `no` | ANSI escapes |
| `input_echo` | `no` | Local echo; `/echo` overrides |

### `[texts]`

| Key | Default | Description |
|-----|---------|-------------|
| `path` | `text` | `banner.txt`, `motd.txt`, `news.txt`, `rules.txt` |

Tokens in `banner.txt`, `motd.txt`, `news.txt`, `rules.txt`:

| Token | Expands to |
|-------|------------|
| `@version@` | HyBBX version (banner) |
| `@service@` | `[service] name` (banner) |
| `@username@` | Session display name (motd) |
| `%time%` | Local time per `[time]` (default `HH:MM:SS` 24h) |
| `%date%` | Local date per `[time] date=` (default `YYYY/MM/DD`) |

### `[chat]`

| Key | Default | Description |
|-----|---------|-------------|
| `channels` | `10` | Channel count |
| `message_max` | `72` | Per message |
| `channel1`…`channel10` | `ChannelN` | Display names |

### `[mail]` (Main only)

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | Personal mailbox |
| `max_messages` | `50` | Per user |
| `subject_max` | `72` | |
| `body_max` | `2048` | |
| `recycle_days` | `10` | Trash retention |
| `path` | `mail` | Under storage root |

### `[networks]`

| Key | Main default | Description |
|-----|--------------|-------------|
| `ax25` | `no` | Packet radio plugin |
| `baycom` | `no` | BayCom PR-Stack plugin ([BAYCOM.md](BAYCOM.md)) |
| `ardop` | `no` | ARDOP host client |
| `crdop` | `no` | CRDOP host client |
| `ssh` | `no` | SSH plugin (libssh, port 3232) |
| `websocket` | `no` | WebSocket forward-proxy (port 4591) |
| `circuit` | `yes` | HBX hub (Main) |
| `mains_proxy` | `no` | Main-to-Main mesh proxy ([MAINS_PROXY.md](MAINS_PROXY.md); relay not active yet) |

Telnet is always started when built (not gated here).

### `[transport.telnet]`

| Key | Default | Description |
|-----|---------|-------------|
| `bind` / `bind6` | `0.0.0.0` / `::` | Listen addresses |
| `port` | `2323` | |
| `ipv4` / `ipv6` | `yes` | Dual-stack toggles |

### `[transport.ssh]` (requires libssh at build time)

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | Gated by `[networks] ssh=yes` |
| `bind` / `bind6` | `0.0.0.0` / `::` | Listen addresses |
| `port` | `3232` | |
| `ipv4` / `ipv6` | `yes` | Dual-stack toggles |
| `hostkey_dir` | `keys` | Ed25519 host key directory (auto-generated, rotated after 5 years) |

SSH username and password are **not** HyBBX accounts — they only satisfy the
SSH client handshake. After connect, behaviour matches telnet: `[auth] auto_login`
assigns a guest, otherwise the session shows the registered `/login` prompt.
Use `/login <user> <pass>` for HyBBX authentication. A registered account may
have only **one** active HyBBX session; a second `/login` is rejected while the
first connection is still open.

See `share/THIRD_PARTY_NOTICES.txt` (libssh LGPL).

### `[transport.websocket]`

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | Gated by `[networks] websocket=yes` |
| `bind` / `bind6` | `127.0.0.1` / `::1` | Loopback — public TLS on reverse proxy |
| `port` | `4591` | Loopback listen (`ss -lntp \| grep 4591`) |
| `path` | `/hybbx` | HyBBX upgrade path; httpd proxies `/hybbx-websocket/ws` here |
| `max_connections` | `10` | Max simultaneous WebSocket clients |
| `cert_dir` | `keys` | Self-signed `hybbx_ws.*` when OpenSSL linked |
| `ipv4` / `ipv6` | `yes` | Dual-stack toggles |

Plugin ships **session data only**. Copy `reverse-proxy/docroot/hybbx-websocket/` to
httpd document root. See [WEBSOCKET.md](WEBSOCKET.md).

### `[circuit]` (Main)

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | Hub master |
| `bind` / `bind6` | `127.0.0.1` / `::1` | Widen for remote Secondaries |
| `port` | `7323` | |
| `link_auth` | `yes` | `LINK_AUTH` required |
| `link_password` | — | Fallback password |
| `link_stale_days` | `10` | Stale link cleanup |
| `max_links` | `8` | Concurrent Secondaries (max 16) |
| `balance` | `yes` | Auto `FLOW_CTRL` pacing |
| `balance_lag_sec` | `5` | |
| `balance_queue_pause` | `4096` | Pause threshold |
| `balance_queue_break` | `16384` | Break threshold |
| `balance_queue_cancel` | `65536` | Cancel threshold |

### `[broadcast]` (Main)

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | `/broadcast` and `/announce` (Sysop → all online users on this Main) |
| `ax25` | `yes` | AX.25 auto-beacon over HBX (internal; not `/broadcast`) |
| `ax25_auto` | `yes` | Periodic AX.25 QST UI when a qualifying link is up |
| `ax25_auto_interval` | `300` | Seconds between AX.25 sends (minimum 300) |
| `ax25_auto_message` | `Broadcast: @service@ online` | UI payload; `@service@` expands from `[service] name` |
| `ax25_mycall` | `HYBBX` | |
| `ax25_dest` | `QST` | |

AX.25 auto-beacon payloads are capped at **48 characters** (1200 baud). This is separate from `/broadcast message`, which reaches every connected session on the local Main (max 240 characters).

### `[ax25]`

MHz list for operator reference (`frequency1`, `frequency1_label`, …). Tune radios manually.

### `[transport.packet_radioN]` (bridge registry)

On **Main**: metadata per remote Secondary (`link_id`, `link_password`, `link_role`, `frequency_mhz`) — no `device` / `tnc` keys. Or add those keys on Main for a **local TNC** (standalone).

On **Secondary**: TNC settings + `circuit_host`, `circuit_port`, matching `link_id` / password. Multiple `[transport.packet_radioN]` sections start multiple TNC devices (unique `device` + `link_id` each). See [TNCS.md](TNCS.md).

| TNC key | Notes |
|---------|-------|
| `tnc` | `tnc2c`, `tnc2`, `pktnc2`, `pk232`, `mfj1278`, `kantronics`, `baycom`, `pccom`, `generic` — [TNCS.md](TNCS.md) |
| `protocol` | `kiss`, `hostmode` (`tnc2` = host mode only), `sixpack` |
| `device` / `device_type` | Serial path |
| `baud` | Host serial baud |
| `serial_line` | `7e1` (TNC2C default), `8n1`, or `data_bits` + `parity` |
| `kiss_entry` | `kiss_on`, `esc_at_k`, `auto`, `none` |
| `kiss_exit` | `kiss_off`, `kiss_frame`, `auto`, `none` |
| `modem` | `tcm3105`, etc. |
| `radio_band` | `amateur`, `cb` |
| `radio_duplex` | `half`, `full` |
| `g3ruh_fsk` | `yes` for 9600 FSK |
| `mycall`, `dest`, `via` | AX.25 addresses |

Full Secondary example: `share/hybbx-secondary.ini.example`. TNC profiles: [TNCS.md](TNCS.md). BayCom PR-Stack: [BAYCOM.md](BAYCOM.md). ARDOP/CRDOP sections: [ARDOP.md](ARDOP.md), [CRDOP.md](CRDOP.md).

### `[transport.baycomN]` (BayCom PR-Stack)

On **Main**: bridge registry (`link_id`, `link_password`, `link_role`, `frequency_mhz`).

On **Secondary**: kernel SER12/PAR96/EPP or serial KISS + HBX circuit. Up to 4 instances. See [BAYCOM.md](BAYCOM.md).

| Key | Default | Notes |
|-----|---------|-------|
| `backend` | `kernel` | `kernel` (hdlcdrv) or `kiss` (serial firmware) |
| `mode` | `ser12*` | `ser12`, `ser12*`, `ser12+`, `ser12hdx`, `par96`, `par96*`, `epp` |
| `interface` | per mode | `bcsf0`, `bcsh0`, `bcp0`, `bce0` |
| `kernel_module` | per mode | `baycom_ser_fdx`, `baycom_ser_hdx`, `baycom_par`, `baycom_epp` |
| `iobase` / `irq` | `0x3f8` / `4` | SER12 UART (kernel backend) |
| `radio_baud` | `1200` | On-air baud |
| `kernel_autoload` | `no` | `modprobe` on start (root) |
| `txdelay`, `slot`, `persist`, `txtail` | see [BAYCOM.md](BAYCOM.md) | Channel access (10 ms units) |
| `device` / `serial_baud` | `/dev/ttyS0` / `1200` | KISS backend only |
| `circuit_host`, `circuit_port`, `link_id`, `link_password` | — | HBX to Main |

### `[transport.mains_proxyN]` (Main-to-Main mesh)

Links Mains via **HBX/Circuit** only; relay not active yet. See [MAINS_PROXY.md](MAINS_PROXY.md).

| Key | Default | Notes |
|-----|---------|-------|
| `peer_id` | (empty) | Logical peer name |
| `circuit_host` | — | Remote Main HBX hub |
| `circuit_port` | `7323` | Remote HBX port |
| `link_id` | — | Bridge id (match peer) |
| `link_password` | — | `LINK_AUTH` secret |
| `wire` | `circuit` | `circuit` or `ax25` (reserved) |
| `duplex` | `full` | `full` or `half` |
| `use_secondary` | `yes` | Prefer Secondary path when available |

Deprecated: `host` / `port` (use `circuit_host` / `circuit_port`).

---

## Commands

Input: `/command` … · `;` and `#` lines ignored · other text → area handler.

Registry and help layout: [COMMANDS.md](COMMANDS.md) · [share/commands.yaml](../share/commands.yaml).

| Surface | Role |
|---------|------|
| `/help`, `/menu` | Commands this session can use |
| `/index` | Full command-index (same for every account) |
| `/alias` | Alternate command names |
| `/help cmd` | Two-line topic help |

Headers: `HyBBX commands /help <cmd> for more` · `HyBBX command-index …` · `HyBBX aliases …`

Menu groups: **General**, **Screen**, **Areas**, **Account**, **Admin**, **Sysop**. User groups: **Sysop**, **Admin**, **Mod**, **User**, **Guest**.

### Guests

With `auto_login = yes`, guests may use:

| Command | Description |
|---------|-------------|
| `/help`, `/menu` | Command menu |
| `/index` | Full command-index |
| `/alias` | Alias map |
| `/news` | System news (`news.txt`) |
| `/motd` | Message of the day |
| `/rules`, `/legal` | Terms of use |
| `/login` | Registered login |
| `/register` | Self-registration |
| `/clear`, `/echo`, `/exit` | Screen, echo, disconnect |

Mail, chat, conference, and `/who` require a registered account.

### All users

| Command | Description |
|---------|-------------|
| `/help`, `/menu` | Command menu |
| `/index` | Full command-index |
| `/alias` | Alias map |
| `/motd` | Message of the day |
| `/news` | System news |
| `/rules`, `/legal` | `text/rules.txt` |
| `/who` | Online users |
| `/users` | Registered count |
| `/session` | Session info |
| `/version` | HyBBX version |
| `/clear` | Clear screen |
| `/echo yes\|no` | Input echo |
| `/exit` | Disconnect |
| `/main` | Main area |
| `/leave` | Up one area |
| `/chat` | Chat channels |
| `/conference` | Two-user conference |
| `/mail` | Local mailbox (registered) |
| `/mail proxymail` | Enter inter-Main mail sub-area (delivery not available yet) |
| `/proxymail` | Same as `/mail proxymail` |
| `/chat proxychat` | Enter inter-Main chat sub-area (not available yet) |
| `/proxychat` | Same as `/chat proxychat` |
| `/login <user> <pass>` | Registered login (one active session per account) |
| `/register` | Self-registration (guest) |
| `/changeme` | Own profile/password |
| `/deleteme` | Delete own account (User) |

### Admin

| Command | Min level |
|---------|-----------|
| `/usercreate` | Admin |
| `/activate` | Admin |
| `/promote`, `/demote` | Admin |
| `/delete` | Admin |
| `/changeuser` | Admin |

### Sysop

| Command | Min level |
|---------|-----------|
| `/deleteuser` | Sysop |
| `/shutdown`, `/restart` | Sysop |
| `/broadcast message`, `/announce` | Sysop — all online users on this Main |

User groups: **Sysop**, **Admin**, **Mod**, **User**, **Guest** — see [share/commands.yaml](../share/commands.yaml).

---

## Licensing

HyBBX: GPL-3.0 — [LICENSING.md](LICENSING.md). External modems are separate programs.

---

## See also

[TOPOLOGY.md](TOPOLOGY.md) · [WEBSOCKET.md](WEBSOCKET.md) · [BUILD.md](BUILD.md)
