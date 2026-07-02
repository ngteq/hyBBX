# HyBBX documentation

Index. Intro: [README.md](../README.md). Main INI: `share/hybbx.ini.example`.

## Contributors

| Document | Audience | Contents |
|----------|----------|----------|
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Humans | PR checklist, git policy |
| [AGENTS.md](../AGENTS.md) | AI agents | Architecture rules, doc map |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Developers | C99 style, plugins, tasks |

## Reference

| Document | Audience | Contents |
|----------|----------|----------|
| [FEATURES.md](FEATURES.md) | Everyone | Feature status (update on code changes) |
| [QUICKSTART.md](QUICKSTART.md) | Operators | Build, run, first telnet session |
| [MANUAL.md](MANUAL.md) | Operators | INI, transports, auth, commands |
| [CLIENTS.md](CLIENTS.md) | Operators | `hybbx-telnet`, `hybbx-terminal` CLI |
| [ROADMAP.md](ROADMAP.md) | Everyone | Planned work (`RELEASE-*` from v1.0.0) |
| [REPOSITORY.md](REPOSITORY.md) | Developers | Source tree |
| [BUILD.md](BUILD.md) | Developers | CMake options, toolchains |
| [PLATFORMS.md](PLATFORMS.md) | Developers | GCC/Clang targets |

## Config and tooling

- Main: [share/hybbx.ini.example](../share/hybbx.ini.example)
- Secondary: [share/hybbx-secondary.ini.example](../share/hybbx-secondary.ini.example)
- Dev: [local/hybbx.ini](../local/hybbx.ini)
- CI: [.github/workflows/ci.yml](../.github/workflows/ci.yml)
- Cursor rules: [.cursor/rules/](../.cursor/rules/)

## Maintainers

- Functional changes → [FEATURES.md](FEATURES.md) + [MANUAL.md](MANUAL.md) / INI examples
- Planned work → [ROADMAP.md](ROADMAP.md)
- Public git: `user.name=ngteq`, empty `user.email`; no agent `Co-authored-by` trailers
