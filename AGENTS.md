# HyBBX — agent & contributor guide

Instructions for **AI coding agents** and human developers working in this repository.

## Project in one paragraph

**HyBBX** is a C99, plugin-based **service solution for different connection types** (telnet, packet radio, …). It is **based on and inspired by** classic BBS/mailbox culture — **not** a new BBS product. **HyBBX uses a centralized daemon and link/repeater daemon technologies to expand networks, range, and features** ([docs/ROADMAP.md](docs/ROADMAP.md)).

Internally the core uses **TCP/IPv4+IPv6 + HBX framing** only. Wire protocols (telnet, KISS, AX.25) live in **link adapter** plugins under `plugins/`.

## Documentation map (read first)


| File                                       | Use when                                               |
| ------------------------------------------ | ------------------------------------------------------ |
| [README.md](README.md)                     | Short presentation                                     |
| [docs/INDEX.md](docs/INDEX.md)             | Full doc index                                         |
| [docs/FEATURES.md](docs/FEATURES.md)       | **What exists** — update when adding/changing features |
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | **How to code** — style, architecture, workflow        |
| [docs/ROADMAP.md](docs/ROADMAP.md)         | Architecture standard; planned mail-area and edge daemons |
| [docs/MANUAL.md](docs/MANUAL.md)           | Operator reference (INI, transports, commands)         |
| [docs/BUILD.md](docs/BUILD.md)             | CMake options                                          |
| [docs/PLATFORMS.md](docs/PLATFORMS.md)     | GCC/Clang targets (Win10+, macOS, AmigaOS 3.9+, …)     |
| [docs/REPOSITORY.md](docs/REPOSITORY.md)   | Directory layout                                       |
| [CONTRIBUTING.md](CONTRIBUTING.md)         | PR checklist, git policy                               |




## Architecture rules (do not break)

0. **Architecture standard** — HyBBX uses a **centralized daemon** and **link/repeater daemon technologies** to expand networks, range, and features ([docs/ROADMAP.md](docs/ROADMAP.md)).
1. **Core never parses on-air radio formats** — no KISS/AX.25 in `src/core/` except HBX circuit handling (`circuit.c`, `circuit_tcp.c`).
2. **Link adapters** implement `hybbx_transport_plugin_t` ([include/hybbx/plugin.h](include/hybbx/plugin.h)).
3. **Packet radio** bridges RF ↔ internal HBX/TCP (`circuit_host` / `[circuit]` hub), not direct session I/O.
4. **Boolean INI values** — use `hybbx_parse_bool()` / `hybbx_config_get_bool()` ([include/hybbx/util.h](include/hybbx/util.h)): canonical `yes`/`no`, aliases `true`/`false`, `enable`/`disable`, `on`/`off`, `1`/`0`.
5. **Bounded buffers** — respect [include/hybbx/limits.h](include/hybbx/limits.h); use `hybbx_strlcpy`, `hybbx_path_join`.
6. **C99** — `CMAKE_C_STANDARD 99`, extensions off. Match existing naming (`hybbx_`*, `snake_case` files).
7. **GCC or LLVM/Clang** — see [docs/PLATFORMS.md](docs/PLATFORMS.md).
8. **Minimal diffs** — only change what the task requires; match surrounding style.



## Build & verify

```bash
./scripts/dev-setup.sh    # or: cmake -B build && cmake --build build
./scripts/hybbx.sh
```

Tests (optional): `cmake -B build -DHYBBX_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build`

## Adding a feature (checklist)

1. Read [docs/FEATURES.md](docs/FEATURES.md) and [docs/ROADMAP.md](docs/ROADMAP.md) — avoid duplicating planned work.
2. Implement with architecture rules above.
3. Update **docs/FEATURES.md** (status table) and **docs/MANUAL.md** / **share/hybbx.ini.example** if user-facing.
4. Build successfully before finishing.
5. Do **not** commit unless the user asks.



## Git / public history (maintainer policy)

- **Author:** `user.name=ngteq`, `user.email` **empty** (repo-local).
- **No** `Co-authored-by: Cursor` or other agent attribution in commit messages.
- User may push; agents should not force-push `main` without explicit request.



## Key paths

```
include/hybbx/     Public API
src/core/          Application + circuit hub
plugins/telnet/    TCP telnet adapter
plugins/packet_radio/  AX.25, TNC, KISS, 6PACK, HBX client
share/hybbx.ini.example
local/hybbx.ini    Dev config (not installed)
```



## License

GPL-3.0 — [LICENSE.txt](LICENSE.txt). Bundled third-party: [docs/MANUAL.md](docs/MANUAL.md#licensing).