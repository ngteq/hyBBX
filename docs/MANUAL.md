# HyBBX manual

Operator reference: INI, transports, commands. Templates: `share/hybbx.ini.example` (Main), `share/hybbx-secondary.ini.example` (Secondary). Index: [INDEX.md](INDEX.md) · Features: [FEATURES.md](FEATURES.md) · Build: [BUILD.md](BUILD.md).

## Overview

HyBBX is a C99 **multi-transport session daemon** for low-bandwidth links. One session core (mail, chat, `/` commands) serves all connections. **Link adapters** (telnet, packet radio, HBX circuit client) terminate wire protocols and attach byte streams to the core on **Main**.

**Secondaries** are **separate remote machines** — extenders, repeaters, gateways, and other next-hop edge devices that bridge RF or other transport **to** Main over HBX/TCP. Each runs its own HyBBX process (`circuit_host` → Main `[circuit]`, port **7323**); Main holds users, storage, and the session core. They are **infrastructure**, not end-user telnet sessions. **Not** Secondaries: telnet on Main (`:2323`), local `[transport.*]` on Main, or any adapter inside the Main process. **v0.8.0:** several remote Secondaries may connect to one Main at once (unique `link_id` per active link). `link_role` (`link`, `repeater`, `gateway`, `digipeater`, …) labels the bridge type; all are the same **Secondary** class relative to Main.

| Role | Default | Hosts |
|------|---------|-------|
| **Main** | `ax25=no`, `circuit=yes` | Users, storage, telnet :2323, HBX hub |
| **Secondary** | `ax25=yes`, `circuit=no` | Remote edge host: TNC/RF; outbound HBX client to Main |
| **Standalone Main** | `ax25=yes` on same host | All adapters local; no remote Secondary |

Override any default in INI. Topology details: [ROADMAP.md](ROADMAP.md). Firewall, VPN, TLS: **system layer** — not built into HyBBX.

## Connection types

| Transport       | Status   | `[networks]` switch | Description |
|-----------------|----------|---------------------|-------------|
| TCP/IP Telnet   | Started  | **static** (always on) | Line-oriented terminal access over TCP |
| SSH             | After v1.0.0 | — | Secure shell transport plugin (post–first GitHub release) |
| AX.25 / Packet Radio | Started | `ax25 = yes\|no` | TNC2C (KISS/AX.25); USB/RS232 |
| ARDOP (host client) | Partial | `ardop = yes\|no` | External **ARDOPC**; amateur-profile host TCP bridge |
| CRDOP (CB host client) | Partial | `crdop = yes\|no` | External **ARDOPC/ardopcf**; CB-profile host TCP bridge |
| WebSocket       | After v1.0.0 | `websocket = yes\|no` | Forward-proxy behind Apache/nginx only |
| HBX circuit hub | Started  | `circuit = yes\|no` | Main TCP hub — remote Secondary processes connect here |

Telnet starts when built (ignores `enabled`). Optional adapters need `[networks]` switches. **SSH**, **WebSocket**, **user-files**, **public-files**: post–v1.0.0 ([ROADMAP.md](ROADMAP.md)).

## Architecture

Plugin API: `hybbx_transport_plugin_t` ([include/hybbx/plugin.h](../include/hybbx/plugin.h)). Core handles TCP/IPv4+IPv6 and HBX only — no KISS/AX.25 parsing in `src/core/`.

**Project default:** HyBBX is **plugin-only** (host-client bridges). Modems, TNCs, and sound-card services stay **external**. Use `ardop` (amateur) and/or `crdop` (CB) plugins against external ARDOPC or ardopcf — not a separate CRDOP daemon repo.

## Configuration (INI)

HyBBX reads `-c` / `--config`. Example: `share/hybbx.ini.example`.

### Boolean values (HyBBX standard)

All yes/no configuration keys use the same parser (`hybbx_parse_bool` in
`include/hybbx/util.h`). HyBBX writes the canonical form **`yes`** / **`no`**.

| Meaning | Accepted values (case-insensitive) |
|---------|-----------------------------------|
| True | `yes`, `true`, `enable`, `enabled`, `on`, `1` |
| False | `no`, `false`, `disable`, `disabled`, `off`, `0` |

Examples: `enabled = yes`, `pace_output = enable`, `ansi = false`,
`g3ruh_fsk = disable`, `auto_login = 1`.

### Networks (`[networks]`)

Master switches for optional connection adapters. Core IP transports are static-enabled.

**Datacenter defaults** (Main template `share/hybbx.ini.example`):

```ini
[networks]
ax25 = no          ; off on Main — run on Secondary(s) by default
ardop = no         ; external ARDOPC on Secondary (parallel to ax25)
websocket = no     ; after v1.0.0
circuit = yes      ; HBX TCP hub on Main; Secondaries connect here
```

**Secondary defaults** (`share/hybbx-secondary.ini.example`): `ax25=yes`, `ardop=no`, `circuit=no`.

**Standalone Main:** set `ax25=yes` and configure `[transport.packet_radio]` locally — all connection types on one host, no remote HBX.

| Key | Main default | Secondary default | Controls |
|-----|--------------|-------------------|----------|
| `ax25` | `no` | `yes` | `[transport.packet_radio]` / `[transport.packet_radioN]` |
| `ardop` | `no` | `no` | `[transport.ardop]` / `[transport.ardopN]` — external ARDOPC |
| `crdop` | `no` | `no` | `[transport.crdop]` / `[transport.crdopN]` — external ARDOPC (CB defaults) |
| `websocket` | `no` | `no` | `[transport.websocket]` after v1.0.0 |
| `circuit` | `yes` (loopback hub) | `no` | `[circuit]` HBX hub on Main; Secondary uses `circuit_host` instead |

Telnet is **not** listed here — it is static-enabled when compiled in.

### Internal circuit (HBX over TCP/IPv4+IPv6)

HyBBX core speaks **only TCP**. Link adapters (packet radio, future stacks) attach to an
internal circuit hub on loopback. Payloads are wrapped in **HBX v1** frames:

| HBX `proto` | Payload | Direction |
|-------------|---------|-----------|
| `ax25` (0x01) | Raw AX.25 frame incl. FCS | RF → core |
| `ax25_ui` (0x02) | Masked path + UI bytes | Optional metadata |
| `terminal` (0x10) | Terminal byte stream | Core ↔ RF TX |
| `flow_ctrl` (0x05) | `action=pause\|break\|cancel\|resume` | Hub ↔ link (load-balance) |

Reserved protocol IDs (`0x20` APRS, `0x21` NETROM, …) are allocated for future stacks.

**Auto load-balancing:** When telnet (high bandwidth, full-duplex) and packet radio (low bandwidth, often half-duplex) share the same Main, the hub queues downlink at each link’s reported `radio_baud` and sends `flow_ctrl` if the queue grows. A few seconds of lag on slow links is normal. Escalation per link: **pause** (hold RF I/O) → **break** (drop queued frames) → **cancel** (drop the secondary link only after users are exhausted). Secondary advertises QoS in `LINK_AUTH` (`baud`, `duplex`, `bandwidth=low`); packet_radio sends these automatically.

**User policy (LIFO):** Under the same pressure, telnet and on-RF **users** are frozen or dropped **last-connected first** — circuit secondaries are never the first sacrifice. Secondary links are cancelled only when no more users can be disconnected. Disconnect message: `You are disconnected by bandwidth limits.`

```ini
[circuit]
enabled = yes
bind = 127.0.0.1
bind6 = ::1
port = 7323
ipv4 = yes
ipv6 = yes
balance = yes
balance_lag_sec = 5
max_links = 8
```

Packet radio connects as a **link client** (`circuit_host` / `circuit_port` in
`[transport.packet_radioN]` on Secondary; Main holds matching bridge section with
`link_id` / `link_password` only). The hub opens a `circuit` session and bridges HBX frames
to `hybbx_session` — the application never parses KISS or AX.25 directly.

```
  RF (AX.25)  →  TNC  →  packet_radio  →  HBX/TCP  →  circuit hub  →  session
  session     →  circuit hub  →  HBX/TCP  →  packet_radio  →  TNC  →  RF
```

Telnet remains native TCP to the session (no HBX wrapper on the wire).

### Main ↔ Secondary bridge

A **Secondary** is a **remote** edge extender/repeater/gateway (or similar device) — not a telnet user session and not a local `[transport.*]` on Main. It runs its own HyBBX process with local RF/TNC (or other link adapters) and maintains an outbound TCP HBX session to Main's `[circuit]` hub.

Each remote Secondary connects to Main `circuit_host:circuit_port` (plain TCP). Link auth: matching `link_id` + `link_password` on Main `[transport.packet_radioN]` and Secondary. No HyBBX link proxy before v1.0 — point `circuit_host` at Main (or your own proxy).

**External security:** firewall (TCP 2323/7323), VPN (`circuit_host` = tunnel IP), `ssh -L`, stunnel, reverse proxy.

**`security.log`** — under `[log] dir` (default `logs/`), even when `hybbx.log` is off. Format: `YYYY-MM-DD HH:MM:SS event key=value …`. fail2ban examples: `share/fail2ban/`.

```
  Secondary: RF → TNC → packet_radio ──TCP:7323──► Main [circuit] → session
                                              telnet :2323, mail, storage, chat
```

Bridge sections — same name on both sides; increment **N** per Secondary:

| Side | Section | Keys |
|------|---------|------|
| Main | `[transport.packet_radio1]` | `link_id`, `link_password`, `link_role` |
| Secondary | `[transport.packet_radio1]` | same link keys + TNC/device settings |

**v0.8.0:** Up to `max_links` concurrent Secondaries (default 8, max 16); one active HBX link per `link_id`. Secondaries are edge infrastructure — not telnet users. Multiple secondaries may share the same MHz — broadcast and load-balance use HyBBX-QoS. See [ROADMAP.md](ROADMAP.md#multi-link-v080--done).

**Main** — circuit hub on loopback by default; register bridges:

```ini
[networks]
ax25 = no
websocket = no
circuit = yes

[circuit]
enabled = yes
bind = 127.0.0.1
bind6 = ::1
port = 7323
link_auth = yes
link_password = <shared-secret>
link_stale_days = 10
max_links = 8

[transport.packet_radio1]
link_id = secondary-1
link_password = <shared-secret>
link_role = link
frequency_mhz = 27.205

; [transport.packet_radio2]
; link_id = secondary-2
; link_password = <other-secret>
; link_role = link
; frequency_mhz = 27.205
```

**Remote Secondaries** — widen the hub bind in `hybbx.ini` (and firewall accordingly):

```ini
[circuit]
bind = 0.0.0.0
bind6 = ::
```

Open **TCP 7323** on the **system firewall** when exposing the hub (and **2323** for telnet users). Boot log should show the configured circuit bind, e.g. `127.0.0.1:7323` (default) or `0.0.0.0:7323` after you change it.

**Secondary** — no local circuit hub (`circuit = no`); bridge RF to Main (section name must match Main):

```ini
[networks]
ax25 = yes
circuit = no

[mail]
enabled = no

[transport.packet_radio1]
circuit_host = <main-host-or-vpn-ip>
circuit_port = 7323
link_id = secondary-1
link_password = <same-as-main-bridge-section>
link_role = link
device = /dev/ttyUSB0
tnc = tnc2c
protocol = kiss
```

Run Secondary: `hybbx -c /path/to/hybbx-secondary.ini`.

**Link flow:** TCP connect → `LINK_AUTH` (`link_password`, `link_id`, `link_role`, optional `baud`, `duplex`, `bandwidth`) → registry → circuit session. RF uplink: HBX `ax25`; downlink: HBX `terminal`.

#### Standalone Main

To run **without Secondary nodes** — Main owns telnet, circuit hub, and AX.25 on one machine:

```ini
[networks]
ax25 = yes
circuit = yes

[circuit]
bind = 127.0.0.1
bind6 = ::1

[transport.packet_radio]
circuit_host = 127.0.0.1
circuit_port = 7323
link_id = local-1
link_password = changeme
device = /dev/ttyUSB0
tnc = tnc2c
protocol = kiss
```

No remote Secondary required — loopback circuit attach only.

### Telnet

```ini
[transport.telnet]
bind = 0.0.0.0
bind6 = ::
port = 2323
ipv4 = yes
ipv6 = yes
```

Dual-stack (`IPV6_V6ONLY`). Set `ipv6 = no` to disable v6 socket.

**Wire behaviour:** `WONT ECHO` (input echo via `/echo yes` at session layer); `WILL SGA`; declines linemode/NAWS/ttype. Accepts CR/LF/CRLF; output LF→CRLF; backspace/delete edit line.

CLI clients: [CLIENTS.md](CLIENTS.md) (`hybbx-telnet`, `hybbx-terminal`).

### Circuit link authentication

Match `link_id` + `link_password` on Main bridge section and Secondary. No link heartbeat/ping in HyBBX.

```ini
[circuit]
link_auth = yes
link_password = changeme
link_stale_days = 10

[transport.packet_radio1]
link_id = secondary-1
link_password = changeme
link_role = repeater
```

Successful auth → `data/links/` + `[link.<id>]`. Stale links (no auth within `link_stale_days`) auto-removed.

### Traffic (2400 baud profile)

```ini
[traffic]
baud = 2400
line_width = 80
pace_output = yes
ansi = no
input_echo = no
```

Default: 80-column ASCII, paced 8N1. Echo off (`input_echo = no`); per-session `/echo yes`. `ansi = yes` for gray-on-black and `/clear`. Keep `text/*.txt` lines ≤80 chars when possible.

### ARDOP (external ARDOPC + HyBBX host client)

HyBBX is **not** a sound-modem service. The operator runs **[ARDOPC](https://github.com/g8bpq/ardop)** or MIT **[ardopcf](https://github.com/pflarue/ardop)** as a **separate external process**. The `ardop` plugin is a host-client over TCP (control **N**, data **N+1**), parallel to AX.25 on a Secondary.

### CRDOP (CB host client — `transport.crdop`)

Same external modem as ARDOP; **`crdop`** plugin applies **CB defaults** (`500MAX`, half-duplex QoS, `crdop-link`). See [CRDOP.md](CRDOP.md).

```ini
[networks]
crdop = yes

[transport.crdop1]
modem_host = 127.0.0.1
modem_port = 8515
mycall = CALL-0
frequency_mhz = 27.205
circuit_host = main.example.com
link_id = secondary-1-crdop
link_password = changeme
```

Not in HyBBX (external modem only): FEC/OFDM, PTC, radio CAT, audio DSP.

**Implemented subset** (enough for HBX bridge traffic):

| Direction | Items |
|-----------|--------|
| Host → TNC | `INITIALIZE`, `MYCALL`, `PROTOCOLMODE ARQ`, `ARQBW`, `LISTEN`, `ARQCALL`, `DISCONNECT`, `RDY` |
| TNC → Host | `CONNECTED` / `DISCONNECTED`, `BUFFER`, `FAULT`, `d:ARQ` data frames |
| Wire | CRC-16 (poly 0x8810), binary `D:`/`d:` payloads |

Not in scope for HyBBX (external modem only): FEC/OFDM modes, PTC emulation, radio CAT, sound-card capture/playback, modem DSP.

**CRC:** host frames use CRC-16 for **error detection**; **ARQ** provides retransmission-based recovery.

**Operator flow (Secondary):**

1. Start ARDOPC, e.g. `ardo1pofdm TCPIP 8515 127.0.0.1` (control **8515**, data **8516**).
2. Enable `[networks] ardop = yes` and configure `[transport.ardop1]` (section name must match Main bridge).
3. HyBBX connects to ARDOPC, bridges `d:ARQ` ↔ HBX `terminal` proto on the circuit uplink.

```ini
[networks]
ax25 = yes
ardop = yes
circuit = no

; Parallel to [transport.packet_radio1] — separate link_id on Main
[transport.ardop1]
enabled = yes
ardop_host = 127.0.0.1
ardop_port = 8515
mycall = CALL-0
arq_bandwidth = 500MAX
listen = yes
; peer_call = REMOTE-0   ; optional outbound ARQ
circuit_host = main.example.com
circuit_port = 7323
link_id = secondary-1-ardop
link_password = changeme
link_role = link
frequency_mhz = 7.100
```

Main bridge registry (`share/hybbx.ini.example`):

```ini
[transport.ardop1]
link_id = secondary-1-ardop
link_password = changeme
link_role = link
frequency_mhz = 7.100
```

### Packet radio / AX.25 TNC stack

HyBBX implements an **AX.25 UI frame layer** and a unified TNC driver with three host protocols:

| Protocol | Use |
|----------|-----|
| `kiss` | KISS (Mike Chepponis) — default for TNC2C and BayCom |
| `hostmode` | TNC2 command prompt / connected converse — default for PC-COM |
| `6pack` | 6PACK (DF6BU) — alternative serial framing |

**TNC profiles** (`tnc=`):

| Profile | Hardware | Default host baud | Default radio | Notes |
|---------|----------|-------------------|---------------|-------|
| `tnc2c` | [Landolt TNC2C](https://www.landolt.de/info/afuinfo/tnc2c.htm) | 2400 | 1200 (TCM3105) | Primary test device |
| `baycom` | BayCom-class modems (TCM3105 AFSK) | 2400 | 1200 | KISS or host/6PACK over RS-232 |
| `pccom` | Albrecht PC-COM CB packet modem | 2400 | 1200 | TNC2 host mode, TCM3105 |
| `generic` | Any TNC2/KISS TNC | 2400 | configurable | User supplies all parameters |

**RF band and duplex** — CB packet is **half-duplex** only (one direction at a time on a shared channel). Amateur setups may use **full-duplex** when the radio/TNC supports it (split frequency, separate RX while TX).

| `radio_band` | Duplex allowed | Typical use |
|--------------|----------------|-------------|
| `cb` | half only | PC-COM, BayCom on 11 m CB |
| `amateur` | half or `full` | TNC2C, 2 m / 70 cm packet |

| `radio_duplex` | Meaning | TNC firmware |
|----------------|---------|--------------|
| `half` | Half-duplex (duplex) — wait after RX before TX | KISS full-duplex=0, `FULLDUP OFF` |
| `full` | Full-duplex — TX while RX possible | KISS full-duplex=1, `FULLDUP ON` |

`radio_duplex=full` is **rejected** when `radio_band=cb`. Legacy `fullduplex=yes|no` maps to `radio_duplex` when `radio_duplex` is not set.

**Modulation** — classic CB/amateur packet uses **AFSK** (Bell 202 / TCM3105 @ 1200 baud). **G3RUH FSK** @ 9600 baud offers higher throughput when the TNC and radio support it.

| Key | Values | Effect |
|-----|--------|--------|
| `g3ruh_fsk` | `yes` / `no` | Enable G3RUH 9600 FSK (`yes`) or AFSK 1200 (`no`, default) |
| `modulation` | `afsk`, `g3ruh` | Alias for `g3ruh_fsk` (`modulation=g3ruh` ≡ `g3ruh_fsk=yes`) |

When `g3ruh_fsk=yes`, HyBBX sets `modem=9600`, `radio_baud=9600`, TNC2C `clock_mhz=9.8` (if unset), G3RUH-oriented `txdelay`/`slot`, sends `MODEM 9600` / `HB 9600` to the TNC before KISS or host mode, and tags HBX uplink frames with `G3RUH_FSK`. CB remains half-duplex only.

```ini
; Secondary — full TNC block; Main bridge section uses link_id/link_password only
[transport.packet_radio1]
enabled = no
tnc = tnc2c
protocol = kiss
device_type = usb
device = /dev/ttyUSB0
baud = 2400
modem = tcm3105
radio_baud = 1200
mycall = DL1ABC-0
dest = DL9XYZ-0
via = RELAY-7,WIDE1-1
radio_band = amateur
radio_duplex = half
kiss_on_startup = yes
host_connect = no
```

**TNC2C example** (Landolt, amateur full-duplex capable):

```ini
tnc = tnc2c
protocol = kiss
radio_band = amateur
radio_duplex = full
clock_mhz = 4.9
txdelay = 50
persist = 128
slot = 10
```

**TNC2C half-duplex** (default, safe on shared channels):

```ini
tnc = tnc2c
radio_band = amateur
radio_duplex = half
```

**PC-COM example** (Albrecht CB, half-duplex only):

```ini
tnc = pccom
protocol = hostmode
radio_band = cb
radio_duplex = half
baud = 2400
mycall = DL1ABC-0
dest = DL9XYZ-0
host_connect = yes
```

**BayCom example** (CB half-duplex, KISS @ 2400 baud host):

```ini
tnc = baycom
protocol = kiss
radio_band = cb
radio_duplex = half
baud = 2400
radio_baud = 1200
```

**CB G3RUH FSK example** (9600 baud, half-duplex, G3RUH-capable hardware):

```ini
tnc = tnc2c
protocol = kiss
radio_band = cb
radio_duplex = half
g3ruh_fsk = yes
baud = 2400
```

| Key | Description |
|-----|-------------|
| `tnc` | `tnc2c`, `baycom`, `pccom`, `generic` |
| `protocol` | `kiss`, `hostmode`, `6pack` |
| `device`, `device_type`, `baud` | Host serial link (`/dev/ttyUSB0`, `COM3`, `serial.device`, … — [PLATFORMS.md](PLATFORMS.md)) |
| `modem`, `radio_baud`, `clock_mhz` | Radio-side metadata (TNC2C) |
| `mycall`, `dest` / `dest_call` | AX.25 addresses (`CALL` or `CALL-SSID`) |
| `via` | Comma-separated digipeater list |
| `radio_band` | `cb` or `amateur` (sets safe duplex defaults) |
| `radio_duplex` | `half` or `full` (`duplex` alias for half) |
| `g3ruh_fsk` | `yes` / `no` — G3RUH 9600 FSK vs AFSK 1200 (default `no`; HyBBX boolean aliases apply) |
| `modulation` | `afsk` or `g3ruh` (alias for `g3ruh_fsk`) |
| `kiss_port`, `kiss_on_startup` | KISS mode control |
| `txdelay`, `persist`, `slot`, `txtail` | KISS/TNC2 timing |
| `fullduplex` | Legacy boolean → `radio_duplex` when unset (HyBBX yes/no standard) |
| `host_connect` | Auto `C <dest>` in host mode on startup |

True BayCom `ser12` kernel modems (non-async serial) require the Linux `baycom` driver and `kissattach`; HyBBX talks to a **KISS or TNC2 host port** on RS-232/USB.

### Service, auth, chat

```ini
[service]
name = hyBBX
max_online = 35
; prompt = hybbx>

[auth]
auto_login = yes
guest_prefix = Guest
guest_timeout_minutes = 30
; Guests are ephemeral (Guest1 … Guest25 in memory only — not stored in user files).
; auto_login=yes (default): next free guest on connect (max 25 simultaneous).
; auto_login=no: login prompt — /login and /register for registered accounts only.

[chat]
channels = 10
message_max = 72
channel1 = Channel1
; channel2 = Channel2   ; omit keys to keep code defaults (Channel1 … Channel10)
```

### Mail

```ini
[mail]
enabled = yes
max_messages = 50
subject_max = 72
body_max = 2048
recycle_days = 10
path = mail
```

Messages stored under `<storage>/mail/<username>/inbox/` on **Main** only (`path` is relative to `[storage] path`).

## Input routing

| Input | Scope | Handler |
|-------|-------|---------|
| `/help`, `/exit`, … | HyBBX | Core (`/` required) |
| `;` / `#` … | Comment | Ignored |
| plain text (non-chat, non-mail-compose) | Local mailbox | Ignored silently |
| plain text in mail compose | Mail body | Appended until `/mail done` |
| plain text in chat | Chat | Broadcast to channel |

**Rules:** `/verb`, `/ verb`, `/command verb`; `/` alone → `/help`.

## Areas

| Area | Enter | Notes |
|------|-------|-------|
| `main` | Default at connect | `/main` or `/menu` from anywhere |
| `mail` | `/mail send …` | Compose body; `/mail done` sends |
| `chat` | `/chat` | Registered users only |
| `conference` | `/conference` / `/meeting` | Private 2-user channel (invite flow) |

**Planned (not in v1.0.0):** **user-files** (per-user documentation/uploads) and **public-files** (shared site library) — after **SSH** and **WebSocket** transports ship. See [ROADMAP.md](ROADMAP.md).

`/leave` (`/back`) goes up **one** level (parent area). `/main` (`/menu`) returns to **main** directly and clears all sub-areas.

## Storage

```ini
[storage]
backend = flatfile
path = data
```

Default flat-file layout under the install prefix `data/` directory (created on first start; relative to cwd when started via `hybbx-start`):

| Path | Purpose |
|------|---------|
| `users/users.ini` | Registered accounts (default Sysop on first start) |
| `mail/<user>/inbox/` | Personal mail |
| `sessions.dat`, `user.next`, `session.next` | Session counters |

Relative paths (`data`, `text`, `logs`) resolve against the install root (`HYBBX_ROOT`, or the directory containing the loaded `hybbx.ini`). `~` in `path` still expands to `$HOME` for absolute overrides.

| Backend | Status |
|---------|--------|
| `flatfile` | Implemented |
| `sqlite`, `mysql`, `mariadb` | Planned |

```ini
[user.1]
id = 1
username = sysop
level = sysop
active = yes
created = 1710000000
fullname = System Operator
country =
location =
email =
password = {sha256}…
```

Legacy `users.dat` (`id|name|level|…`) migrates on first startup. Plain passwords upgrade to `{sha256}` on open.

| Password format | Notes |
|-----------------|-------|
| `{sha256}<hex>` | Default (tinysha256, or OpenSSL if configured) |
| `{md5}<hex>` | Verify-only legacy |
| plain text | Auto-upgraded to `{sha256}` |

| `sessions.dat`, `user.next`, `session.next` | Session counters / log |
| `mail/<user>/inbox/*.msg` | Personal mail (Main only) |

**Default Sysop** (empty DB): `Sysop` / `SysopPassword` — change via `/changeme` (passwords 8–24 chars).

**Levels** (high → low): Sysop, Admin, Mod, User, Guest. Guests ephemeral (memory only). `/register` → inactive until `/activate`.

## Cryptography

Reversible: `include/hybbx/crypto.h`. Password hashing: `hybbx/password.h`. Bundled by default; optional OpenSSL/libsodium at build time.

```ini
[crypto]
password_hash = tinysha256
aes_gcm = tinyaes
chacha = monocypher
x25519 = monocypher
random = system
```

| Algorithm | Use |
|-----------|-----|
| AES-256-GCM | NIST AEAD |
| XChaCha20-Poly1305 | Extended-nonce ChaCha |
| X25519 + XChaCha20-Poly1305 | Sealed messages |

Bundled: [Monocypher](third_party/monocypher/), [tiny-AES-c](third_party/tinyaes/), [tinysha256](third_party/tinysha256/).

## Authentication & privileges

**Guests** (`Guest1`…`Guest25`): ephemeral, max 25 concurrent, not in user DB. `auto_login=yes` → slot on connect. `auto_login=no` → login prompt; `/login` and `/register` for registered accounts only.

Pre-login commands: `/help`, `/motd`, `/news`, `/rules`, `/login`, `/register`, `/clear`, `/echo`, `/exit`.

**Username:** 4–12 chars, `a`–`z`, ≤4 digits, one `_` or `-` max.

| Action | Who |
|--------|-----|
| `/register` | Guest or login-prompt |
| `/changeme` | Registered users |
| `/changeuser` | Sysop: Admin/Mod/User; Admin: Mod/User (alias: `/userchange`) |
| `/usercreate`, `/activate` | Sysop, Admin (alias: `/createuser`) |
| Promote → Admin | Sysop |
| Promote → Mod | Sysop, Admin |
| Demote | Sysop (Admin), Sysop/Admin (Mod) |
| `/delete` | Sysop, Admin (not Sysop) |
| `/deleteuser` | Sysop (not self, not Sysop target; alias: `/userdelete`) |
| `/deleteme` | Registered (not Sysop) |

`max_online` default 35. Guest timeout: `guest_timeout_minutes` (default 30).

## Text files

```ini
[texts]
path = ./text
```

| File | When |
|------|------|
| `banner.txt` | At connect (guests and all users) |
| `motd.txt` | `/motd` (guests); also shown automatically after registered `/login` |
| `news.txt` | `/news` |
| `rules.txt` | `/rules` (`/legal`) — legal notice and acceptable use (`text/rules.txt`) |

Registered users see `motd.txt` only after a successful `/login` (not on guest connect). Guests see `banner.txt` on connect; use `/motd` on demand.

Banner tokens: `@version@`, `@service@`. MOTD tokens: `@username@`.

## Commands

| Command | Description |
|---------|-------------|
| `/help` | List or explain commands |
| `/news`, `/motd` | News / MOTD |
| `/rules` | Legal notice and acceptable use (`/legal`, `rules.txt`) |
| `/who` | Online users and connection type (telnet, ax25, …); alias `/online` |
| `/users` | Registered user levels as percentages (sum 100%); total count |
| `/session` | Session info (`/info`) |
| `/version` | HyBBX version and host OS name (`/ver`) |
| `/leave` | Up one area (`/back`) |
| `/main` | Main area (`/menu`) |
| `/clear` | Clear screen and input (`/cls`, `/reset`) |
| `/echo` | Show or toggle input echo (`/echo yes\|no`) |
| `/chat` | List/join channels; `show`, `showall` |
| `/conference` | Private two-user channel (`/meeting`) |
| `/mail` | Inbox; `/mail list 1-15`, `read`, `delete`, `send` |
| `/login <user> <pass>` | Login |
| `/register <user> …>` | Self-registration (guests; includes password) |
| `/changeme <oldpass> <newpass> <name> …>` | Update own profile and password (newpass 8–24 chars) |
| `/changeuser <user> <newpass> <name> …>` | Staff overwrite profile and password (alias: `/userchange`) |
| `/deleteuser <user>` | Sysop delete any account except Sysop (alias: `/userdelete`) |
| `/usercreate <user> …>` | Create user account (alias: `/createuser`) |
| `/activate`, `/promote`, `/demote`, `/delete` | Staff (Sysop, Admin) |
| `/shutdown` | Stop the HyBBX daemon (Sysop only) |
| `/restart` | Stop and re-exec HyBBX (Sysop only) |
| `/broadcast` | AX.25 RF or TCP broadcast (Sysop, Main only) |
| `/deleteme yes\|no` | Delete own account |
| `/exit` | Disconnect (`/quit`, `/logout`, `/bye`) |

### Chat

`/chat <number>` or `/chat <name>`. Channel name = topic. Messages: `ME: …` / `<user>: …`. Max length: `message_max` (default 72). Output wraps at 80 columns.

`/chat show` — display names in your current channel only. `/chat showall` — all chat users as `nick@ChannelN`, sorted by channel, wrapped at 80 columns (conference sessions excluded).

**Conference** — `/conference <topic> <user>` (alias `/meeting`): invite; partner accepts `y`/`n`. On accept, **both** see `Conference: <topic>` and the same prompt as chat (`Each line is a message; /leave or /main to exit.`). **Chat** has no leave notice; **conference** notifies the partner (`<name> left the conference.`) when the other party leaves.

**Mail** (registered users): `/mail` or `/mail list` shows inbox (`*` = unread). `/mail list 1-15` shows messages 1–15; `/mail list 5-20` shows 5–20 only. `/mail read <n>`, `/mail delete <n>` or `/mail delete <from-to>` (moves to recycle bin). `/mail recycle` permanently empties the recycle bin. Deleted mail is auto-purged after `recycle_days` (default 10, `[mail]` in INI). `/mail send <user> <subject>` then body lines; `/mail done` sends. `/mail cancel`, `/leave`, or `/main` aborts compose.

### Broadcast (Main only)

Only the **Main** instance originates broadcasts. Remote Secondaries are RF extenders — they TX what Main sends over HBX.

**AX.25** broadcasts only on links that are **low-bandwidth and half-duplex together** (HyBBX QoS from `LINK_AUTH`). High-bandwidth or full-duplex links never receive RF broadcast frames.

Frequencies are **MHz only** — HyBBX does not use CB channel numbers (same channel number maps to different MHz in different countries).

| Command | Effect |
|---------|--------|
| `/broadcast ax25 list` | Configured MHz list |
| `/broadcast ax25 27.205 <text>` | QST on that MHz (matching Secondary) |
| `/broadcast ax25 all <text>` | Any connected low+half-duplex Secondary |
| `/broadcast tcp <text>` | **Stub** — logged on Main |

INI: `[broadcast]`, `[ax25]` with `frequency1 = 27.205` … (optional `frequencyN_label`). Secondary: `frequency_mhz = 27.205` in `[transport.packet_radioN]`; advertised in `LINK_AUTH`.

**Navigation:** `/leave` (`/back`) = one menu level up. `/main` (`/menu`) = main area from anywhere.

## Build and install

See [QUICKSTART.md](QUICKSTART.md) and [BUILD.md](BUILD.md). Install prefix: `<prefix>/hybbx/` (`hybbx`, `hybbx-start`, `hybbx.ini`, `text/`, `data/`, `logs/`).

## Licensing

HyBBX is licensed under **GNU GPL v3** — see [LICENSE.txt](../LICENSE.txt). **ARDOP** integration and planned **CRDOP** (Level 2, post–v1.0.0): [LICENSING.md](LICENSING.md), [CRDOP.md](CRDOP.md).

Bundled third-party code under `third_party/` is compiled into `hybbx_core` (or plugins) and retains its own license. When you distribute binaries, include attribution for these components as required by their licenses:

| Component | Path | License |
|-----------|------|---------|
| Monocypher | `third_party/monocypher/` | BSD-2-Clause **or** CC0-1.0 (dual-licensed; SPDX in headers) |
| tiny-AES-c | `third_party/tinyaes/` | Public domain ([kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c)) |
| tinysha256 | `third_party/tinysha256/` | The Unlicense (adapted from [983/SHA-256](https://github.com/983/SHA-256)) |

Optional build-time backends (OpenSSL, libsodium) are linked externally when enabled via CMake; their license terms apply to those libraries, not to the HyBBX sources themselves.
