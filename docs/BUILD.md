# HyBBX build reference

CMake build options and toolchains. Quick compile steps: [QUICKSTART.md](QUICKSTART.md). Operator install notes: [MANUAL.md — Building & installing](MANUAL.md#building--installing).

## Basic build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Clang:

```bash
CC=clang cmake -B build-clang -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang
```

Optional crypto backends:

```bash
cmake -B build -DHYBBX_CRYPTO_OPENSSL=ON
cmake -B build -DHYBBX_CRYPTO_LIBSODIUM=ON
```

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `HYBBX_BUILD_PLUGINS` | ON | Build transport plugins |
| `HYBBX_PLUGIN_TELNET` | ON | Telnet plugin |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON | Packet radio plugin |
| `HYBBX_BUILD_TESTS` | OFF | Test targets |
| `HYBBX_HARDENING` | ON | Compiler security flags (stack protector, FORTIFY, RELRO/PIE) |
| `HYBBX_WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors |
| `HYBBX_CRYPTO_OPENSSL` | OFF | OpenSSL for password hash, AES-GCM, random |
| `HYBBX_CRYPTO_LIBSODIUM` | OFF | libsodium for ChaCha and X25519 |

With `HYBBX_HARDENING=ON`: probed warnings, stack protector, `_FORTIFY_SOURCE=2` (Release), RELRO/NOW + PIE on Linux. Bounded buffers: `include/hybbx/limits.h`, safe `hybbx_path_join`.

## AmigaOS cross-build

```bash
cmake -B build-amiga \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake \
  -DAMIGA_SDK_PATH=/path/to/amiga-sdk
cmake --build build-amiga
```

Toolchain file: `cmake/Toolchain-AmigaOS.cmake`.

## Install targets

```bash
cmake --install build --prefix /path/to/prefix
```

Produces `bin/hybbx`, `bin/hybbx-start`, `etc/hybbx.ini`, `text/`, `data/`, `lib/` under the prefix.
