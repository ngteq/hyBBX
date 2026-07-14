# AX25SRV operator guide · HyBBX 2.4.0

Standalone Main with one or two local TNCs — MAX25 prep first, HyBBX second, RF verify last.

## Scope matrix

| In scope | Out of scope |
|----------|--------------|
| Standalone Main + local `packet_radio` | Remote Secondary split |
| TNC2C / PK-TNC2 (TheFirmware) | BayCom-only sites |
| MAX25 → HyBBX → `/broadcast ax25` | mains_proxy mesh |

## Responsibility matrix

| Phase | MAX25 | HyBBX |
|-------|-------|-------|
| TNC cold boot (DTR+RTS) | boot-wait / max25d | — |
| MYCALL, TXDELAY, `kiss on` | max25d | — |
| max25d TCP probe | listens `:7325` | `[max25] check=yes` |
| KISS DATA / PTT | — | `packet_radio` |
| User sessions, `/broadcast ax25` | — | Main |

## Pre-flight matrix

| # | Check | Pass |
|---|-------|------|
| 1 | MAX25 scripts installed | boot-wait executable |
| 2 | HyBBX `packet_radio` built | plugin loads |
| 3 | INI in `./local/hybbx.ini` | merged from `share/` template |
| 4 | `[broadcast] ax25_mycall` | valid callsign |
| 5 | `kiss_entry=none` | on each `[transport.packet_radioN]` |
| 6 | Unique `link_id` per transport | matches bridge registry |

## Start sequence matrix

| # | Action |
|---|--------|
| 1 | MAX25 prep per TNC — verify `ss -ltn | grep 7325` |
| 2 | `hybbxd -c ./local/hybbx.ini` |
| 3 | Log: max25d reachable → KISS attach |
| 4 | `/broadcast ax25` — verify PTT and on-air decode |

## Related

| Goal | Doc |
|------|-----|
| Master guide | [MASTER-GUIDE.md](MASTER-GUIDE.md) |
| TNC profiles | [TNCS.md](TNCS.md) |
| Topology | [TOPOLOGY.md](TOPOLOGY.md) |
