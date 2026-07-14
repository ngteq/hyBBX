# Development · HyBBX 2.4.0

Contributor build, test, and code layout reference.

## Workflow matrix

| Step | Command |
|------|---------|
| Debug build | `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build` |
| Tests | `-DHYBBX_BUILD_TESTS=ON && ctest --test-dir build` |
| Clients only | `-DHYBBX_CLIENTS_ONLY=ON` |
| Format / lint | project scripts in `scripts/` |

## Layout matrix

| Path | Content |
|------|---------|
| `src/` | Daemon core |
| `plugins/` | Transport plugins |
| `include/hybbx/` | Public headers |
| `share/` | INI templates, YAML registries |
| `tests/` | Unit tests (opt-in build) |

## v2.4.0 dev notes matrix

| Topic | Rule |
|-------|------|
| MAX25 boundary | No BayCom/PC-COM in `packet_radio` |
| Local TNC tests | Mock max25d `:7325` recommended |
| Live secrets | `./local/` only |

## Related

| Goal | Doc |
|------|-----|
| Build | [BUILD.md](BUILD.md) |
| Contributing | [../CONTRIBUTING.md](../CONTRIBUTING.md) |
