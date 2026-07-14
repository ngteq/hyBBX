# CRDOP plugin · HyBBX 2.4.0

HyBBX transport for MAX25-SoftModem (CRDOP) — acoustic AX.25 over soundcard.

## Integration matrix

| Layer | Owner |
|-------|-------|
| Soft-modem prep, TCP 8515/8516 | MAX25 `soft-crdop` |
| HyBBX transport | `crdop` plugin |
| INI merge | `share/hybbx/crdop-host.ini.example` |

## Config matrix

| Item | Value |
|------|-------|
| Enable | `[networks] crdop=yes` |
| CMake | `HYBBX_PLUGIN_CRDOP=ON` (default) |
| `modem_host` | MAX25 host (loopback or LAN) |
| `modem_port` | `8515` (control) |

## Start order matrix

| # | Action |
|---|--------|
| 1 | `max25-ctl start --hardware soft-modems --device soft-crdop` |
| 2 | Merge CRDOP host INI fragment |
| 3 | Start HyBBX with `[networks] crdop=yes` |

## Related

| Goal | Doc |
|------|-----|
| MAX25 CRDOP | MAX25-Stack `docs/CRDOP.md` |
| Manual | [MANUAL.md](MANUAL.md) |
