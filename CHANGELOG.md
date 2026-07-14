# Changelog · HyBBX 2.4.0

All notable changes to HyBBX are documented here.

## [HyBBX 2.4.0]

Version `2.4.0`. Product label **HyBBX 2.4.0**.

### RF / MAX25 integration matrix

| Change | Detail |
|--------|--------|
| MAX25-minimal boundary | MAX25 owns hardware prep; HyBBX probes max25d and attaches KISS only |
| `[max25] check=yes` | Local `packet_radio` **requires** reachable max25d TCP `:7325` |
| `kiss_entry=none` | Default after MAX25 prep — HyBBX does not send `kiss on` |
| `tnc=baycom\|pccom` | **Rejected** in `packet_radio` — use MAX25 for BayCom/PC-COM |
| `[networks] baycom=no` | Runtime default off; plugin built (`HYBBX_PLUGIN_BAYCOM=ON`) |

### Operator INI matrix

| Key | v2.4.0 value |
|-----|--------------|
| `[max25] check` | `yes` |
| `kiss_entry` | `none` |
| `[networks] baycom` | `no` |

### Carried from v2.0.0-p2

| Item | Notes |
|------|-------|
| YAML menu/rights registry, `hybbxd`, screen/tmux | baseline |
| TheFirmware TNC terminal recovery before KISS | baseline |
| AX.25 broadcast timing / dual-TNC sequencing | baseline |
| Circuit reconnect race fix | baseline |
| Unified log levels | baseline |

## Related

| Goal | Doc |
|------|-----|
| Master guide | [docs/MASTER-GUIDE.md](docs/MASTER-GUIDE.md) |
| TNC profiles | [docs/TNCS.md](docs/TNCS.md) |
