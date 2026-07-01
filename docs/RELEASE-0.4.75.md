# HyBBX 0.4.75

**Shipped.** Superseded by [RELEASE-0.5.0.md](RELEASE-0.5.0.md) (tag `v0.5.0`).

Release target: **full-functional test build** of the current feature set, plus **mail-area**, then **source release**.

## Feature freeze (lifted — 0.4.75 shipped)

**No new features** except those listed under **In scope** below.

Allowed without freeze exception:

- Bug fixes and regressions in shipped code
- Docs/INI comments aligned with existing behavior
- Release engineering (version strings, packaging, CI for the tag)

New capability requires an explicit **0.4.75 required** note in the PR/issue before implementation.

## In scope (0.4.75 only)

| Item | Status | Notes |
|------|--------|-------|
| Mail-area | Done | Persistent mailbox on centralized daemon (not chat) |
| Stabilize existing features | Required | Telnet, chat, auth, packet radio, circuit, clients |
| Source release | Required | Tag `v0.4.75`, build/install verified per [PLATFORMS.md](PLATFORMS.md) |
| Chat channel defaults | Done | `Channel1` … `Channel10`; INI `channel1` … `channel10` |

## Out of scope (defer until after 0.4.75)

- SSH / WebSocket transports
- SQL storage backend
- Link/repeater edge daemon roles (multi-site relay modes)
- HBX APRS / NETROM
- BayCom `ser12` native path
- Any other [ROADMAP.md](ROADMAP.md) item not marked required above

## Version

- `HYBBX_VERSION_STRING` = `0.4.75`
- CMake `PROJECT_VERSION` = `0.4.75`

## Release checklist

- [x] Mail-area implemented and documented in [FEATURES.md](FEATURES.md)
- [x] `share/hybbx.ini.example` + [MANUAL.md](MANUAL.md) updated for mail
- [ ] Full build: daemon, plugins, clients (`HYBBX_CLIENTS_ONLY`)
- [ ] Smoke: telnet session, chat, mail, packet_radio/circuit if enabled
- [x] Git tag `v0.4.75` on maintainer host (superseded by `v0.5.0`)
