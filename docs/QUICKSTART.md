# Quick start

**v1.7.5** (testing) — telnet, SSH, WebSocket, built-in security bans. Topology: [TOPOLOGY.md](TOPOLOGY.md).

## Requirements

CMake 3.16+, GCC or Clang — [PLATFORMS.md](PLATFORMS.md). **libssh** for SSH plugin. **libsqlite3** optional for SQLite storage.

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

`<prefix>/hybbx/` — `hybbx`, `hybbx-start`, `hybbx.ini`, `keys/`, `reverse-proxy/`, `data/`, `logs/`.

## First login

Empty data dir → **Sysop** with a random password (printed on first start; change with `/changeme`). `auto_login = yes` → guest; `/login` for registered users.

## Layouts

| Site | Config |
|------|--------|
| Main only | `share/hybbx.ini.example` |
| Main + remote RF | Main + `share/hybbx-secondary.ini.example` on edge host |
| Linked BBS (future) | Two Mains + `mains_proxy` — [MAINS_PROXY.md](MAINS_PROXY.md) |

## Commands

`/help` in session — [MANUAL.md — Commands](MANUAL.md#commands).

## Next

[FEATURES.md](FEATURES.md) · [RELEASE-1.7.5.md](RELEASE-1.7.5.md) · [BUILD.md](BUILD.md)
