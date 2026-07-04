# Development

[AGENTS.md](../AGENTS.md) · [REPOSITORY.md](REPOSITORY.md) · **v1.0.0**

## Toolchain

CMake 3.16+, GCC or Clang, pthread.

```bash
./scripts/dev-setup.sh
```

## Architecture

```
Session core ←→ telnet | HBX circuit ←→ packet_radio | ardop | crdop
```

- No wire-protocol parsing in `src/core/`
- No modem DSP in HyBBX — external TNC / ARDOPC / CRDOPC
- Plugins: `hybbx_transport_plugin_t` — register in `src/main.c`

## Conventions

- C99, `hybbx_` prefix, `hybbx_result_t`
- INI booleans: `hybbx_parse_bool()`
- Limits: `include/hybbx/limits.h`

## Testing

```bash
cmake -B build -DHYBBX_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

**v1.0.0 verified:** telnet session path. **AX.25/RF:** local tests only. Optional: `scripts/test-ardop-plugin.sh`, `scripts/test-crdop-plugin.sh`.

## Doc duty

| Change | Update |
|--------|--------|
| Behavior | FEATURES.md |
| INI/operator | MANUAL.md + `share/*.ini.example` |
| Build | BUILD.md |
| Planned | ROADMAP.md |

Git: `user.name=ngteq`, empty email.
