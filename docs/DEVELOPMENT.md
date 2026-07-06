# Development

[AGENTS.md](../AGENTS.md) · [REPOSITORY.md](REPOSITORY.md) · **v1.0.2**

## Toolchain

CMake 3.16+, GCC or Clang, pthread.

```bash
./scripts/dev-setup.sh
```

## Architecture

```
Session core ←→ telnet | ssh | websocket | HBX circuit ←→ packet_radio | ardop | crdop
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

**Verified:** telnet session path. **Built:** SSH, WebSocket. **AX.25/RF:** local tests only.

## Doc duty

| Change | Update |
|--------|--------|
| Behavior | FEATURES.md |
| INI/operator | MANUAL.md + `share/*.ini.example` (generic Linux paths) |
| Build | BUILD.md |
| Planned | ROADMAP.md |

Git: `user.name=ngteq`, empty email.
