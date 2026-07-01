# HyBBX development guide

How to work on the codebase. AI agents: see also [AGENTS.md](../AGENTS.md). Humans: [CONTRIBUTING.md](../CONTRIBUTING.md).

## Toolchain

| Requirement | Version / notes |
|-------------|-----------------|
| CMake | 3.16+ |
| C compiler | **GCC** or **LLVM/Clang** — see [PLATFORMS.md](PLATFORMS.md) |
| OS | Linux, BSD, **macOS 10+**, **Windows 10+** (MinGW), **AmigaOS 3.9+** (cross), other POSIX |
| Threads | pthread (`Threads::Threads` in CMake) |

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or use the dev helper (also links `compile_commands.json` for your IDE):

```bash
./scripts/dev-setup.sh
```

`CMAKE_EXPORT_COMPILE_COMMANDS=ON` — use `build/compile_commands.json` locally for IDE navigation (gitignored; symlink created by `dev-setup.sh`).

## Repository layout

See [REPOSITORY.md](REPOSITORY.md). Summary:

- **`include/hybbx/`** — public headers only for stable API
- **`src/core/`** — service, sessions, config, storage, crypto, circuit hub
- **`plugins/*/`** — transport link adapters (static libs linked into `hybbx`)
- **`third_party/`** — vendored crypto; do not replace without license review

## Architecture constraints

### Internal transport

```
Application (session, commands, storage)
        ↕ byte stream
Internal TCP (IPv4/IPv6) + HBX v1 framing ([circuit])
        ↕
Link adapters: telnet | packet_radio | (future ssh, ws)
```

- **Do not** add KISS, AX.25, or telnet protocol parsing to `src/core/`.
- **Do** extend plugins or `circuit.c` / `circuit_tcp.c` for new wire formats.

### Architecture standard

HyBBX uses a **centralized daemon** and **link/repeater daemon technologies** to expand networks, range, and features. See [ROADMAP.md](ROADMAP.md).

### Transport plugin API

```c
/* include/hybbx/plugin.h */
hybbx_transport_plugin_t  /* name, kind, init, shutdown, start, stop, write */
```

Register in `src/main.c` via `hybbx_registry_register()`. INI section: `[transport.<name>]`, key `enabled = yes`.

**Packet radio** is special: `write` may be NULL; it uses the HBX circuit client to reach the session hub.

## Coding conventions

### Language

- **C99**, extensions off (`CMAKE_C_EXTENSIONS OFF`)
- Prefix public symbols: `hybbx_`
- Types: `hybbx_foo_t`, enums `hybbx_foo`
- Files: `snake_case.c` / `.h`
- Return errors as `hybbx_result_t` ([include/hybbx/types.h](include/hybbx/types.h))

### Style (match existing code)

- 4-space indent in C sources
- Opening brace on same line for functions (project style)
- Minimal comments — only non-obvious logic
- No drive-by refactors in unrelated files

### Configuration

- INI via [include/hybbx/config.h](include/hybbx/config.h)
- Plugin sections passed as `key=value;key=value` strings to `start()`
- **Booleans:** [include/hybbx/util.h](include/hybbx/util.h) — `hybbx_parse_bool()`, canonical `yes`/`no`

### Security

- `HYBBX_HARDENING=ON` by default ([BUILD.md](BUILD.md))
- Use `hybbx_strlcpy`, `hybbx_path_join`, limits in [include/hybbx/limits.h](include/hybbx/limits.h)
- No secrets in repo; `local/` is dev-only

## Common tasks

### Add an INI key

1. Parse in the relevant `*_config.c` or `hybbx_service_apply_config`.
2. Document in [MANUAL.md](MANUAL.md) and `share/hybbx.ini.example`.
3. Use `hybbx_parse_bool()` for yes/no keys.

### Add a `/` command

1. `src/core/command.c` — register verb, privilege check via `hybbx_auth_*`.
2. Document in [MANUAL.md](MANUAL.md) commands table.
3. Update [FEATURES.md](FEATURES.md).

### Add a transport plugin

1. `plugins/<name>/` — CMakeLists, plugin struct, config parser.
2. Register in `src/main.c` + top-level `plugins/CMakeLists.txt`.
3. Option `HYBBX_PLUGIN_<NAME>` in root `CMakeLists.txt`.
4. Document `[transport.<name>]` in MANUAL + hybbx.ini.example + FEATURES.

### Packet radio / TNC

- Stack under `plugins/packet_radio/` — `tnc.c`, `ax25.c`, `kiss.c`, profiles in `tnc_profiles.c`
- Public TNC API: [include/hybbx/tnc.h](include/hybbx/tnc.h)
- Duplex/band/G3RUH: finalize in `hybbx_tnc_finalize_*` helpers

## Testing

Enable tests:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHYBBX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CI runs the same on every push/PR (`.github/workflows/ci.yml`). Until more tests land, also:

1. `cmake --build build` — must succeed
2. `./scripts/hybbx.sh` + `telnet 127.0.0.1 2323` — smoke test
3. Packet radio — requires hardware/TNC; document manual steps in PR

## Documentation duty

| Change type | Update |
|-------------|--------|
| New/changed feature | [FEATURES.md](FEATURES.md) |
| Config / operator | [MANUAL.md](MANUAL.md), `share/hybbx.ini.example` |
| Build / CMake | [BUILD.md](BUILD.md) |
| Future / planned only | [ROADMAP.md](ROADMAP.md) |
| Layout / modules | [REPOSITORY.md](REPOSITORY.md) |

## Git (ngteq/hyBBX)

```ini
# .git/config (local)
user.name=ngteq
user.email=
```

No agent co-author trailers in public commits. See [CONTRIBUTING.md](../CONTRIBUTING.md).

## AI-assisted contributions

- Read [AGENTS.md](../AGENTS.md) before editing.
- Cursor rules: `.cursor/rules/` (project conventions).
- Prefer small, reviewable diffs; cite existing patterns with file paths.
