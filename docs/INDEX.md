# HyBBX documentation

Central index for all project documentation. The root [README.md](../README.md) is a short presentation only; details live here.

HyBBX is a **connection-type service platform** inspired by classic BBS/mailbox culture — not a new BBS or mailbox product. See [README.md](../README.md) for the positioning statement.

| Document | Audience | Contents |
|----------|----------|----------|
| **[FEATURES.md](FEATURES.md)** | Everyone | **Start here** for what HyBBX does — maintained feature list with status (Done / Planned) |
| **[QUICKSTART.md](QUICKSTART.md)** | Operators | Clone, build, run, install, first telnet session |
| **[MANUAL.md](MANUAL.md)** | Operators & developers | Full reference: config, transports, packet radio, auth, commands, crypto, build |
| **[REPOSITORY.md](REPOSITORY.md)** | Developers | Source tree layout and main directories |
| **[BUILD.md](BUILD.md)** | Developers | CMake options, toolchains, hardening |

Also useful:

- Example config: [share/hybbx.ini.example](../share/hybbx.ini.example)
- Dev config: [local/hybbx.ini](../local/hybbx.ini)
- License: [LICENSE.txt](../LICENSE.txt)

**Maintainers:** update [FEATURES.md](FEATURES.md) when adding or changing functionality.

**Git / public pushes:** commit and push as **`ngteq`** only (`user.name=ngteq`; keep `user.email` empty). No `Co-authored-by`, Cursor, or agent attribution in commit messages.
