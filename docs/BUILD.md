# Build · HyBBX 2.4.0

POSIX+ build reference. Platforms: [PLATFORMS.md](PLATFORMS.md).

## Build matrix

| Step | Command |
|------|---------|
| Release build | `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build` |
| Run | `./scripts/hybbx.sh` |
| Test | `-DHYBBX_BUILD_TESTS=ON` → `ctest --test-dir build` |
| Telnet smoke | `telnet 127.0.0.1 2323` |
| SSH smoke | `ssh 127.0.0.1 -p 3232` |

## CMake options matrix

| Option | Default | Description |
|--------|---------|-------------|
| `HYBBX_BUILD_DAEMON` | ON | `hybbxd` + core |
| `HYBBX_BUILD_CLIENTS` | ON | CLI clients |
| `HYBBX_PLUGIN_PACKET_RADIO` | ON | Packet radio |
| `HYBBX_PLUGIN_BAYCOM` | ON | BayCom plugin (v2.4.0 built; runtime off) |
| `HYBBX_PLUGIN_MAINS_PROXY` | OFF | Main-to-Main mesh proxy |
| `HYBBX_PLUGIN_ARDOP` | ON | ARDOP plugin |
| `HYBBX_PLUGIN_CRDOP` | ON | CRDOP plugin |
| `HYBBX_PLUGIN_SSH` | ON | SSH (libssh) |
| `HYBBX_PLUGIN_WEBSOCKET` | ON | WebSocket proxy |
| `HYBBX_BUILD_TESTS` | OFF | Unit tests |
| `HYBBX_STORAGE_SQLITE` | ON | SQLite backend |

## Config matrix

| Item | Value |
|------|-------|
| Live INI | `./local/hybbx.ini` (gitignored) |
| Templates | `share/hybbx-*.ini.example` |
| Secrets | `./local/` only — never commit |

## Related

| Goal | Doc |
|------|-----|
| Platforms | [PLATFORMS.md](PLATFORMS.md) |
| Developer | [DEVELOPMENT.md](DEVELOPMENT.md) |
