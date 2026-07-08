# Platforms

HyBBX is developed on **Linux** (GCC primary, Clang). Operator documentation assumes Linux (`HTTPD_DOCROOT`, `systemctl`, `ss`).

C99 · CMake 3.16+ · pthread (POSIX targets).

## Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

| Need | Role |
|------|------|
| CMake, GCC or Clang, make | Build |
| libssh (pkg-config) | SSH plugin (default ON) |
| OpenSSL / libsodium | Optional crypto (`HYBBX_CRYPTO_*`) |

Serial TNC paths: `/dev/ttyUSB0`, `/dev/ttyACM0`, `/dev/ttyS*`.

CI: Linux GCC — [`.github/workflows/ci.yml`](../.github/workflows/ci.yml).

## AmigaOS 3.9+

Cross-compile with m68k-amigaos-gcc. Amiga builds enable **telnet** and **packet_radio** only; SSH, WebSocket, ARDOP, CRDOP, and baycom are disabled automatically.

```bash
cmake -B build-amiga \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake \
  -DAMIGA_SDK_PATH=/opt/amiga \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-amiga
```

Set `AMIGA_SDK_PATH` to your cross-compiler prefix. Serial devices use Amiga `serial.device` unit numbers (see `device=` in [MANUAL.md](MANUAL.md)).

Hardening: stack protector where supported; no Linux PIE/RELRO on Amiga.

## Other POSIX

BSD and macOS builds are supported in tree with GCC or Clang; verify plugins in your environment.

## Windows

MinGW/LLVM-MinGW builds use the Win32 serial and socket paths in plugins; SSH follows libssh availability.
