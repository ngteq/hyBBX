# HyBBX build reference

[PLATFORMS.md](PLATFORMS.md) · [QUICKSTART.md](QUICKSTART.md)

## Basic build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Clang: `CC=clang cmake -B build ...`

Optional crypto: `-DHYBBX_CRYPTO_OPENSSL=ON`, `-DHYBBX_CRYPTO_LIBSODIUM=ON`

Tests: `-DHYBBX_BUILD_TESTS=ON` then `ctest --test-dir build`

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `HYBBX_BUILD_DAEMON` | ON | Server `hybbx` + `hybbx_core` |
| `HYBBX_BUILD_CLIENTS` | ON | CLI clients |
| `HYBBX_CLIENTS_ONLY` | OFF | Clients only (no daemon/plugins) |
| `HYBBX_BUILD_PLUGINS` | ON | Transport plugins |
| `HYBBX_PLUGIN_TELNET` | ON | Telnet plugin |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON | Packet radio plugin |
| `HYBBX_PLUGIN_ARDOP` | ON | ARDOP host-client plugin (external ARDOPC/ardopcf) |
| `HYBBX_PLUGIN_CRDOP` | ON | CRDOP CB host-client plugin (external ARDOPC/ardopcf) |
| `HYBBX_BUILD_TESTS` | OFF | Unit tests |
| `HYBBX_HARDENING` | ON | Stack protector, FORTIFY, RELRO/PIE |
| `HYBBX_WARNINGS_AS_ERRORS` | OFF | `-Werror` |
| `HYBBX_CRYPTO_OPENSSL` | OFF | OpenSSL backends |
| `HYBBX_CRYPTO_LIBSODIUM` | OFF | libsodium backends |

`HYBBX_HARDENING=ON`: stack protector, `_FORTIFY_SOURCE=2` (Release), RELRO/PIE on Linux.

### Clients-only

```bash
cmake -B build-clients -DHYBBX_CLIENTS_ONLY=ON && cmake --build build-clients
```

Or `./scripts/build-clients.sh` — [CLIENTS.md](CLIENTS.md).

## Platform notes

| Platform | Command |
|----------|---------|
| Windows MinGW | `cmake -B build -G "MinGW Makefiles"` |
| AmigaOS cross | `-DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake` |

## Install

```bash
cmake --install build --prefix /path
```

→ `<prefix>/hybbx/` (`hybbx`, `hybbx-start`, `hybbx.ini`, `text/`, `data/`, `logs/`, `lib/`).
