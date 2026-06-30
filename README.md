# HyBBX

**HyBBX** is a plugin-based BBS/mailbox inspired service for classic terminal access, amateur packet radio, and future link types. The core is plain C99, configured with INI files, and optimized for slow links (40 columns, 2400 baud pacing).

**Version:** 0.1.0 (early development) — [full manual](docs/MANUAL.md)

## Features

- **Telnet** over TCP/IPv4 and IPv6 (default port **2323**)
- **Packet radio** via TNC2C (KISS/AX.25 over USB or RS-232; any host baud)
- **Guest auto-login**, registration
- **Multi-channel chat**, MOTD/banner/news from `text/`
- **INI user database** — up to **50 users per file**, sharded as `users/users.ini`, `users2.ini`, …
- **Bundled crypto** — SHA-256 passwords, AES-256-GCM, XChaCha20-Poly1305, X25519 sealed boxes (optional OpenSSL/libsodium at build time)
- **Flat-file storage** today; SQL backends planned

## Architecture

Internally HyBBX uses **TCP/IPv4+IPv6 only**. The application layer (sessions, commands, auth) sits on byte streams over internal TCP circuits. Wire protocols are **link adapters**:

| Layer | Role |
|-------|------|
| Application | Sessions, `/` commands, chat, users |
| Internal transport | TCP (loopback, LAN, tunnel) |
| Link adapters | Telnet (native TCP), packet radio (AX.25/KISS → TCP), SSH/WebSocket (planned) |

Telnet maps directly. Packet radio and future stacks terminate on the wire and map into an internal TCP endpoint — the core never speaks AX.25 directly.

## Quick start

**Requirements:** CMake 3.16+, GCC or Clang, POSIX (Linux/BSD).

```bash
git clone <your-repo-url>
cd hyBBX
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/hybbx.sh          # dev: uses local/hybbx.ini, telnet on 127.0.0.1:2323
```

```bash
telnet 127.0.0.1 2323
```

**Install** (self-contained tree under a prefix):

```bash
cmake --install build --prefix "$HOME/hybbx"
"$HOME/hybbx/bin/hybbx-start"
```

System install: `sudo cmake --install build --prefix /usr/local/hybbx`

## Configuration

Example: `share/hybbx.ini.example` or `local/hybbx.ini`.

```ini
[service]
name = hyBBX
max_online = 35

[storage]
backend = flatfile
path = ./data

[auth]
auto_login = yes
guest_prefix = Guest

[traffic]
baud = 2400
line_width = 40
pace_output = yes
ansi = no

[texts]
path = ./text

[chat]
message_max = 72

[transport.telnet]
enabled = yes
bind = 0.0.0.0
bind6 = ::
port = 2323

[transport.packet_radio]
enabled = no
tnc = tnc2c
protocol = kiss
device = /dev/ttyUSB0
baud = 2400
```

On first run, HyBBX creates a default **Sysop** account (`Sysop` / `Sysop`) if none exists. Change it after login.

### Users (INI shards)

Under `data/users/`:

```ini
[user.1]
id = 1
username = sysop
level = sysop
active = yes
created = 1710000000
fullname = System Operator
password = {sha256}…
```

Legacy `users.dat` is migrated automatically to INI shards.

## Commands

HyBBX commands start with `/`. Guests see a subset; staff commands require login and the right level.

| Command | Purpose |
|---------|---------|
| `/help` | List or explain commands |
| `/motd`, `/news` | Message of the day / news |
| `/who`, `/session`, `/version` | Online users, session info, version |
| `/login`, `/register` | Sign in / register (activation by Sysop/Admin) |
| `/chat` | Join a chat channel (registered users) |
| `/activate`, `/promote`, `/demote`, `/delete` | Staff user management |
| `/leave`, `/exit` | Leave area / disconnect |

Details: commands, privileges, telnet, packet radio, storage, crypto — [docs/MANUAL.md](docs/MANUAL.md).

## Transports

| Plugin | Status | Notes |
|--------|--------|-------|
| `telnet` | Working | IPv4 + IPv6, minimal RFC telnet |
| `packet_radio` | In progress | [TNC2C](https://www.landolt.de/info/afuinfo/tnc2c.htm), KISS, host-mode |
| `ssh` | Planned | Same session core as telnet |
| `websocket` | Planned | Behind Apache/nginx reverse-proxy only |

## Build options

| CMake option | Default | Description |
|--------------|---------|-------------|
| `HYBBX_BUILD_PLUGINS` | ON | Transport plugins |
| `HYBBX_PLUGIN_TELNET` | ON | Telnet plugin |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON | Packet radio plugin |
| `HYBBX_CRYPTO_OPENSSL` | OFF | Optional OpenSSL backends |
| `HYBBX_CRYPTO_LIBSODIUM` | OFF | Optional libsodium backends |
| `HYBBX_HARDENING` | ON | Compiler security flags |

```bash
CC=clang cmake -B build-clang -DCMAKE_BUILD_TYPE=Release
cmake -B build -DHYBBX_CRYPTO_OPENSSL=ON
```

AmigaOS cross-build: `cmake/Toolchain-AmigaOS.cmake`

## Repository layout

```
hyBBX/
  include/hybbx/     Public API
  src/core/          Service, sessions, storage, crypto, traffic
  plugins/telnet/    TCP telnet adapter
  plugins/packet_radio/  KISS/TNC2C adapter
  third_party/       tinysha256, Monocypher, tiny-AES (bundled)
  text/              banner.txt, motd.txt, news.txt
  share/             hybbx.ini.example
  scripts/hybbx.sh   Dev start helper
```

## Status & roadmap

- v0.1.0: core sessions, telnet, flat-file + INI users, chat, staff commands, 2400 baud traffic layer
- Next: packet radio → internal TCP circuit, SSH plugin, SQLite storage, WebSocket behind reverse-proxy

