# HyBBX manual

INI, transports, commands. Config template: `share/hybbx.ini.example`. Arch: [ROADMAP.md](ROADMAP.md).

- [FEATURES.md](FEATURES.md) · [QUICKSTART.md](QUICKSTART.md) · [INDEX.md](INDEX.md)

## Connection types

| Transport       | Status   | `[networks]` switch | Description |
|-----------------|----------|---------------------|-------------|
| TCP/IP Telnet   | Started  | **static** (always on) | BBS-inspired terminal access over TCP |
| SSH             | Planned  | **static** (always on) | Secure shell (transport plugin) |
| AX.25 / Packet Radio | Started | `ax25 = yes\|no` | TNC2C (KISS/AX.25); USB/RS232 |
| WebSocket       | Planned  | `websocket = yes\|no` | Forward-proxy behind Apache/nginx only |
| HBX circuit hub | Started  | `circuit = yes\|no` | Internal TCP hub for edge link adapters |

Telnet and SSH ignore `enabled` in their `[transport.*]` sections — they start whenever the plugin is built. All other adapters are off unless `[networks]` enables them.

## Architecture

Plugins: `hybbx_transport_plugin_t` in `include/hybbx/plugin.h`. Core = TCP/IPv4+IPv6 + HBX only.

Arch (central + edge daemons): [ROADMAP.md](ROADMAP.md). Circuit INI: `[circuit]` in `share/hybbx.ini.example`.

### Planned transports

| Plugin | INI section | Notes |
|--------|-------------|-------|
| `ssh` | `[transport.ssh]` | Same session core as telnet |
| `websocket` | `[transport.websocket]` | Local endpoint behind reverse-proxy; no public HTTP/TLS in HyBBX |

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

```ini
[networks]
ax25 = no          ; packet_radio plugin (AX.25 / KISS / TNC)
websocket = no     ; planned
circuit = yes      ; HBX TCP hub for edge link/repeater daemons
```

| Key | Default | Controls |
|-----|---------|----------|
| `ax25` | `no` | `[transport.packet_radio]` (alias: `packet_radio`) |
| `websocket` | `no` | `[transport.websocket]` when implemented |
| `circuit` | `yes` | `[circuit]` HBX hub (also respects `[circuit] enabled`) |

Telnet and SSH are **not** listed here — they are static-enabled when compiled in.

### Internal circuit (HBX over TCP/IPv4+IPv6)

HyBBX core speaks **only TCP**. Link adapters (packet radio, future stacks) attach to an
internal circuit hub on loopback. Payloads are wrapped in **HBX v1** frames:

| HBX `proto` | Payload | Direction |
|-------------|---------|-----------|
| `ax25` (0x01) | Raw AX.25 frame incl. FCS | RF → core |
| `ax25_ui` (0x02) | Masked path + UI bytes | Optional metadata |
| `terminal` (0x10) | BBS byte stream | Core ↔ RF TX |

Reserved protocol IDs (`0x20` APRS, `0x21` NETROM, …) are allocated for future stacks.

```ini
[circuit]
enabled = yes
bind = 127.0.0.1
bind6 = ::1
port = 7323
ipv4 = yes
ipv6 = yes
```

Packet radio connects as a **link client** (`circuit_host` / `circuit_port` in
`[transport.packet_radio]`). The hub opens a `circuit` session and bridges HBX frames
to `hybbx_session` — the application never parses KISS or AX.25 directly.

```
  RF (AX.25)  →  TNC  →  packet_radio  →  HBX/TCP  →  circuit hub  →  session
  session     →  circuit hub  →  HBX/TCP  →  packet_radio  →  TNC  →  RF
```

Telnet remains native TCP to the session (no HBX wrapper on the wire).

### Telnet

```ini
[transport.telnet]
enabled = yes
bind = 0.0.0.0
bind6 = ::
port = 2323
ipv4 = yes
ipv6 = yes
```

Listens on IPv4 and IPv6 (`IPV6_V6ONLY` so both can share the same port). Set `ipv6 = no` to disable the v6 socket.

### hybbx-telnet (HyBBX client)

`hybbx-telnet` is the **official HyBBX telnet client** — pure CLI (parameters and environment variables only). No INI files, no GUI. Use it for testing and operator sessions against any HyBBX telnet transport.

```bash
hybbx-telnet -H 127.0.0.1 -p 2323
hybbx-telnet --baud 2400 --line-width 80
hybbx-telnet -u myuser -P secret
```

Environment: `HYBBX_HOST`, `HYBBX_PORT`. See [CLIENTS.md](CLIENTS.md).

### hybbx-terminal (AX.25 / circuit client)

`hybbx-terminal` is the **official HyBBX circuit terminal client** — pure CLI for AX.25 and HBX testing over the internal circuit hub.

```bash
hybbx-terminal -H 127.0.0.1 -p 7323
hybbx-terminal --link-id edge-1 --link-password changeme
hybbx-terminal --mycall DL1ABC-0 --dest DL9XYZ-0 --ax25-ui
```

Environment: `HYBBX_CIRCUIT_HOST`, `HYBBX_CIRCUIT_PORT`. See [CLIENTS.md](CLIENTS.md).

### Circuit link authentication

Edge adapters (packet radio, future link/repeater edge daemons) authenticate to `[circuit]` with **password only**. HyBBX does **not** use TCP/IP-style ping/pong or heartbeat health checks across links or protocols.

```ini
[circuit]
link_auth = yes
link_password = changeme
link_stale_days = 10

[transport.packet_radio]
link_id = edge-1
link_password = changeme
link_role = repeater
```

Successful auth records the link under `data/links/` and `[link.<id>]` in hybbx.ini. Links with no successful password auth for more than `link_stale_days` are **auto-removed** from config and storage.

Server telnet behaviour:

- **WILL ECHO**, **WILL SGA**; declines linemode/NAWS/terminal-type
- Accepts **CR**, **LF**, or **CRLF**
- Output: bare **LF** → **CRLF**
- **Backspace** / **Delete** edit the current line
- Ignores **IAC** and **NUL**

### Traffic (2400 baud profile)

```ini
[traffic]
baud = 2400
line_width = 80
pace_output = yes
ansi = no
input_echo = yes
```

Default: plain ASCII, **80-column** wrap, paced 8N1 output. `input_echo = yes` echoes typed/pasted characters to the client. Set `ansi = yes` for telnet gray-on-black (`ANSI 37;40`) and ANSI screen clear (`/clear`). Keep `text/*.txt` lines within **80 characters** when possible.

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
[transport.packet_radio]
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
```

Messages stored under `data/mail/<username>/inbox/` on the **centralized daemon** only.

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

`/leave` (`/back`) goes up **one** level (parent area). `/main` (`/menu`) returns to **main** directly and clears all sub-areas.

## Storage

```ini
[storage]
backend = flatfile
path = ~/.hybbx
```

Default flat-file layout under `$HOME/.hybbx/` (created on first start):

| Path | Purpose |
|------|---------|
| `users/users.ini` | User accounts |
| `mail/<user>/inbox/` | Personal mail |
| `sessions.dat`, `*.next` | Session counters |

`~` in `path` expands to `$HOME`. `hybbx-start` creates `$HOME/.hybbx` if needed.

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

| `sessions.dat`, `guest.next`, `user.next`, `session.next` | Counters / session log |
| `mail/<user>/inbox/*.msg` | Personal mail (centralized daemon) |

**Default Sysop** (if none exists): username `Sysop`, password `Sysop` — change after first login.

**Levels** (high → low): Sysop (one), Admin, Mod, User, Guest. `/register` creates inactive users until `/activate`.

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

Guests (`Guest1` … `Guest111`) may use: `/help`, `/motd`, `/news`, `/login`, `/register`, `/clear`, `/echo`.

**Username rules:** 4–12 chars, `a`–`z`, ≤4 digits, at most one `_` or `-` (not both).

| Action | Who |
|--------|-----|
| Activate `/register` accounts | Sysop, Admin |
| Promote → Admin | Sysop only |
| Promote → Mod | Sysop, Admin |
| Demote Admin | Sysop only |
| Demote Mod | Sysop, Admin |
| Delete Mod/User/Guest | Sysop, Admin |
| Delete Admin | Sysop only |
| Delete Sysop | Never |
| Delete self | `/deleteme yes` (not Sysop) |

`/help` is level-aware. Max **35** online sessions by default (`max_online`). Guests disconnect after **30** minutes (`guest_timeout_minutes`).

## Text files

```ini
[texts]
path = ./text
```

| File | When |
|------|------|
| `banner.txt` | At connect |
| `motd.txt` | After guest login |
| `news.txt` | `/news` |

Banner tokens: `@version@`, `@service@`.

## Commands

| Command | Description |
|---------|-------------|
| `/help` | List or explain commands |
| `/news`, `/motd` | News / MOTD |
| `/who` | Online users |
| `/session` | Session info (`/info`) |
| `/version` | Version (`/ver`) |
| `/leave` | Up one area (`/back`) |
| `/main` | Main area (`/menu`) |
| `/clear` | Clear screen and input (`/cls`, `/reset`) |
| `/echo` | Show or toggle input echo (`/echo yes\|no`) |
| `/chat` | List/join channels |
| `/mail` | Inbox; `/mail list 1-15`, `read`, `delete`, `send` |
| `/login <user> <pass>` | Login |
| `/register <user> <name> <country> <location> <email>` | Register |
| `/activate`, `/promote`, `/demote`, `/delete` | Staff |
| `/deleteme yes` | Delete own account |
| `/exit` | Disconnect (`/quit`, `/logout`, `/bye`) |

### Chat

`/chat <number>` or `/chat <name>`. Channel name = topic. Messages: `ME: …` / `<user>: …`. Max length: `message_max` (default 72). Output wraps at 80 columns.

**Mail** (registered users): `/mail` or `/mail list` shows inbox (`*` = unread). `/mail list 1-15` shows messages 1–15; `/mail list 5-20` shows 5–20 only. `/mail read <n>`, `/mail delete <n>` or `/mail delete <from-to>` (moves to recycle bin). `/mail recycle` permanently empties the recycle bin. Deleted mail is auto-purged after `recycle_days` (default 10, `[mail]` in INI). `/mail send <user> <subject>` then body lines; `/mail done` sends. `/mail cancel`, `/leave`, or `/main` aborts compose.

**Navigation:** `/leave` (`/back`) = one menu level up. `/main` (`/menu`) = main area from anywhere.

## Building & installing

CMake 3.16+, C99. GCC primary; Clang supported.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
CC=clang cmake -B build-clang -DCMAKE_BUILD_TYPE=Release
cmake -B build -DHYBBX_CRYPTO_OPENSSL=ON
```

Install layout under prefix: `bin/hybbx`, `bin/hybbx-start`, `etc/hybbx.ini`, `text/`, `data/`, `lib/`.

**Run installed:** `hybbx-start` (set `HYBBX_CONFIG`, `HYBBX_ROOT` to override).

**Dev:** `./scripts/hybbx.sh` → `local/hybbx.ini`, telnet `127.0.0.1:2323`.

### CMake options

| Option | Default |
|--------|---------|
| `HYBBX_BUILD_PLUGINS` | ON | Build transport plugins |
| `HYBBX_PLUGIN_TELNET` | ON | Telnet plugin |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON | Packet radio plugin |
| `HYBBX_BUILD_TESTS` | OFF | Test targets |
| `HYBBX_HARDENING` | ON | Compiler security flags |
| `HYBBX_WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors |
| `HYBBX_CRYPTO_OPENSSL` | OFF | OpenSSL crypto backends |
| `HYBBX_CRYPTO_LIBSODIUM` | OFF | libsodium ChaCha/X25519 backends |

**Platforms:** GCC and LLVM/Clang on Linux, BSD, macOS 10+, Windows 10+ (MinGW), AmigaOS 3.9+ (cross). See [PLATFORMS.md](PLATFORMS.md) and [BUILD.md](BUILD.md).

### Hardening

With `HYBBX_HARDENING=ON`: probed warnings, stack protector, `_FORTIFY_SOURCE=2` (Release), RELRO/NOW + PIE on Linux. Bounded buffers in `include/hybbx/limits.h`, safe `hybbx_path_join`.

### AmigaOS (3.9+, cross-GCC)

See [PLATFORMS.md](PLATFORMS.md) and [BUILD.md](BUILD.md).

```bash
cmake -B build-amiga \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake \
  -DAMIGA_SDK_PATH=/path/to/amiga-sdk
```

## Licensing

HyBBX is licensed under **GNU GPL v3** — see [LICENSE.txt](../LICENSE.txt).

Bundled third-party code under `third_party/` is compiled into `hybbx_core` (or plugins) and retains its own license. When you distribute binaries, include attribution for these components as required by their licenses:

| Component | Path | License |
|-----------|------|---------|
| Monocypher | `third_party/monocypher/` | BSD-2-Clause **or** CC0-1.0 (dual-licensed; SPDX in headers) |
| tiny-AES-c | `third_party/tinyaes/` | Public domain ([kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c)) |
| tinysha256 | `third_party/tinysha256/` | The Unlicense (adapted from [983/SHA-256](https://github.com/983/SHA-256)) |

Optional build-time backends (OpenSSL, libsodium) are linked externally when enabled via CMake; their license terms apply to those libraries, not to the HyBBX sources themselves.
