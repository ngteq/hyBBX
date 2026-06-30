# HyBBX manual

Full reference for operators and developers. For a short overview and quick start, see [README.md](../README.md).

## Connection types

| Transport       | Status   | Description |
|-----------------|----------|-------------|
| TCP/IP Telnet   | Started  | Classic BBS access over TCP |
| Packet Radio    | Started  | TNC2C (KISS/AX.25), any baud; USB/RS232 |
| SSH             | Planned  | Secure shell (transport plugin) |
| WebSocket       | Planned  | Forward-proxy behind Apache/nginx only |

## Architecture

HyBBX uses **TCP/IP (IPv4 and IPv6) as the only internal network semantics**. The core never speaks AX.25, KISS, or other link protocols directly — only byte streams over internal TCP circuits. External technologies are **link adapters**.

| Layer | Responsibility |
|-------|----------------|
| **Application** | HyBBX session (lines, commands, chat, auth, storage) |
| **Internal transport** | TCP over IPv4/IPv6 (loopback, LAN, tunnel) |
| **Link adapters** | Telnet (native TCP), packet radio (AX.25/KISS → internal TCP), future stacks |

```
                    ┌─────────────────────────────────┐
                    │     hybbx_core (application)     │
                    │  sessions · commands · storage   │
                    └───────────────┬─────────────────┘
                                    │ byte stream
                    ┌───────────────▼─────────────────┐
                    │   internal TCP (IPv4 / IPv6)     │
                    └───────────────┬─────────────────┘
          ┌─────────────────────────┼─────────────────────────┐
   ┌──────▼──────┐          ┌───────▼───────┐         ┌───────▼───────┐
   │   telnet    │          │ packet_radio  │         │  ssh / ws     │
   │  (TCP in)   │          │ AX.25/KISS    │         │   (later)     │
   │             │          │ → TCP circuit │         │  → TCP in     │
   └─────────────┘          └───────────────┘         └───────────────┘
```

Plugins implement `hybbx_transport_plugin_t` in `include/hybbx/plugin.h`.

### Planned transports

| Plugin | INI section | Notes |
|--------|-------------|-------|
| `ssh` | `[transport.ssh]` | Same session core as telnet |
| `websocket` | `[transport.websocket]` | Local endpoint behind reverse-proxy; no public HTTP/TLS in HyBBX |

## Configuration (INI)

HyBBX reads `-c` / `--config`. Example: `share/hybbx.ini.example`.

### Telnet

```ini
[transport.telnet]
enabled = yes
bind = 0.0.0.0
bind6 = ::
port = 2323
; ipv4 = yes
; ipv6 = yes
```

Listens on IPv4 and IPv6 on the same port. HyBBX speaks minimal RFC telnet:

- **WILL ECHO**, **WILL SGA**; declines linemode/NAWS/terminal-type
- Accepts **CR**, **LF**, or **CRLF**
- Output: bare **LF** → **CRLF**
- **Backspace** / **Delete** edit the current line
- Ignores **IAC** and **NUL**

### Traffic (2400 baud profile)

```ini
[traffic]
baud = 2400
line_width = 40
pace_output = yes
ansi = no
```

Default: plain ASCII, 40-column wrap, paced 8N1 output. Set `ansi = yes` for telnet gray-on-black (`ANSI 37;40`). Keep `text/*.txt` lines within **40 characters** when possible.

### Packet radio / TNC2C

Primary TNC: [Landolt TNC2C](https://www.landolt.de/info/afuinfo/tnc2c.htm). RS232/V.24 (USB-serial) with **KISS** or **hostmode**.

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
clock_mhz = 4.9
kiss_on_startup = yes
```

| TNC2C feature | HyBBX |
|---------------|-------|
| RS232 host port | `device`, `device_type` |
| KISS / host-mode | `protocol = kiss` or `hostmode` |
| TCM 3105 @ 1200 | `modem = tcm3105`, `radio_baud = 1200` |
| 9600 / 19200 modems | `modem = 9600` / `19200` |
| 4.9 / 9.8 / 10 MHz | `clock_mhz` |
| Any host baud | `baud` (platform serial) |

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
channel1 = General
```

## Input routing

| Input | Scope | Handler |
|-------|-------|---------|
| `/help`, `/exit`, … | HyBBX | Core (`/` required) |
| `;` / `#` … | Comment | Ignored |
| plain text (non-chat) | Local mailbox | Ignored silently |
| plain text in chat | Chat | Broadcast to channel |

**Rules:** `/verb`, `/ verb`, `/command verb`; `/` alone → `/help`.

## Areas

| Area | Enter | Notes |
|------|-------|-------|
| `main` | Default at connect | `/leave` returns here |
| `mail` | (local mailbox) | |
| `chat` | `/chat` | Registered users only |

## Storage

```ini
[storage]
backend = flatfile
path = ./data
```

| Backend | Status |
|---------|--------|
| `flatfile` | Implemented |
| `sqlite`, `mysql`, `mariadb` | Planned |

### User files (INI, 50 per shard)

| Path | Purpose |
|------|---------|
| `users/users.ini` | Users 1–50 |
| `users/users2.ini` | Users 51–100 |
| `users/usersN.ini` | Further shards |
| `users.dat.migrated` | Legacy pipe-format after upgrade |

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

Other data files: `sessions.dat`, `guest.next`, `user.next`, `session.next`.

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

Guests (`Guest1` … `Guest111`) may use: `/help`, `/motd`, `/news`, `/login`, `/register`.

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
| `/leave` | Back to main |
| `/chat` | List/join channels |
| `/login <user> <pass>` | Login |
| `/register <user> <name> <country> <location> <email>` | Register |
| `/activate`, `/promote`, `/demote`, `/delete` | Staff |
| `/deleteme yes` | Delete own account |
| `/exit` | Disconnect (`/quit`, `/logout`, `/bye`) |

### Chat

`/chat <number>` or `/chat <name>`. Channel name = topic. Messages: `ME: …` / `<user>: …`. Max length: `message_max` (default 72). Output wraps at 40 columns.

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
| `HYBBX_BUILD_PLUGINS` | ON |
| `HYBBX_PLUGIN_TELNET` | ON |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON |
| `HYBBX_BUILD_TESTS` | OFF |
| `HYBBX_HARDENING` | ON |
| `HYBBX_WARNINGS_AS_ERRORS` | OFF |
| `HYBBX_CRYPTO_OPENSSL` | OFF |
| `HYBBX_CRYPTO_LIBSODIUM` | OFF |

### Hardening

With `HYBBX_HARDENING=ON`: probed warnings, stack protector, `_FORTIFY_SOURCE=2` (Release), RELRO/NOW + PIE on Linux. Bounded buffers in `include/hybbx/limits.h`, safe `hybbx_path_join`.

### AmigaOS

```bash
cmake -B build-amiga \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake \
  -DAMIGA_SDK_PATH=/path/to/amiga-sdk
```
