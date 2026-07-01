# Contributing to HyBBX

Thank you for helping improve HyBBX. This project welcomes **human and AI-assisted** contributions when they follow the same standards.

## Start here

1. Read [README.md](README.md), `share/hybbx.ini.example`, [docs/FEATURES.md](docs/FEATURES.md).
2. Read [docs/FEATURES.md](docs/FEATURES.md) — current capabilities and status.
3. Read [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) — coding rules and workflow.
4. **AI agents:** read [AGENTS.md](AGENTS.md) first.

## Quick setup

```bash
git clone git@github.com:ngteq/hyBBX.git
cd hyBBX
./scripts/dev-setup.sh
./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

Details: [docs/QUICKSTART.md](docs/QUICKSTART.md), [docs/BUILD.md](docs/BUILD.md).

## What we look for

- **Focused changes** — one concern per PR when possible.
- **Architecture respected** — Main + Secondary layout ([AGENTS.md](AGENTS.md), [docs/ROADMAP.md](docs/ROADMAP.md)).
- **Docs updated** — [docs/FEATURES.md](docs/FEATURES.md) for functional changes; [docs/MANUAL.md](docs/MANUAL.md) / `share/hybbx.ini.example` for config/operator changes.
- **GCC** or **LLVM/Clang** — [docs/PLATFORMS.md](docs/PLATFORMS.md)
- **GPL-3.0** compatible — see [LICENSE.txt](LICENSE.txt).

## Pull requests

Use the PR template (`.github/pull_request_template.md`). Include:

- What changed and why
- How you tested (build, telnet, packet radio if relevant)
- FEATURES.md / MANUAL updates (yes/no)

## Git identity (public repository)

Commits on `ngteq/hyBBX` use:

```ini
user.name=ngteq
user.email=
```

(empty email — do not use personal or agent addresses in public history)

**No** `Co-authored-by: Cursor`, Copilot, or other tool attribution in commit messages.

## Communication

- **Bugs / ideas:** open a GitHub issue (templates under `.github/ISSUE_TEMPLATE/`).
- **Questions:** issue or discussion on the repo.

## Code of conduct

Be constructive and respectful. HyBBX serves amateur radio and retro-terminal communities — keep discourse technical and welcoming.
