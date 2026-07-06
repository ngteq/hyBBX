# Quick start

**v1.0.0** targets telnet sessions on Main. INI detail: [MANUAL.md](MANUAL.md).

## Requirements

CMake 3.16+, GCC or Clang — [PLATFORMS.md](PLATFORMS.md).

## Build and run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

Client alternative: `./build/src/clients/hybbx-telnet -H 127.0.0.1 -p 2323`

## Install

```bash
cmake --install build --prefix "$HOME"
"$HOME/hybbx/hybbx-start"
```

Tree: `<prefix>/hybbx/` — `hybbx`, `hybbx-start`, `hybbx.ini`, `keys/`, `data/`, `text/`, `web/`.

```bash
cd ~/hybbx && ./hybbx-start
```

Override: `HYBBX_CONFIG`, `HYBBX_ROOT`. WebSocket browser access: [WEBSOCKET.md](WEBSOCKET.md).

## First login

Empty data dir → default **Sysop** / **SysopPassword** (change with `/changeme`).

| Mode | Behavior |
|------|----------|
| `auto_login = yes` (Main default) | Guest slot, `/login` for registered users |
| `auto_login = no` | Login prompt |

After registered `/login`, MOTD from `text/motd.txt` is shown automatically.

## Main vs Secondary

| Role | Typical INI | Use |
|------|-------------|-----|
| Main | `share/hybbx.ini.example` | Telnet users + HBX hub |
| Secondary | `share/hybbx-secondary.ini.example` | RF edge → Main (not v1.0.0 verified on air) |

## Commands

`/help` in session. Full list: [MANUAL.md — Commands](MANUAL.md#commands).

## Next

[FEATURES.md](FEATURES.md) · [RELEASE-1.0.0.md](RELEASE-1.0.0.md) · [BUILD.md](BUILD.md)
