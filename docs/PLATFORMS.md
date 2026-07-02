# HyBBX supported platforms

C99, **GCC** or **LLVM/Clang**. CMake 3.16+.

| Compiler | Role |
|----------|------|
| GCC | Primary (Linux, BSD, MinGW, Amiga cross) |
| Clang | Full support |

```bash
CC=gcc   cmake -B build ...
CC=clang cmake -B build ...
```

## Operating systems

| OS | Version | Build | Status |
|----|---------|-------|--------|
| Linux | distros | native | **Full** — CI target |
| BSD | FreeBSD, OpenBSD, … | native | **Full** |
| macOS | 10+ | native | **Full** (adjust TNC `device=`) |
| Windows | 10+ | MinGW/LLVM-MinGW | **Full** (`COMx` serial) |
| AmigaOS | 3.9+ | cross-GCC | **Full** (`serial.device`) |

**Full** = core, telnet, circuit, flat-file storage, bundled crypto.

### Linux / BSD / macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

### Windows

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

`device = COM3` in `[transport.packet_radio]`.

### AmigaOS cross

```bash
cmake -B build-amiga -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake \
  -DAMIGA_SDK_PATH=/path/to/amiga-sdk
cmake --build build-amiga
```

## Serial `device=` paths

| Platform | Examples |
|----------|----------|
| Linux/BSD | `/dev/ttyUSB0`, `/dev/ttyS0` |
| macOS | `/dev/tty.usbserial-*`, `/dev/cu.*` |
| Windows | `COM3`, `\\.\COM10` |
| AmigaOS | `serial.device`, `serial.device:1`, `0` |

## Component matrix

| Component | Linux/BSD | macOS | Windows | Amiga cross |
|-----------|-----------|-------|---------|-------------|
| Core / telnet / HBX | yes | yes | yes | target |
| Packet radio | yes | yes* | yes | yes** |
| OpenSSL / libsodium | optional | optional | optional | SDK-dependent |

\* Correct `/dev/tty.*` path. \** Cross-build on POSIX host.

## pthread / sockets

POSIX threads via CMake `Threads::Threads` (winpthreads on MinGW). Socket branches for `_WIN32` in telnet/circuit code.

## CI

GitHub Actions: Ubuntu + GCC ([`.github/workflows/ci.yml`](../.github/workflows/ci.yml)). Other platforms: maintainer hosts until matrix expands.

Issues: OS version, compiler version, CMake generator, failing target — [CONTRIBUTING.md](../CONTRIBUTING.md).
