# HyBBX build reference

CMake build options and toolchains. **Platforms:** [PLATFORMS.md](PLATFORMS.md) (Windows 10+, macOS X+, AmigaOS 3.9+, Linux/BSD — **GCC and LLVM/Clang**). Quick steps: [QUICKSTART.md](QUICKSTART.md).

## Compilers

| Compiler | Support |
|----------|---------|
| GCC | Primary |
| LLVM Clang | Full |

```bash
CC=gcc   cmake -B build -DCMAKE_BUILD_TYPE=Release
CC=clang cmake -B build-clang -DCMAKE_BUILD_TYPE=Release
```

## Basic build (Linux / BSD / macOS)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # with -DHYBBX_BUILD_TESTS=ON
```

Optional: `scripts/dev-setup.sh` (build + IDE `compile_commands.json` symlink).

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
| `HYBBX_BUILD_DAEMON` | ON | Build `hybbx` server daemon and `hybbx_core` |
| `HYBBX_BUILD_CLIENTS` | ON | Build standalone CLI clients |
| `HYBBX_CLIENTS_ONLY` | OFF | Shorthand: daemon OFF, plugins OFF, clients ON |
| `HYBBX_BUILD_CLIENT_TELNET` | ON | Build `hybbx-telnet` (requires `HYBBX_BUILD_CLIENTS`) |
| `HYBBX_BUILD_CLIENT_TERMINAL` | ON | Build `hybbx-terminal` (requires `HYBBX_BUILD_CLIENTS`) |
| `HYBBX_BUILD_PLUGINS` | ON | Build transport plugins |
| `HYBBX_PLUGIN_TELNET` | ON | Telnet plugin |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON | Packet radio plugin |
| `HYBBX_BUILD_TESTS` | OFF | Test targets |
| `HYBBX_HARDENING` | ON | Compiler security flags (stack protector, FORTIFY, RELRO/PIE) |
| `HYBBX_WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors |
| `HYBBX_CRYPTO_OPENSSL` | OFF | OpenSSL for password hash, AES-GCM, random |
| `HYBBX_CRYPTO_LIBSODIUM` | OFF | libsodium for ChaCha and X25519 |

With `HYBBX_HARDENING=ON`: probed warnings, stack protector, `_FORTIFY_SOURCE=2` (Release), RELRO/NOW + PIE on Linux. Bounded buffers: `include/hybbx/limits.h`, safe `hybbx_path_join`.

### Clients-only build

```bash
cmake -B build-clients -DHYBBX_CLIENTS_ONLY=ON
cmake --build build-clients
# bin: build-clients/src/clients/hybbx-telnet, hybbx-terminal
```

Or: `./scripts/build-clients.sh`. See [CLIENTS.md](CLIENTS.md).

## Windows (10+, MinGW / LLVM-MinGW)

Use MSYS2 MinGW-w64 or LLVM-MinGW with GCC or Clang.

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Telnet and packet radio serial (`COMx`) supported. Details: [PLATFORMS.md](PLATFORMS.md).

## macOS (10.x+)

GCC or Clang (Homebrew `gcc` / `llvm`, or the system toolchain). Same CMake flow as Linux. Adjust `device=` for USB TNC paths under `/dev/tty.*`.

## AmigaOS (3.9+, cross-GCC)

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
