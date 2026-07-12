# Operator manual

**v2.0.0** — telnet, SSH, WebSocket. Templates: `share/hybbx.ini.example`, `share/hybbx-secondary.ini.example`.

Booleans: `yes`/`no` (+ `true`/`false`, `on`/`off`, `1`/`0`).

## Text-based operation

HyBBX is a **text-first** system: sessions read and write **plain text** (lines, `/` commands, mail bodies, chat). Default settings assume classic terminal use — 80 columns, optional 2400-baud pacing, ANSI off.

| Need | INI / mechanism |
|------|-----------------|
| Plain telnet/BBS feel | `[traffic]` defaults (`ansi=no`, `pace_output=yes`) |
| Colour / cursor control | `[traffic] ansi=yes` |
| Banners, MOTD, rules | `[texts]` flat files with tokens |
| Richer UI later | Plugins or WebSocket client ([WEBSOCKET.md](WEBSOCKET.md)) — core stays line-oriented |

RF and mesh paths (HBX, AX.25 UI beacons) also carry **text payloads**, not graphical desktops. Portability: same session model on Linux, *BSD, AmigaOS, MacOS, Windows — [PLATFORMS.md](PLATFORMS.md).

---

## Topology

```
Users (telnet :2323, SSH :3232, WebSocket via proxy) ──► Main (storage, mail)
                                                              ▲
                                                              │ HBX/TCP :7323
                                                         Secondary (packet_radio / ardop / crdop)
```

| Role | `[networks]` typical | Hosts |
|------|----------------------|-------|
| **Main** | `circuit=yes`, `ax25=no` | Users, HBX hub |
| **Secondary** | `circuit=no`, `ax25=yes` | TNC/RF; `circuit_host` → Main |
| **Standalone Main** | `ax25=yes` on same box | Lab / single-host |

**Secondary** = separate HyBBX process (infrastructure), not a telnet user. Multiple Secondaries: unique `link_id` per active link; `max_links` on Main (default 8, max 16).

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
| `seconds` | `no` | Append `:SS` when `yes` |

### `[log]`

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | File logging (`yyyymmdd-hybbx.log`) |
| `dir` | `logs` | Log directory under install root |
| `level` | `warn` | `debug` \| `info` \| `stats` \| `warn` |

`security.log` is always written to the same directory (independent of `enabled`).

### `[storage]`

| Key | Default | Description |
|-----|---------|-------------|
| `backend` | `flatfile` | `flatfile` or `sqlite` |
| `path` | `data` | User shards under `users/` |

First start creates Sysop in `users/users.ini` with a random password (10–14 chars, `a-z` `0-9`); printed once on the console.

### `[auth]`

| Key | Default | Description |
|-----|---------|-------------|
| `auto_login` | Main: `yes` | Guest slots when `yes` |
| `guest_prefix` | `Guest` | Guest nickname prefix |
| `guest_timeout_minutes` | `30` | Guest disconnect |

### `[crypto]`

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
| `ansi` | `no` | ANSI escapes (optional colour/menus on capable clients) |
| `input_echo` | `no` | Local echo; `/echo` overrides |

### `[texts]`

| Key | Default | Description |
|-----|---------|-------------|
| `path` | `text` | `banner.txt`, `motd.txt`, `news.txt`, `rules.txt` |

Tokens: `@version@`, `@service@`, `@username@` in banner/motd.

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

Telnet is always started when built (not gated here).

Entertain Area applications (e.g. text chess) are **plugins only** on Main — see [ENTERTAIN.md](ENTERTAIN.md). Enable per app when shipped (`chess = yes`, …).

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
| `balance_lag_sec` | `8` | |
| `balance_queue_pause` | `8192` | Pause threshold |
| `balance_queue_break` | `32768` | Break threshold |
| `balance_queue_cancel` | `131072` | Cancel threshold |

### `[broadcast]` (Main)

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `yes` | |
| `ax25` | `yes` | QST UI to low/half-duplex links |
| `ax25_auto` | `yes` | Periodic AX.25 QST beacon |
| `ax25_auto_interval` | `300` | Seconds between beacon cycles (min 300) |
| `ax25_auto_stagger` | `0` | Seconds between successive link phases within one interval (`0` = all links at once; dual-radio e.g. `150`) |
| `ax25_auto_message` | `Broadcast: @service@ online` | Auto-beacon text (`@service@` = service name); also used by `/broadcast ax25` |
| `tcp` | `yes` | **Stub** — log only |
| `ax25_mycall` | `HYBBX` | |
| `ax25_dest` | `QST` | |

Sysop `/broadcast <message>` announces to logged-in local users only (not circuit/TNC links). `/broadcast ax25` sends `ax25_auto_message` instantly to each packet-radio link in sequence. INI `ax25_auto` is the periodic staggered background beacon.

### `[ax25]`

MHz list for operator reference (`frequency1`, `frequency1_label`, …). Tune radios manually.

### `[transport.packet_radioN]` (bridge registry)

On **Main**: metadata per remote Secondary (`link_id`, `link_password`, `link_role`, `frequency_mhz`).

On **Secondary**: TNC settings + `circuit_host`, `circuit_port`, matching `link_id` / password.
With `[circuit] link_auth = yes` (default), `link_password` is required on the
edge side; missing password causes `link_auth_fail ... reason=timeout_no_link_auth`.

| TNC key | Notes |
|---------|-------|
| `tnc` | `tnc2c`, `tnc2`, `pktnc2`, `pk232`, `mfj1278`, `kantronics`, `baycom`, `pccom`, `generic` |
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

---

## Commands

Input: `/command` … · `;` and `#` lines ignored · other text → area handler.

### All users

| Command | Description |
|---------|-------------|
| `/help` | List or `/help <cmd>` |
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
| `/mail` | Mailbox (registered) |
| `/login <user> <pass>` | Registered login (one active session per account) |
| `/register` | Self-registration (guest) |
| `/changeme` | Own profile/password |

### Admin and Sysop

| Command | Role |
|---------|------|
| `/usercreate` | Admin+ |
| `/activate` | Admin+ |
| `/promote`, `/demote` | Admin+ |
| `/delete`, `/deleteuser` | Admin+ |
| `/changeuser` | Admin+ |
| `/deleteme` | User (own account) |
| `/shutdown`, `/restart` | Sysop |
| `/broadcast <message>` | Sysop — local online users |
| `/broadcast ax25` | Sysop — instant RF beacon (INI `ax25_auto_message`) |

---

## Licensing

HyBBX: GPL-3.0 — [LICENSING.md](LICENSING.md). External modems are separate programs.

---

## See also

[TOPOLOGY.md](TOPOLOGY.md) · [WEBSOCKET.md](WEBSOCKET.md) · [BUILD.md](BUILD.md)
