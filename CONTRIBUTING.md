# Contributing to HyBBX

Human and AI-assisted contributions follow the same standards.

## Start

1. [README.md](README.md), `share/hybbx.ini.example`, [docs/FEATURES.md](docs/FEATURES.md)
2. [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) — coding rules
3. AI agents: [AGENTS.md](AGENTS.md) first

```bash
git clone git@github.com:ngteq/hyBBX.git && cd hyBBX
./scripts/dev-setup.sh && ./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

## PR expectations

- Focused changes; respect Main/Secondary architecture ([ROADMAP.md](docs/ROADMAP.md))
- Update [FEATURES.md](docs/FEATURES.md) for behavior changes; [MANUAL.md](docs/MANUAL.md) / INI for operator changes
- GCC or Clang ([PLATFORMS.md](docs/PLATFORMS.md)); GPL-3.0 compatible

Use PR template. Test: `cmake --build build`, telnet login check, packet radio if touched.

## Git identity (public)

```ini
user.name=ngteq
user.email=
```

No `Co-authored-by` tool attribution in commit messages.

## Issues

Templates under `.github/ISSUE_TEMPLATE/`. Keep discussion technical.
