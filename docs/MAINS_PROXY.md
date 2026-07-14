# mains_proxy · HyBBX 2.4.0

Opt-in mesh linking Main or Secondary instances for `/proxymail` and `/proxychat`.

## Feature matrix

| Feature | Supported |
|---------|-----------|
| `/proxymail` cross-site | yes |
| `/proxychat` cross-site | yes |
| User account sync | no |
| Sysop actions over proxy | no |
| Raw Main-to-Main TCP bypass | no — HBX/Circuit only |

## Build matrix

| Item | Value |
|------|-------|
| CMake | `-DHYBBX_PLUGIN_MAINS_PROXY=ON` |
| INI | `[networks] mains_proxy=yes` |
| Template | `share/hybbx-mesh.ini.example` |
| Auth | Same as Secondary — `link_id`, `link_password` |

## Topology matrix

| Peer A | Peer B | Transport |
|--------|--------|-----------|
| Main | Main | HBX circuit client |
| Main | Secondary | HBX circuit client |
| Secondary | Secondary | via Main hub (typical) |

## Related

| Goal | Doc |
|------|-----|
| Topology | [TOPOLOGY.md](TOPOLOGY.md) |
| Manual | [MANUAL.md](MANUAL.md) |
