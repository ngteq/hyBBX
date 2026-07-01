# HyBBX documentation

Central index for all project documentation. The root [README.md](../README.md) is a short presentation only; details live here.

HyBBX is a **connection-type service platform** inspired by classic BBS/mailbox culture — not a new BBS or mailbox product. See [README.md](../README.md) for the positioning statement.

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
| **[ROADMAP.md](ROADMAP.md)** | Everyone | **1× central**; gateway, digipeater, repeater, link |
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

- Update [FEATURES.md](FEATURES.md) when adding or changing functionality.
- Update [ROADMAP.md](ROADMAP.md) for planned topology/features not yet coded.
- **Git / public pushes:** `user.name=ngteq`, `user.email` empty. No `Co-authored-by` or agent attribution in commit messages.
