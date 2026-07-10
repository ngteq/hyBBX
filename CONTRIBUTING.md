# Contributing

Standards: [AGENTS.md](AGENTS.md) (coding agents), [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) (humans).

```bash
git clone <repository-url> && cd hyBBX
./scripts/dev-setup.sh && ./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

## Fork

HyBBX is **GPL-3.0**. A fork is welcome — you do not need permission from the original author.

1. Keep [LICENSE.txt](LICENSE.txt) and GPL notices in distributions.
2. Ship corresponding source with binaries (or offer it per GPL-3.0).
3. Mark your changes in commit history and docs where behaviour differs.

The tree is meant to be continued without the original maintainer:

| Start here | Why |
|------------|-----|
| [AGENTS.md](AGENTS.md) | Rules for AI coding assistants |
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | Layout, architecture, conventions |
| [docs/TOPOLOGY.md](docs/TOPOLOGY.md) | Main, Secondary, HBX circuit |
| [share/commands.yaml](share/commands.yaml) | Command registry (runtime) |
| `src/main.c` | Plugin registration, daemon entry |
| `src/core/` | Sessions, storage, circuit, commands |

Technical docs only in `docs/` — operator manual, build, transports. No roadmap or personal notes required.

## AI-assisted development

Point your agent at **AGENTS.md** first, then the doc map there. Typical tasks:

- INI / operator behaviour → [docs/MANUAL.md](docs/MANUAL.md) + `share/*.ini.example`
- Commands / help → `share/commands.yaml`, [docs/COMMANDS.md](docs/COMMANDS.md), `src/core/commands_registry.c`
- New transport → `plugins/`, register in `src/main.c`, add a short doc under `docs/`

Keep diffs small; match existing C99 style (`hybbx_` prefix, `hybbx_result_t`).

## Pull requests

- Focused diffs; Main/Secondary model unchanged unless discussed
- Behaviour / INI change → [docs/MANUAL.md](docs/MANUAL.md) + `share/*.ini.example`
- Verify: `cmake --build build`; telnet session if session/core touched

Issues: `.github/ISSUE_TEMPLATE/`.
