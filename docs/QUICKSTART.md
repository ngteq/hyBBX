# HyBBX quick start

Build, run, first telnet session. Reference: [MANUAL.md](MANUAL.md), [FEATURES.md](FEATURES.md).

## Requirements

CMake 3.16+, GCC or Clang. Linux, BSD, macOS 10+, Windows 10+ (MinGW), AmigaOS 3.9+ cross — [PLATFORMS.md](PLATFORMS.md).

## Build and run

```bash
git clone <your-repo-url>
cd hyBBX
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/hybbx.sh          # local/hybbx.ini → 127.0.0.1:2323
```

```bash
telnet 127.0.0.1 2323
# or: ./build/src/clients/hybbx-telnet -H 127.0.0.1 -p 2323
```

## Install

```bash
cmake --install build --prefix "$HOME"
"$HOME/hybbx/hybbx-start"
```

Layout: `<prefix>/hybbx/` — `hybbx`, `hybbx-start`, `hybbx.ini`, `text/`, `data/`, `logs/`. Override: `HYBBX_CONFIG`, `HYBBX_ROOT`.

## First login

Empty DB → default Sysop in `data/users/users.ini` (`[storage] path = data`):

- User: `Sysop` · Pass: `SysopPassword` (change via `/changeme`; new passwords 8–24 chars)
- MOTD auto-shown after registered `/login` (guests: `/motd` only)

## Configuration

Templates: `share/hybbx.ini.example` (Main), `local/hybbx.ini` (dev). Booleans: `yes`/`no` + aliases — [MANUAL.md](MANUAL.md#boolean-values-hybbx-standard).

Main default: telnet + HBX on, AX.25 off (Secondary). Standalone: `ax25=yes` on Main — [MANUAL.md](MANUAL.md).

## Commands

`/help` after connect. Full list: [MANUAL.md — Commands](MANUAL.md#commands).

## Next

[FEATURES.md](FEATURES.md) · [MANUAL.md](MANUAL.md) · [BUILD.md](BUILD.md)
