# HyBBX quick start

Build, run, first telnet session. Reference: [MANUAL.md](MANUAL.md), [FEATURES.md](FEATURES.md).

## Requirements

- CMake 3.16+
- **GCC** or **LLVM/Clang**
- **Linux**, **BSD**, **macOS 10+**, **Windows 10+** (MinGW), or **AmigaOS 3.9+** cross-GCC — [docs/PLATFORMS.md](docs/PLATFORMS.md)

## Build and run (development)

```bash
git clone <your-repo-url>
cd hyBBX
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/hybbx.sh          # uses local/hybbx.ini
```

Connect:

```bash
telnet 127.0.0.1 2323
# or: ./build/src/clients/hybbx-telnet -H 127.0.0.1 -p 2323
```

The dev script starts HyBBX with telnet on **127.0.0.1:2323** and the internal circuit on loopback (see `[circuit]` in `local/hybbx.ini`).

## Install (prefix layout)

```bash
cmake --install build --prefix "$HOME/hybbx"
"$HOME/hybbx/bin/hybbx-start"
```

System-wide example:

```bash
sudo cmake --install build --prefix /usr/local/hybbx
```

Installed layout: `bin/hybbx`, `bin/hybbx-start`, `etc/hybbx.ini`, `text/`, `data/`, `lib/`. Override with `HYBBX_CONFIG` and `HYBBX_ROOT`.

## First login

On first run with an empty user database, HyBBX creates a default **Sysop** account in `data/users/users.ini` (when `[storage] path = data`):

- Username: `Sysop`
- Password: `SysopPassword` (8–24 characters required for new passwords via `/changeme`)

Change this after login. MOTD is shown automatically after a successful registered `/login` (guests use `/motd` only).

## Configuration

Copy or edit `share/hybbx.ini.example` (datacenter **Main** — TCP/IP + HBX by default) or `local/hybbx.ini` (loopback dev). Boolean keys use the HyBBX standard (`yes`/`no` and aliases — see [MANUAL.md — Boolean values](MANUAL.md#boolean-values-hybbx-standard)).

**Main default:** telnet + HBX circuit on; AX.25 off (runs on Secondary). **Override:** `ax25=yes` on Main for standalone/local TNC — see [MANUAL.md — Standalone Main](MANUAL.md#standalone-main-all-connections-local).

Minimal telnet-only example:

```ini
[transport.telnet]
enabled = yes
bind = 127.0.0.1
port = 2323
```

Packet radio, circuit hub, chat, and traffic profile are documented in [MANUAL.md](MANUAL.md).

## Commands at the prompt

HyBBX commands start with `/`. Type `/help` after connecting. Guest and staff command lists: [MANUAL.md — Commands](MANUAL.md#commands).

## Next steps

- [FEATURES.md](FEATURES.md) — what is implemented and planned
- [MANUAL.md](MANUAL.md) — operators’ full reference
- [BUILD.md](BUILD.md) — CMake options and cross-build
