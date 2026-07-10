# Development

[AGENTS.md](../AGENTS.md) · [REPOSITORY.md](REPOSITORY.md) · **v1.7.5**

## Toolchain

CMake 3.16+, GCC or Clang, pthread.

```bash
./scripts/dev-setup.sh
```

## Architecture

```
Users → telnet | ssh | websocket → Main (sessions, storage)
Secondary / proxy network / RF plugins → HBX circuit :7323 only
```

- No wire-protocol parsing in `src/core/`
- Inter-node: HBX/Circuit + `LINK_AUTH` only
- Built-in `[security]` in `src/core/security_ban.c` — network abuse + excessive spam; policy in [SECURITY.md](SECURITY.md)
- Plugins: `hybbx_transport_plugin_t` — register in `src/main.c`

## Commands

Registry: [share/commands.yaml](../share/commands.yaml). Layout: [COMMANDS.md](COMMANDS.md).

- User groups: Sysop, Admin, Mod, User, Guest — never “Staff”
- `/help` and `/menu` (no args): filtered menu for this session
- `/index`: full command-index for every account
- `/alias`: alias map
- Help topics: two lines, ≤80 cols, no square brackets in session text
- `/broadcast` / `/announce`: Sysop → all online sessions on local Main (`hybbx_broadcast_announce`)
- Proxy network: user services only; no admin commands over proxy links

## Conventions

- C99, `hybbx_` prefix, `hybbx_result_t`
- INI booleans: `hybbx_parse_bool()`
- Limits: `include/hybbx/limits.h`

## Testing

```bash
cmake -B build -DHYBBX_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

**Verified:** telnet session path. **Built:** SSH, WebSocket. **AX.25/RF:** local tests only.

## Doc duty

| Change | Update |
|--------|--------|
| Behavior | FEATURES.md |
| Commands | commands.yaml, COMMANDS.md, MANUAL.md, commands_registry.c |
| INI/operator | MANUAL.md + `share/*.ini.example` (Linux-based paths) |
| Build | BUILD.md |
| Planned | ROADMAP.md |

Git: `user.name=ngteq`, empty email.
