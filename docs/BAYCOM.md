# BayCom plugin · HyBBX 2.4.0

Linux BayCom PR-Stack L2 path — HDLC + AX.25 UI on kernel modems, bridged to Main over HBX.

## v2.4.0 status matrix

| Item | Value |
|------|-------|
| CMake | `HYBBX_PLUGIN_BAYCOM=ON` (built) |
| Runtime default | `[networks] baycom=no` |
| `packet_radio` + `tnc=baycom` | **rejected** — use this plugin via MAX25 |
| RF prep owner | MAX25 `hardware/modems` |

## Backend matrix

| `backend` | Hardware | Notes |
|-----------|----------|-------|
| `kernel` (default) | SER12, PAR96, EPP | Thomas Sailer `baycom_*` + hdlcdrv |
| `kiss` | BayCom firmware serial | 8N1 KISS @ `serial_baud` |

## Kernel mode matrix

| `mode` | Driver | Netdev |
|--------|--------|--------|
| `ser12` / `ser12*` | `baycom_ser_fdx` | `bcsf0` |
| `ser12hdx` | `baycom_ser_hdx` | `bcsh0` |
| `par96` / `par96*` | `baycom_par` | `bcp0` |
| `epp` / `par97` | `baycom_epp` | `bce0` |

## Start order matrix

| # | Action |
|---|--------|
| 1 | MAX25 BayCom prep — `baycom-pr-ctl setup`, `max25-ctl start --hardware modems` |
| 2 | Merge `share/hybbx/baycom-ser12-host.ini.example` into `./local/hybbx.ini` |
| 3 | Set `[networks] baycom=yes` |
| 4 | Start HyBBX Secondary → HBX link to Main |

## Related

| Goal | Doc |
|------|-----|
| TNC (serial) | [TNCS.md](TNCS.md) |
| Manual | [MANUAL.md](MANUAL.md) |
| MAX25 BayCom | MAX25-Stack `docs/BAYCOM.md` |
