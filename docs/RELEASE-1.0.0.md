# HyBBX v1.0.0

First official GitHub release. **Telnet-session release** — the telnet user path on Main is **tested and verified** for normal operation (guest login, registered login, mail, chat, commands, Sysop tools).

## Verified in v1.0.0

| Area | Status |
|------|--------|
| Telnet transport (`:2323`) | **Verified** — production-style session testing passed |
| Session core | **Verified** via telnet (areas, mail, chat, `/` commands) |
| Flat-file storage + auth | **Verified** via telnet |
| Default traffic profile (2400 baud pacing, 80 cols) | **Verified** via telnet |
| CMake build + unit tests (`ctest`) | **Verified** in CI |

## Present but not verified in live operation

| Area | Status |
|------|--------|
| AX.25 / packet radio plugin | **Local tests only** — no confirmed active RF deployment |
| HBX circuit hub + Secondary bridge | Code complete; **not verified** end-to-end on air |
| ARDOP / CRDOP plugins | Built; host-TCP local scripts only; **no live modem RF** |
| TCP `/broadcast` | Stub (log only) |
| SQL storage | Not implemented |

Further RF and multi-link field validation is planned after v1.0.0 — [ROADMAP.md](ROADMAP.md).

## Install layout

`cmake --install build --prefix <path>` → `<path>/hybbx/`:

`hybbx`, `hybbx-start`, `hybbx.ini`, `text/`, `data/`, `logs/`, `lib/`

## Upgrade from 0.9.x

- Bump version strings only; INI keys unchanged
- Re-read [MANUAL.md](MANUAL.md) — comments in `share/hybbx.ini.example` are leaner; detail moved here

## License

GPL-3.0 — [LICENSE.txt](../LICENSE.txt). External modems: [LICENSING.md](LICENSING.md).
