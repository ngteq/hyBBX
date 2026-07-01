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
| **[RELEASE-0.5.0.md](RELEASE-0.5.0.md)** | Everyone | **Active 0.5.x stability line**, scope and checklist |
| **[RELEASE-0.4.75.md](RELEASE-0.4.75.md)** | Everyone | Previous release (shipped) |
| **[ROADMAP.md](ROADMAP.md)** | Everyone | Post-release planned work |
| **[REPOSITORY.md](REPOSITORY.md)** | Developers | Source tree layout and main directories |
| **[BUILD.md](BUILD.md)** | Developers | CMake options, toolchains, hardening |
| **[PLATFORMS.md](PLATFORMS.md)** | Developers | GCC/Clang on Windows 10+, macOS X+, AmigaOS 3.9+, Linux/BSD |

## Also useful

- Example config: [share/hybbx.ini.example](../share/hybbx.ini.example)
- Dev config: [local/hybbx.ini](../local/hybbx.ini)
- License: [LICENSE.txt](../LICENSE.txt)
- PR template: [.github/pull_request_template.md](../.github/pull_request_template.md)
- CI workflow: [.github/workflows/ci.yml](../.github/workflows/ci.yml)
- Cursor rules: [.cursor/rules/](../.cursor/rules/)
- Editor defaults: [.editorconfig](../.editorconfig)

## Maintainers

- Update [FEATURES.md](FEATURES.md) when changing shipped functionality (0.5.x line: see [RELEASE-0.5.0.md](RELEASE-0.5.0.md)).
- Update [ROADMAP.md](ROADMAP.md) only for post-0.5.0 planned work.
- **Git / public pushes:** `user.name=ngteq`, `user.email` empty. No `Co-authored-by` or agent attribution in commit messages.
