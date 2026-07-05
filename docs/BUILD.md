# Build

[PLATFORMS.md](PLATFORMS.md) · [QUICKSTART.md](QUICKSTART.md) · **v1.0.0**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Tests: `-DHYBBX_BUILD_TESTS=ON` → `ctest --test-dir build`

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `HYBBX_BUILD_DAEMON` | ON | `hybbx` + core |
| `HYBBX_BUILD_CLIENTS` | ON | CLI clients |
| `HYBBX_CLIENTS_ONLY` | OFF | Clients without daemon |
| `HYBBX_BUILD_PLUGINS` | ON | Transport plugins |
| `HYBBX_PLUGIN_TELNET` | ON | Telnet |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON | Packet radio |
| `HYBBX_PLUGIN_ARDOP` | ON | ARDOP plugin |
| `HYBBX_PLUGIN_CRDOP` | ON | CRDOP plugin |
| `HYBBX_PLUGIN_SSH` | ON | SSH (requires libssh) |
| `HYBBX_BUILD_TESTS` | OFF | Unit tests |
| `HYBBX_HARDENING` | ON | Stack protector, RELRO/PIE |
| `HYBBX_CRYPTO_OPENSSL` | OFF | Optional OpenSSL |
| `HYBBX_CRYPTO_LIBSODIUM` | OFF | Optional libsodium |

Clients-only: `-DHYBBX_CLIENTS_ONLY=ON` or `./scripts/build-clients.sh`.

## Install

```bash
cmake --install build --prefix /path
```

→ `<prefix>/hybbx/` (`hybbx`, `hybbx-start`, `hybbx.ini`, `text/`, `data/`, `logs/`, `lib/`).

CRDOPC is **not** built by HyBBX — [CRDOP.md](CRDOP.md).

## Toolchains

| Target | Notes |
|--------|-------|
| Linux/BSD/macOS | Native GCC/Clang |
| Windows | MinGW Makefiles |
| AmigaOS | `cmake/Toolchain-AmigaOS.cmake` |
