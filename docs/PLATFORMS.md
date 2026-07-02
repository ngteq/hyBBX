# HyBBX supported platforms

HyBBX is **plain C99** and builds with **GCC** or **LLVM/Clang** only. Use MinGW-w64 or GCC or Clang on Windows.

CMake 3.16+ is required on all hosts.

## Toolchain policy


| Compiler       | Role                                                                 |
| -------------- | -------------------------------------------------------------------- |
| **GCC**        | Primary toolchain (Linux, BSD, MinGW-w64, AmigaOS cross-GCC, …)      |
| **LLVM Clang** | Fully supported (Linux, BSD, macOS, Windows LLVM-MinGW, …) |


Set explicitly when needed:

```bash
CC=gcc   cmake -B build ...
CC=clang cmake -B build ...
```



## Operating systems


| OS          | Version                             | Host build  | Typical compiler                | Status                                                        |
| ----------- | ----------------------------------- | ----------- | ------------------------------- | ------------------------------------------------------------- |
| **Linux**   | mainstream distros                  | yes         | GCC, Clang                      | **Full** — primary CI target                                  |
| **BSD**     | FreeBSD, OpenBSD, NetBSD, DragonFly | yes         | GCC, Clang                      | **Full** — POSIX paths                                        |
| **macOS**   | **10.x+** (macOS X and later)       | yes         | GCC, Clang                      | **Full** telnet/core; packet radio needs USB/serial device    |
| **Windows** | **10+**                             | yes         | MinGW-w64 GCC, LLVM-MinGW Clang | **Full** telnet + packet radio (`COMx` serial)                |
| **AmigaOS** | **3.9+**                            | cross-build | m68k-amigaos-**gcc**            | **Full** telnet + packet radio (`serial.device`) cross-target |
| Other POSIX | Solaris, etc.                       | often       | GCC, Clang                      | **Best effort** — same POSIX code paths as Linux/BSD          |


**Full** = core session, telnet, circuit hub, flat-file storage, crypto bundles build and run.  
**Partial** = known gaps (documented below); contributions accepted.

## Build by platform



### Linux / BSD (native)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/hybbx.sh
```



### macOS (10.x+)

Install **GCC** or **Clang** (Homebrew `gcc` / `llvm`, or the system toolchain).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/src/hybbx -c local/hybbx.ini
```

Default USB TNC device in examples may be `/dev/tty.usb*` — adjust `device=` in INI.

### Windows (10+)

Use **MSYS2** / **MinGW-w64** or **LLVM-MinGW** with GCC or Clang.

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\src\hybbx.exe -c local\hybbx.ini
```

Telnet and packet radio serial are supported. Set `device = COM3` (or `COM3` / `\\.\COM10`) in `[packet_radio]` — MSYS `/dev/ttyS0` maps to `COM1`.

### AmigaOS (3.9+, cross-build)

Requires an AmigaOS 3.x **GCC** cross-compiler and SDK.

```bash
cmake -B build-amiga \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake \
  -DAMIGA_SDK_PATH=/path/to/amiga-sdk
cmake --build build-amiga
```

Toolchain: [cmake/Toolchain-AmigaOS.cmake](../cmake/Toolchain-AmigaOS.cmake). Packet radio uses Amiga `serial.device` (unit `0` default). INI examples: `device = serial.device`, `device = serial.device:1`, or `device = 0` for built-in serial unit 0.

## Serial device paths


| Platform  | `device=` examples                      | Notes                                                       |
| --------- | --------------------------------------- | ----------------------------------------------------------- |
| Linux/BSD | `/dev/ttyUSB0`, `/dev/ttyS0`            | POSIX `termios`, 8N1 raw                                    |
| macOS     | `/dev/tty.usbserial-*`, `/dev/cu.*`     | Same POSIX backend                                          |
| Windows   | `COM3`, `com1`, `/dev/ttyS0` → COM1     | Win32 `CreateFile` + `DCB`; `\\.\COM10` for COM10+          |
| AmigaOS   | `serial.device`, `serial.device:1`, `0` | `serial.device` via Exec `OpenDevice`, 8N1, no HW handshake |




## Feature availability by platform


| Component                      | Linux/BSD | macOS | Windows | AmigaOS cross  |
| ------------------------------ | --------- | ----- | ------- | -------------- |
| Core / session / commands      | yes       | yes   | yes     | target         |
| Telnet transport               | yes       | yes   | yes     | target         |
| Internal HBX circuit           | yes       | yes   | yes     | target         |
| Flat-file storage              | yes       | yes   | yes     | target         |
| Bundled crypto                 | yes       | yes   | yes     | target         |
| Packet radio (TNC serial)      | yes       | yes*  | yes     | yes**          |
| OpenSSL / libsodium (optional) | yes       | yes   | yes     | depends on SDK |


macOS: use correct `/dev/tty.*` or `/dev/cu.*` device path.  
AmigaOS: cross-built on a POSIX host; run on real hardware or emulator with a TNC on the Amiga serial port.

## pthread and sockets

- Core and plugins use **POSIX threads** (`pthread`). On Windows this comes from **winpthreads** (MinGW) via CMake `Threads::Threads`.
- Telnet and circuit code include `_WIN32` branches for sockets and timing where needed.



## CI

GitHub Actions CI (`.github/workflows/ci.yml`) currently builds and tests on **Ubuntu** with GCC. macOS/Windows/Amiga cross-builds are supported by the project policy but validated on maintainer hosts until CI matrix rows are added.

## Reporting platform issues

Open a GitHub issue with: OS version, `gcc --version` or `clang --version`, CMake generator, and the failing target (core, telnet, packet_radio). See [CONTRIBUTING.md](../CONTRIBUTING.md).