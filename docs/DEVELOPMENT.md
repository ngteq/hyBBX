# HyBBX development guide

AI agents: [AGENTS.md](../AGENTS.md). Humans: [CONTRIBUTING.md](../CONTRIBUTING.md).

## Toolchain

CMake 3.16+, GCC or Clang, pthread. [PLATFORMS.md](PLATFORMS.md).

```bash
./scripts/dev-setup.sh    # build + compile_commands.json symlink
```

## Layout

[REPOSITORY.md](REPOSITORY.md) — `include/hybbx/` (API), `src/core/`, `plugins/`, `third_party/` (vendored crypto).

## Architecture

```
Session core (commands, storage, mail, chat)
        ↕ byte stream
TCP/IPv4+IPv6 + HBX v1 ([circuit])
        ↕
Link adapters: telnet | packet_radio | (ssh, ws post–v1.0.0)
```

- No KISS/AX.25/telnet parsing in `src/core/`.
- Plugins: `hybbx_transport_plugin_t` ([plugin.h](../include/hybbx/plugin.h)); register in `src/main.c`.
- Packet radio: HBX circuit client; `write` may be NULL.

## Conventions

- C99, `hybbx_` prefix, `hybbx_result_t` errors, 4-space indent
- INI: [config.h](../include/hybbx/config.h); booleans via `hybbx_parse_bool()`
- Security: [limits.h](../include/hybbx/limits.h), `HYBBX_HARDENING=ON` ([BUILD.md](BUILD.md))

## Common tasks

| Task | Where |
|------|-------|
| INI key | Parse in config module; document MANUAL + `share/hybbx.ini.example` |
| `/` command | `command.c`; auth via `hybbx_auth_*`; update FEATURES + MANUAL |
| Transport plugin | `plugins/<name>/`, `main.c`, CMake option |
| TNC/radio | `plugins/packet_radio/`, [tnc.h](../include/hybbx/tnc.h) |

## Testing

```bash
cmake -B build -DHYBBX_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

Also: `./scripts/hybbx.sh` + telnet smoke test.

## Doc duty

| Change | Update |
|--------|--------|
| Feature | FEATURES.md |
| Config/operator | MANUAL.md, INI examples |
| Build | BUILD.md |
| Planned only | ROADMAP.md |
| Layout | REPOSITORY.md |

Git: `user.name=ngteq`, empty email; no agent co-author trailers.
