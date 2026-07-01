# HyBBX documentation

Doc index. Short intro: [README.md](../README.md). Config reference: `share/hybbx.ini.example`.

## For contributors

| Document | Audience | Contents |
|----------|----------|----------|
| **[CONTRIBUTING.md](../CONTRIBUTING.md)** | Humans | How to contribute, PR checklist, git policy |
| **[AGENTS.md](../AGENTS.md)** | AI agents | Architecture rules, doc map, build, git policy |
| **[DEVELOPMENT.md](DEVELOPMENT.md)** | Developers | C99 style, plugins, common tasks, testing |

## Reference

| Document | Audience | Contents |
|----------|----------|----------|
| **[FEATURES.md](FEATURES.md)** | Everyone | **Start here** for what HyBBX does — maintained feature list (Done / Planned) |
| **[QUICKSTART.md](QUICKSTART.md)** | Operators | Clone, build, run, install, first telnet session |
| **[MANUAL.md](MANUAL.md)** | Operators & developers | Full reference: config, transports, packet radio, auth, commands, crypto |
| **[CLIENTS.md](CLIENTS.md)** | Operators | Standalone `hybbx-telnet` / `hybbx-terminal` (no server build) |
| **[ROADMAP.md](ROADMAP.md)** | Everyone | Planned work (`RELEASE-*` docs from v1.0.0) |
| **[REPOSITORY.md](REPOSITORY.md)** | Developers | Source tree layout and main directories |
| **[BUILD.md](BUILD.md)** | Developers | CMake options, toolchains, hardening |
| **[PLATFORMS.md](PLATFORMS.md)** | Developers | GCC/Clang on Windows 10+, macOS X+, AmigaOS 3.9+, Linux/BSD |

## Also useful

- Main config: [share/hybbx.ini.example](../share/hybbx.ini.example)
- Secondary config: [share/hybbx-secondary.ini.example](../share/hybbx-secondary.ini.example)
- Dev config: [local/hybbx.ini](../local/hybbx.ini)
- License: [LICENSE.txt](../LICENSE.txt)
- PR template: [.github/pull_request_template.md](../.github/pull_request_template.md)
- CI workflow: [.github/workflows/ci.yml](../.github/workflows/ci.yml)
- Cursor rules: [.cursor/rules/](../.cursor/rules/)
- Editor defaults: [.editorconfig](../.editorconfig)

## Maintainers

- Update [FEATURES.md](FEATURES.md) when changing shipped functionality.
- Update [ROADMAP.md](ROADMAP.md) for planned work. Per-version `docs/RELEASE-*` files begin at **v1.0.0**.
- **Git / public pushes:** `user.name=ngteq`, `user.email` empty. No `Co-authored-by` or agent attribution in commit messages.
