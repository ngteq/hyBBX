# Platforms

C99 · CMake 3.16+ · **GCC** (primary) or **Clang**.

| OS | Status |
|----|--------|
| Linux | Full — CI target |
| BSD | Full |
| macOS 10+ | Full |
| Windows 10+ MinGW | Full |
| AmigaOS 3.9+ cross | Target |

```bash
CC=clang cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Serial `device=`

| OS | Example |
|----|---------|
| Linux/BSD | `/dev/ttyUSB0` |
| macOS | `/dev/tty.usbserial-*` |
| Windows | `COM3` |
| AmigaOS | `serial.device` |

## Components

| Component | All POSIX targets |
|-----------|-------------------|
| Core + telnet | yes |
| HBX circuit | yes |
| Packet radio | yes* |
| OpenSSL/libsodium | optional CMake |

\* Correct device path per OS.

CI: Ubuntu GCC — [`.github/workflows/ci.yml`](../.github/workflows/ci.yml).
