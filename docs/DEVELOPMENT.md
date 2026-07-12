# Development

[AGENTS.md](../AGENTS.md) · [CONTRIBUTING.md](../CONTRIBUTING.md) · **v2.0.0 (upcoming)**

## Toolchain

CMake 3.16+, GCC or Clang, pthread. **POSIX+ first** — shared code uses C99 and portable POSIX APIs; isolate OS-specific paths (see [PLATFORMS.md](PLATFORMS.md)). Keep `*BSD` (FreeBSD, NetBSD, OpenBSD, …), **AmigaOS 3.9+**, MacOS, and Windows builds working when you touch core, clients, or serial/socket code.

```bash
./scripts/dev-setup.sh
```

## Layout

API: `include/hybbx/`. Config: `share/`. Sessions and commands: `src/core/`. Transports: `plugins/`. Full INI: [MANUAL.md](MANUAL.md).

```
src/core/       session, storage, circuit, commands, commands_registry
src/clients/    hybbx-telnet, hybbx-ssh, hybbx-terminal
plugins/        telnet, ssh, websocket, packet_radio, entertain_* (chess, …)
share/          hybbx.ini.example, commands.yaml
```

## Architecture

```
Users → telnet | ssh | websocket → Main (sessions, storage)
Secondary / proxy network / RF plugins → HBX circuit :7323 only
```

- **Text-first:** core I/O is line-oriented plain text; `[traffic]` controls width, pace, optional ANSI
- **Extensible:** plugins add transports, entertain apps, or richer clients — no wire/game parsing in core
- **Portable:** POSIX+ C99 in shared code; platform branches in dedicated files — [PLATFORMS.md](PLATFORMS.md)
- No wire-protocol parsing in `src/core/`
- Inter-node: HBX/Circuit + `LINK_AUTH` only
- Built-in `[security]` in `src/core/security_ban.c` — [SECURITY.md](SECURITY.md)
- Plugins: `hybbx_transport_plugin_t` — register in `src/main.c`
- **Entertain Area:** every game/app is a plugin in `plugins/` — never `src/core/`. See [ENTERTAIN.md](ENTERTAIN.md).

## Commands

Registry: [share/commands.yaml](../share/commands.yaml). Layout: [COMMANDS.md](COMMANDS.md).

- User groups: Sysop, Admin, Mod, User, Guest — never “Staff”
- `/help` and `/menu` (no args): filtered menu for this session
- `/index`: full command-index for every account
- `/alias`: alias map
- Help topics: two lines, ≤80 cols, no square brackets in session text
- `/broadcast` / `/announce`: Sysop → all online sessions on local Main

## Conventions

- C99, `hybbx_` prefix, `hybbx_result_t`
- INI booleans: `hybbx_parse_bool()`
- Limits: `include/hybbx/limits.h`
- **Portability:** prefer POSIX+ APIs in `src/core/` and shared helpers; no Linux-only calls without a guarded fallback. Platform branches: `_WIN32`, `__AMIGA__`, `__APPLE__`, or dedicated files (e.g. `serial_port.c`). After serial/socket/client changes, confirm `*BSD` (FreeBSD, NetBSD, OpenBSD, …) and AmigaOS cross-build paths still compile.

## Testing

```bash
cmake -B build -DHYBBX_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

## Doc duty

| Change | Update |
|--------|--------|
| INI / operator | MANUAL.md + `share/*.ini.example` |
| Commands | commands.yaml, COMMANDS.md, commands_registry.c |
| Build | BUILD.md |
| Transport plugin | matching doc in `docs/` (TNCS, WEBSOCKET, …) |

Technical docs only — no planning or status matrices.
