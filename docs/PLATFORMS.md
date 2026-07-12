# Platforms

HyBBX is developed on **Linux** (GCC primary, Clang). Operator documentation assumes Linux (`HTTPD_DOCROOT`, `systemctl`, `ss`).

**Text-first, portable:** the product is built around **plain-text sessions** (commands, mail, chat) so the same codebase runs on constrained links and diverse hosts. Shared logic uses C99 and POSIX APIs; platform code stays behind narrow guards. That flexibility lets you deploy text telnet on AmigaOS or *BSD today and add ANSI, menus, or WebSocket graphics through plugins without rewriting core.

**Portability:** HyBBX stays **POSIX+ friendly** — standard C99 and POSIX APIs first, platform code behind narrow guards (`_WIN32`, `__AMIGA__`, …). We keep `*BSD` and **AmigaOS 3.9+** working in-tree and aim to make every change **easy to port** (no Linux-only shortcuts in shared core).

C99 · CMake 3.16+ · pthread on POSIX-class targets.

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

Cross-compile `hybbx-telnet` with bebbo-gcc (`m68k-amigaos-gcc`). Uses clib2
(`-mcrt=clib2`) and links `libnet` (bsdsocket). Copy the binary to your Amiga;
TCP/IP stack (MiamiDX, Genesis, Roadshow, …) must be running.

```bash
./scripts/build-amiga-telnet.sh
# or:
cmake -B build-amiga \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake \
  -DAMIGA_SDK_PATH=/opt/amiga \
  -DHYBBX_CLIENTS_ONLY=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-amiga --target hybbx-telnet
```

Set `AMIGA_SDK_PATH` to your bebbo-gcc prefix (default `/opt/amiga` when
present). The cross-build uses headers and libs from that prefix
(`m68k-amigaos/clib2`, `m68k-amigaos/ndk-include`) — not a manual merge of
NDK trees.

Amiga builds enable **hybbx-telnet** and **packet_radio** only; SSH,
WebSocket, ARDOP, CRDOP, baycom, and `hybbx-terminal` are disabled automatically.

On Amiga: `hybbx-telnet -H host -p 2323` from Shell. IPv6 (`-6`) is not
supported on AmigaOS.

Hardening: stack protector where supported; no Linux PIE/RELRO on Amiga.

## *BSD

**FreeBSD**, **NetBSD**, **OpenBSD**, DragonFly BSD, and related BSD systems (`*BSD` notation). First-class alongside Linux — build with GCC or Clang; verify plugins in your environment.

Serial TNC paths follow the host (`/dev/cua*`, `/dev/tty*`).

## MacOS X+

**MacOS X** and newer (Intel and Apple Silicon). Build with Apple Clang or LLVM Clang; pthread and serial I/O use the POSIX paths in-tree.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

| Need | Role |
|------|------|
| Xcode CLT or Homebrew LLVM | Compiler |
| libssh (pkg-config) | SSH plugin |
| OpenSSL / libsodium | Optional crypto |

Serial TNC paths: `/dev/cu.*`, `/dev/tty.*`. Verify libssh and plugin deps on your MacOS version.

## Windows 10+

**Windows 10** and newer via **MinGW-w64** or **LLVM-MinGW** (GCC or Clang). Win32 serial (`COMn`) and socket code paths in plugins; SSH follows libssh availability.

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Map TNC `device=` to `COM1`, `COM2`, … (or `\\.\COM10` for higher ports). Operator docs (`systemctl`, paths) remain Linux-oriented; adapt commands for your environment.
