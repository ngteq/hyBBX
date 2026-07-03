# HyBBX — agent & contributor guide

For AI agents and developers. Humans: [CONTRIBUTING.md](CONTRIBUTING.md).

## Project

C99 multi-transport session daemon (mail/chat/commands + link adapters). **Main** + **Secondary** topology: Main hosts users, storage, telnet, and the HBX hub; **Secondaries** are **remote edge machines** (separate HyBBX processes with `circuit_host` → Main) — not telnet users or local `[transport.*]` on Main. Default Main: TCP/IP + HBX (`ax25=no`). **0.8.x** line; `RELEASE-*` docs from **v1.0.0**. INI: `share/hybbx.ini.example`, `share/hybbx-secondary.ini.example`.

## Product boundary (default — non-negotiable)

**HyBBX is plugin-only:** session core + **host-client bridge plugins**. Modems, TNCs, sound-card software, ARDOPC, CRDOPC, and all RF/audio DSP run as **external services** the operator starts separately.

That is the **absolute standard** for this project — not an ARDOP/CRDOP special case. Anything that embeds a sound-modem, runs modem DSP, or captures/plays radio audio **inside the daemon** is a **different product**, not HyBBX. Do not add it here.

| In HyBBX | External (not HyBBX) |
|----------|-------------------------|
| `src/core/` sessions, HBX hub, storage | USB/serial TNC, KISS device |
| `plugins/telnet` | Direwolf, sound-card packet apps |
| `plugins/packet_radio` (serial/host bytes) | ARDOPC, ardopcf |
| `plugins/ardop` (host TCP to modem) | ARDOPC, ardopcf |
| `plugins/crdop` (CB host TCP) | ARDOPC, ardopcf (same external modem family) |

---

## Doc map

| File | Use when |
|------|----------|
| [FEATURES.md](docs/FEATURES.md) | What exists — update on feature changes |
| [DEVELOPMENT.md](docs/DEVELOPMENT.md) | How to code |
| [ROADMAP.md](docs/ROADMAP.md) | Planned work |
| [MANUAL.md](docs/MANUAL.md) | Operator INI/commands |
| [LICENSING.md](docs/LICENSING.md) | GPL, third-party, ARDOP/CRDOP |
| [CRDOP.md](docs/CRDOP.md) | CB digital protocol (Level 2, experimental) |
| [BUILD.md](docs/BUILD.md) | CMake |
| [REPOSITORY.md](docs/REPOSITORY.md) | Tree layout |

## Architecture rules

1. **Plugin-only / external modems** — [Product boundary](#product-boundary-default--non-negotiable); embedded modem DSP is out of scope (different project).
2. Match [FEATURES.md](docs/FEATURES.md) and [ROADMAP.md](docs/ROADMAP.md); no `RELEASE-*` before v1.0.0.
3. Core (`src/core/`) = sessions + HBX/TCP only — **no KISS/AX.25/telnet wire parsing** in core.
4. Link adapters: `hybbx_transport_plugin_t` in `plugins/` only.
5. Packet radio on **remote Secondaries** bridges RF ↔ HBX/TCP (`circuit_host` / `[circuit]` hub on Main).
6. Booleans: `hybbx_parse_bool()` — canonical `yes`/`no` ([util.h](include/hybbx/util.h)).
7. Bounded buffers: [limits.h](include/hybbx/limits.h); `hybbx_strlcpy`, `hybbx_path_join`.
8. C99, `hybbx_*` prefix, minimal diffs, match existing style.

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
