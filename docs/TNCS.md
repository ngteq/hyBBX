# Supported TNCs — packet_radio plugin

**v2.0.0** · INI: [MANUAL.md](MANUAL.md) · one plugin: `packet_radio`

HyBBX uses a single configurable `packet_radio` transport. Each `[transport.packet_radioN]` section on a **Secondary** opens one serial (or USB) TNC and one HBX link to Main. Up to **8** instances per process (`HYBBX_PACKET_RADIO_MAX_INSTANCES`).

On **Main**, sections without `device` / `tnc` / `protocol` are bridge-registry rows only (`link_id`, password, frequency). Add those keys on Main for a **local TNC** (standalone).

## Host protocols

| Protocol | INI `protocol=` | Use |
|----------|-----------------|-----|
| KISS | `kiss` (default) | Secondary RF edge, APRS-style AX.25 frames |
| TNC2 host converse | `hostmode`, `host`, `tnc2` | Interactive host session (not WA8DED JHOST binary) |
| 6PACK | `sixpack` | DF6BU 6PACK over serial |

Recommended for HyBBX Secondary: **KISS**.

## KISS entry / exit

| INI key | Values | Notes |
|---------|--------|-------|
| `kiss_entry` | `kiss_on`, `esc_at_k`, `auto`, `none` | How HyBBX enters KISS at start |
| `kiss_exit` | `kiss_off`, `kiss_frame`, `auto`, `none` | How HyBBX leaves KISS on shutdown |
- **`kiss_on`** — send `kiss on` (TheFirmware, TNC2C, PK-232, Kantronics).
- **`esc_at_k`** — send ESC + `@K` (older TF / Landolt TF2.7 style).
- **`auto`** — try `kiss on`, then ESC+`@K` (default for `tnc2`, `generic`, `mfj1278`).
- **`kiss_frame`** exit — send `0xC0 0xFF 0xC0` (KISS Return); used when entry was ESC+`@K` and `kiss_exit=auto`.

## TNC profiles (`tnc=`)

| Profile | Aliases | Host serial | KISS entry | RTS/DTR | Typical host baud |
|---------|---------|-------------|------------|---------|-------------------|
| `tnc2c` | `tnc2-c` | 7E1 | `kiss_on` | on | 2400 (TERM jumper) |
| `tnc2` | `pktnc2`, `pk-tnc2`, `tnc-2`, `tapr`, `tf`, `thefirmware` | 8N1 | `auto` | off | 9600 / 2400 |
| `pk232` | `pk-232`, `aea`, `pk87` | 8N1 | `kiss_on` | off | 9600 |
| `mfj1278` | `mfj-1278`, `mfj` | 7E1 | `auto` | off | 4800 |
| `kantronics` | `kpc`, `kpc3`, `kpc9612` | 8N1 | `kiss_on` | off | 9600 |
| `baycom` | `bay-com` | 8N1 KISS only | `kiss_on` | off | 2400 |
| `pccom` | `pc-com`, `albrecht` | 8N1 | — (hostmode) | off | 2400 |
| `generic` | `tnc` | 8N1 | `auto` | off | 2400 |

For native SER12/PAR96 (kernel hdlcdrv), use the [`baycom`](BAYCOM.md) plugin — not `packet_radio`.

Override serial line: `serial_line=7e1|8n1` or `data_bits`, `parity`, `stop_bits`, `rts_dtr`.

Radio speed on the TNC (1200 / 9600 AFSK or G3RUH) is still set on the **hardware** (jumpers, modem board). HyBBX `radio_baud` is QoS metadata for the HBX link.

## Hardware notes

### Landolt TNC2C

7E1, RTS/DTR required. `kiss on` at start. See Landolt TNC2C manual (V24).

### PK-TNC2 / TNC-2 class (TheFirmware TF2.7)

8N1. Terminal baud via board jumpers (1200–19200). KISS: `KISS ON` or ESC+`@K`. HyBBX profile: `tnc=tnc2`, `kiss_entry=auto`.

### AEA PK-232 / PK-87

8N1 @ 9600 typical. HyBBX sends PK-232 prep (`XFLOW OFF`, `AWLEN 8`, …) before KISS. `kiss on` after prep.

### MFJ-1278 (TNC-2 converted)

Often fixed **4800 7E1**. Profile `mfj1278`; try `kiss_entry=auto`.

### Kantronics KPC-3 / KPC-9612

KISS in standard firmware. 8N1, `kiss_on`.

### BayCom SER12 / parallel

Profile `baycom` in `packet_radio`: KISS @ 1200 on async serial (firmware TNC). Native SER12 bit-bang: [`baycom` plugin](BAYCOM.md).

## Multi-TNC on one Secondary

```ini
[networks]
ax25 = yes

[transport.packet_radio1]
tnc = tnc2c
device = /dev/ttyUSB0
link_id = edge-a
...

[transport.packet_radio2]
tnc = tnc2
device = /dev/ttyUSB1
link_id = edge-b
...
```

Each section needs a unique `device` and `link_id`. Main must list matching `[transport.packet_radioN]` registry rows.

`link_password` must be set when Main keeps `[circuit] link_auth=yes`
(default). If missing, the hub logs
`link_auth_fail ... reason=timeout_no_link_auth`.

## Not supported in HyBBX

- WA8DED **JHOST** binary polling (`JHOST 1`, `G` channel poll) — use KISS or simple host converse mode.
- USB-KISS adapters that expose `ax0` without serial — use Linux `kissattach` outside HyBBX or serial KISS TNC.
- Dedicated KISS-only ROM without command mode — set `kiss_entry=none` if already in KISS at power-on.

## References

- KISS protocol: Phil Karn / Mike Chepponis (1987).
- TheFirmware TF2.7: NORD-LINK e.V.
- PK-TNC2: Neuner-Funk / TNC-2 class documentation.
- TNC2C: Landolt Computer (7E1 host).
