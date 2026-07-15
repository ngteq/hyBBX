# TNC profiles · HyBBX 2.4.0

Single `packet_radio` transport — up to 8 instances per process. MAX25 owns RF prep; HyBBX attach-only in v2.4.0.

## packet_radio / MAX25 matrix

| Rule | Value |
|------|-------|
| `kiss_entry` | `none` (default) — MAX25 sends `kiss on` |
| `[max25] check` | `yes` — probe max25d `:7325` before serial open |
| Serial ownership | One process per `/dev/tty*` |

## Host protocol matrix

| Protocol | INI `protocol=` | Use |
|----------|-----------------|-----|
| KISS | `kiss` (default) | Secondary RF — recommended after MAX25 prep |
| TNC2 host converse | `hostmode`, `host`, `tnc2` | Interactive host session |
| 6PACK | `sixpack` | DF6BU 6PACK over serial |

## KISS entry/exit matrix

| INI key | Values | v2.4.0 default |
|---------|--------|----------------|
| `kiss_entry` | `none`, `kiss_on`, `esc_at_k`, `auto` | `none` |
| `kiss_exit` | `none`, `kiss_off`, `kiss_frame`, `auto` | `none` |

## TNC profile matrix (packet_radio)

| Profile | Aliases | Serial | KISS attach | RTS/DTR |
|---------|---------|--------|-------------|---------|
| `tnc2c` | `tnc2-c` | 7E1 | `none` | boot-wait |
| `tnc2` | `pktnc2`, `pk-tnc2`, `tapr`, `thefirmware` | 8N1 | `none` | boot-wait |
| `pk232` | `pk-232`, `aea` | 8N1 | `none` | off |
| `mfj1278` | `mfj-1278` | 7E1 | `none` | off |
| `kantronics` | `kpc`, `kpc3` | 8N1 | `none` | off |
| `generic` | `tnc` | 8N1 | `none` | off |

BayCom hardware: optional [`baycom`](BAYCOM.md) transport plugin — not a `packet_radio` TNC profile.

## AX.25 source matrix

| Item | Owner |
|------|-------|
| Host `MYCALL` on TNC | MAX25 (prep) |
| Outbound UI source | HyBBX `[broadcast] ax25_mycall` |
| KISS DATA FCS | Stripped by HyBBX — TNC adds CRC on air |
| `persist` on CB | `255` recommended — CSMA defer avoidance |

## Related

| Goal | Doc |
|------|-----|
| Manual | [MANUAL.md](MANUAL.md) |
| BayCom plugin | [BAYCOM.md](BAYCOM.md) |
| Dual-TNC | [DUAL-TNC-OPERATOR-GUIDE.md](DUAL-TNC-OPERATOR-GUIDE.md) |
