# Platforms · HyBBX 2.4.0

POSIX+ first — Linux, *BSD, AmigaOS 3.9+, macOS, Windows.

## Platform matrix

| Platform | Daemon | Telnet client | packet_radio | SSH |
|----------|--------|---------------|--------------|-----|
| Linux | yes | yes | yes | yes (libssh) |
| FreeBSD / NetBSD / OpenBSD | yes | yes | yes | yes |
| AmigaOS 3.9+ | partial | yes | yes | no |
| macOS | yes | yes | yes | yes |
| Windows | yes | yes | limited | yes |

## AmigaOS matrix

| Item | Value |
|------|-------|
| Cross-build | `./scripts/build-amiga-telnet.sh` |
| Daemon plugins | telnet + packet_radio only |
| Full feature set | Linux / *BSD recommended |

## Related

| Goal | Doc |
|------|-----|
| Build | [BUILD.md](BUILD.md) |
| Manual | [MANUAL.md](MANUAL.md) |
