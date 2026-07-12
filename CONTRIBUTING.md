# Contributing

Standards: [AGENTS.md](AGENTS.md), [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

```bash
git clone <repository-url> && cd hyBBX
./scripts/dev-setup.sh && ./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

## Pull requests

- Small, focused diffs; Main/Secondary model unchanged unless discussed
- Behaviour / INI change → [docs/MANUAL.md](docs/MANUAL.md) + `share/*.ini.example`
- Entertain apps → plugin under `plugins/` only — [docs/ENTERTAIN.md](docs/ENTERTAIN.md)
- Verify: `cmake --build build`; telnet session if session/core touched

Issues: `.github/ISSUE_TEMPLATE/`.
