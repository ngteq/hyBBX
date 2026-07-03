# HyBBX — agent & contributor guide

For AI agents and developers. Humans: [CONTRIBUTING.md](CONTRIBUTING.md).

## Project

C99 multi-transport session daemon (mail/chat/commands + link adapters). **Main** + **Secondary** topology: Main hosts users, storage, telnet, and the HBX hub; **Secondaries** are **remote edge machines** (separate HyBBX processes with `circuit_host` → Main) — not telnet users or local `[transport.*]` on Main. Default Main: TCP/IP + HBX (`ax25=no`). **0.8.x** line; `RELEASE-*` docs from **v1.0.0**. INI: `share/hybbx.ini.example`, `share/hybbx-secondary.ini.example`.

## Doc map

| File | Use when |
|------|----------|
| [FEATURES.md](docs/FEATURES.md) | What exists — update on feature changes |
| [DEVELOPMENT.md](docs/DEVELOPMENT.md) | How to code |
| [ROADMAP.md](docs/ROADMAP.md) | Planned work |
| [MANUAL.md](docs/MANUAL.md) | Operator INI/commands |
| [BUILD.md](docs/BUILD.md) | CMake |
| [REPOSITORY.md](docs/REPOSITORY.md) | Tree layout |

## Architecture rules

1. Match [FEATURES.md](docs/FEATURES.md) and [ROADMAP.md](docs/ROADMAP.md); no `RELEASE-*` before v1.0.0.
2. Core (`src/core/`) = sessions + HBX/TCP only — **no KISS/AX.25/telnet wire parsing** in core.
3. Link adapters: `hybbx_transport_plugin_t` in `plugins/`.
4. Packet radio on **remote Secondaries** bridges RF ↔ HBX/TCP (`circuit_host` / `[circuit]` hub on Main).
5. Booleans: `hybbx_parse_bool()` — canonical `yes`/`no` ([util.h](include/hybbx/util.h)).
6. Bounded buffers: [limits.h](include/hybbx/limits.h); `hybbx_strlcpy`, `hybbx_path_join`.
7. C99, `hybbx_*` prefix, minimal diffs, match existing style.

## Build

```bash
./scripts/dev-setup.sh
./scripts/hybbx.sh
ctest --test-dir build   # with -DHYBBX_BUILD_TESTS=ON
```

Do not commit unless the user asks.

## Git (public)

`user.name=ngteq`, empty `user.email`. No `Co-authored-by` agent lines.

## Key paths

```
include/hybbx/     Public API
src/core/          Session, storage, circuit hub
plugins/telnet/    TCP adapter
plugins/packet_radio/  AX.25, TNC, HBX client
share/hybbx.ini.example
local/hybbx.ini    Dev only
```

GPL-3.0 — [LICENSE.txt](LICENSE.txt)
