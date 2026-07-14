# ARDOP plugin · HyBBX 2.4.0

HyBBX transport plugin for ARDOP modem paths — external modem ownership.

## Status matrix

| Item | Value |
|------|-------|
| CMake | `HYBBX_PLUGIN_ARDOP=ON` (default) |
| INI | `[networks] ardop=yes` when modem available |
| RF prep | External ARDOP stack — not MAX25 CRDOP |
| Host role | Secondary typical |

## Config matrix

| Item | Value |
|------|-------|
| Transport section | `[transport.ardopN]` per instance |
| Circuit link | `circuit_host`, `link_id`, `link_password` |
| Protocol | ARDOP session bridge to HBX |

## Related

| Goal | Doc |
|------|-----|
| Topology | [TOPOLOGY.md](TOPOLOGY.md) |
| CRDOP (soft modem) | [CRDOP.md](CRDOP.md) |
