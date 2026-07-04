# HyBBX — agent guide

Humans: [CONTRIBUTING.md](CONTRIBUTING.md). **v1.0.0** telnet-session release — see [docs/RELEASE-1.0.0.md](docs/RELEASE-1.0.0.md).

## Product

Plugin-only session daemon. **Main** = users + telnet + HBX hub. **Secondary** = remote edge (HBX client to Main). No modem DSP inside `hybbx`.

| In tree | External |
|---------|----------|
| `src/core/`, `plugins/telnet` | TNC, KISS, sound-card apps |
| `plugins/packet_radio` | Serial/USB TNC |
| `plugins/ardop`, `plugins/crdop` | ARDOPC, CRDOPC |

## Rules

1. Core = sessions + HBX/TCP only — no KISS/AX.25/telnet wire parsing in `src/core/`
2. Plugins: `hybbx_transport_plugin_t` in `plugins/`
3. Booleans: `hybbx_parse_bool()` — `yes`/`no`
4. Buffers: [limits.h](include/hybbx/limits.h)
5. Doc changes: feature → FEATURES.md; INI → MANUAL.md + `share/*.ini.example`

## Doc map (`docs/` = full technical reference)

| File | Use |
|------|-----|
| [RELEASE-1.0.0.md](docs/RELEASE-1.0.0.md) | v1.0.0 scope and verification |
| [FEATURES.md](docs/FEATURES.md) | Shipped vs partial |
| [MANUAL.md](docs/MANUAL.md) | INI + commands |
| [ROADMAP.md](docs/ROADMAP.md) | Post–1.0.0 |
| [BUILD.md](docs/BUILD.md) | CMake |

## Build

```bash
./scripts/dev-setup.sh
ctest --test-dir build   # -DHYBBX_BUILD_TESTS=ON
```

Do not commit unless the user asks. Git: `ngteq`, empty email.
