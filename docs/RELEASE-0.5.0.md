# HyBBX 0.5.0

Release line: **stable 0.5.x** — maintain on this branch until a future major/minor bump is declared.

Shipped from **0.4.75** / **0.4.75-up1** plus stabilization work.

## Feature freeze (active on 0.5.x)

**No new features** except those listed under **In scope** below.

Allowed without freeze exception:

- Bug fixes and regressions in shipped code
- Docs/INI comments aligned with existing behavior
- Release engineering (version strings, packaging, CI)

New capability requires an explicit **0.5.x required** note in the PR/issue before implementation.

## Shipped in v0.5.0

| Item | Notes |
|------|-------|
| Mail-area | Flat-file inbox; `/mail` commands |
| Area navigation | `/main` (`/menu`), `/leave` (`/back`), area stack |
| User storage | Default `$HOME/.hybbx`; `~` expansion in `[storage] path` |
| `[networks]` | Telnet/SSH static; `ax25`, `websocket`, `circuit` toggles |
| Startup | Config discovery, `hybbx-start` creates `~/.hybbx`, Linux service run fix |
| Core platform | Telnet, chat, auth, packet radio, HBX circuit, clients |

## Out of scope (defer until after 0.5.x)

- SSH / WebSocket transports
- SQL storage backend
- Link/repeater edge daemon roles (multi-site relay modes)
- HBX APRS / NETROM
- BayCom `ser12` native path
- Other [ROADMAP.md](ROADMAP.md) items not marked required above

## Version

- `HYBBX_VERSION_STRING` = `0.5.0`
- CMake `PROJECT_VERSION` = `0.5.0`

## Release checklist

- [x] Mail-area implemented and documented in [FEATURES.md](FEATURES.md)
- [x] `share/hybbx.ini.example` + [MANUAL.md](MANUAL.md) updated
- [x] Storage default `~/.hybbx` documented and tested
- [x] Area navigation (`/main`, `/leave`) documented
- [x] `[networks]` section documented
- [x] Smoke: telnet session, guest auto-login, MOTD
- [ ] Git tag `v0.5.0` on maintainer host
