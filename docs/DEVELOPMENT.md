# Development

[AGENTS.md](../AGENTS.md) · [CONTRIBUTING.md](../CONTRIBUTING.md) · **v2.0.0 (upcoming)**

Fork-friendly: this file + AGENTS.md are the maintainer handoff for humans and coding agents.

## Toolchain

CMake 3.16+, GCC or Clang, pthread.

```bash
./scripts/dev-setup.sh
```

## Layout

API: `include/hybbx/`. Config: `share/`. Sessions and commands: `src/core/`. Transports: `plugins/`. Full INI: [MANUAL.md](MANUAL.md).

```
src/core/       session, storage, circuit, commands, commands_registry
src/clients/    hybbx-telnet, hybbx-ssh, hybbx-terminal
plugins/        telnet, ssh, websocket, packet_radio, …
share/          hybbx.ini.example, commands.yaml
```

## Architecture

```
Users → telnet | ssh | websocket → Main (sessions, storage)
Secondary / proxy network / RF plugins → HBX circuit :7323 only
```

- No wire-protocol parsing in `src/core/`
- Inter-node: HBX/Circuit + `LINK_AUTH` only
- Built-in `[security]` in `src/core/security_ban.c` — [SECURITY.md](SECURITY.md)
- Plugins: `hybbx_transport_plugin_t` — register in `src/main.c`

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

## Fork / new maintainer

1. [CONTRIBUTING.md](../CONTRIBUTING.md) — GPL obligations, fork checklist
2. [TOPOLOGY.md](TOPOLOGY.md) — do not break HBX-only inter-node rule
3. `include/hybbx/` — public API; extend before duplicating logic in plugins
4. Tests: `./scripts/dev-setup.sh` then `ctest --test-dir build` with `-DHYBBX_BUILD_TESTS=ON`
