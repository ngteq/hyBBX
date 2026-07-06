# Linux

HyBBX documentation is **Linux-based**. C99 · CMake 3.16+ · GCC (primary) or Clang · pthread.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Alternate compiler:

```bash
CC=clang cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Dependencies

| Need | Role |
|------|------|
| CMake, GCC or Clang, make | Build |
| libssh (pkg-config) | SSH plugin (default ON) |
| OpenSSL / libsodium | Optional crypto (`HYBBX_CRYPTO_*`) |

## Serial `device=`

Typical USB TNC paths: `/dev/ttyUSB0`, `/dev/ttyACM0`.

## CI

Linux GCC — [`.github/workflows/ci.yml`](../.github/workflows/ci.yml).
