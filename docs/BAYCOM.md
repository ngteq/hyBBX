# BayCom plugin · HyBBX

Optional **transport plugin** (`plugins/baycom/`).

| Item | Value |
|------|-------|
| Build | `HYBBX_PLUGIN_BAYCOM=ON` (default in CMake) |
| Example INI | `[networks] baycom=no` in `share/hybbx-*.ini.example` |
| Enable | Set `[networks] baycom=yes` and configure `[transport.baycom]` per site |

See `share/hybbx/baycom-ser12-host.ini.example` when enabling.

## Related

| Goal | Doc |
|------|-----|
| TNC serial (`packet_radio`) | [TNCS.md](TNCS.md) |
| Manual | [MANUAL.md](MANUAL.md) |
