# Entertain Area · HyBBX 2.4.0

Optional Main-only plugins for games and apps — not on Secondary RF hosts.

## Plugin matrix

| Plugin | Status | Notes |
|--------|--------|-------|
| Entertain Area framework | opt-in CMake | Main hosts only |
| Chess / games | deferred | future or doc removal |

## Config matrix

| Item | Value |
|------|-------|
| Enable | `-DHYBBX_PLUGIN_ENTERTAIN=ON` (when available) |
| Host role | Main only — Secondary runs RF transports |
| User access | Level-gated via `[areas]` / `share/areas.yaml` |

## Related

| Goal | Doc |
|------|-----|
| Topology | [TOPOLOGY.md](TOPOLOGY.md) |
| Commands | [COMMANDS.md](COMMANDS.md) |
