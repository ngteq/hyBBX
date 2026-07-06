# Quick start

**v1.0.1** — telnet, SSH, WebSocket. INI: [MANUAL.md](MANUAL.md).

## Requirements

CMake 3.16+, GCC or Clang — [PLATFORMS.md](PLATFORMS.md). **libssh** for SSH plugin.

## Build and run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

| Transport | Try |
|-----------|-----|
| SSH | `ssh 127.0.0.1 -p 3232` |
| WebSocket | [WEBSOCKET.md](WEBSOCKET.md) |

## Install

```bash
cmake --install build --prefix "$HOME"
"$HOME/hybbx/hybbx-start"
```

`<prefix>/hybbx/` — `hybbx`, `hybbx-start`, `hybbx.ini`, `keys/`, `reverse-proxy/`, `data/`.

## First login

Empty data dir → **Sysop** / **SysopPassword** (change with `/changeme`). `auto_login = yes` → guest; `/login` for registered users.

## Commands

`/help` in session — [MANUAL.md — Commands](MANUAL.md#commands).

## Next

[FEATURES.md](FEATURES.md) · [RELEASE-1.0.1.md](RELEASE-1.0.1.md) · [BUILD.md](BUILD.md)
