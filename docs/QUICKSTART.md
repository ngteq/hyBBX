# HyBBX quick start

Get HyBBX running locally in a few minutes. HyBBX is a **BBS/mailbox-inspired service** for multiple link types — not a classic BBS replacement. Full reference: [MANUAL.md](MANUAL.md). Feature overview: [FEATURES.md](FEATURES.md).

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

On first run with an empty user database, HyBBX creates a default **Sysop** account:

- Username: `Sysop`
- Password: `Sysop`

Change this after login. User files live under the configured storage path (`[storage] path`); see [MANUAL.md — Storage](MANUAL.md#storage).

## Configuration

Copy or edit `share/hybbx.ini.example` or `local/hybbx.ini`. Boolean keys use the HyBBX standard (`yes`/`no` and aliases — see [MANUAL.md — Boolean values](MANUAL.md#boolean-values-hybbx-standard)).

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
