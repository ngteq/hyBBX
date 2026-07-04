# Contributing

Standards: [AGENTS.md](AGENTS.md) (agents), [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) (humans).

```bash
git clone git@github.com:ngteq/hyBBX.git && cd hyBBX
./scripts/dev-setup.sh && ./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

## Pull requests

- Small, focused diffs; Main/Secondary model unchanged unless discussed
- Behavior change → [docs/FEATURES.md](docs/FEATURES.md)
- INI/operator change → [docs/MANUAL.md](docs/MANUAL.md) + `share/*.ini.example`
- Verify: `cmake --build build`; telnet session if session/core touched

Git identity (public): `user.name=ngteq`, empty `user.email`. No `Co-authored-by` agent lines.

Issues: `.github/ISSUE_TEMPLATE/`.
