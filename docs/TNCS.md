# Supported TNCs — packet_radio plugin

**v2.0.0** · INI: [MANUAL.md](MANUAL.md) · one plugin: `packet_radio`

HyBBX uses a single configurable `packet_radio` transport. Each `[transport.packet_radioN]` section on a **Secondary** opens one serial (or USB) TNC and one HBX link to Main. Up to **8** instances per process (`HYBBX_PACKET_RADIO_MAX_INSTANCES`).

On **Main**, sections without `device` / `tnc` / `protocol` are bridge-registry rows only (`link_id`, password, frequency). Add those keys on Main for a **local TNC** (standalone).

## RF prep (MAX25)

TNC boot-wait, DTR/RTS power-on, host **`MYCALL`**, **`kiss on`**, TXDELAY/Persist defaults, and modem line setup live in **MainAX25-Stack (MAX25)** — not in HyBBX.

1. Run MAX25 device prep (`max25-ctl start --hardware tncs`, boot-wait script, or `max25d`).
2. TNC must be in **KISS** with valid MYCALL before HyBBX starts.
3. HyBBX opens the serial port and attaches (`kiss_entry=none` default).

Contract: MAX25 `docs/HYBBX.md` + `share/hybbx-*.ini.example`. Technical AX.25 facts: MAX25 `docs/PACKET-RADIO.md`.

HyBBX `[max25] check=yes` (default) probes max25d TCP (**7325**) before opening local serial TNC ports; without max25d, local AX.25 is skipped. Set `check=no` for legacy standalone (`kiss_entry=kiss_on`). See [MANUAL.md](MANUAL.md).

Do not run minicom and HyBBX on the same port. Only one process owns `/dev/tty*`.

## Host protocols

| Protocol | INI `protocol=` | Use |
|----------|-----------------|-----|
| KISS | `kiss` (default) | Secondary RF edge, APRS-style AX.25 frames |
| TNC2 host converse | `hostmode`, `host`, `tnc2` | Interactive host session (not WA8DED JHOST binary) |
| 6PACK | `sixpack` | DF6BU 6PACK over serial |

Recommended for HyBBX Secondary: **KISS** after MAX25 prep.

## AX.25 source call (HyBBX)

HyBBX builds outbound UI frames from INI — it does **not** send host `MYCALL` to the TNC (MAX25 does that).

Set **`[broadcast] ax25_mycall`** once for local beacons and RF TX source address. Optional per-transport `mycall=` when `[broadcast]` has no `ax25_mycall`.

**KISS DATA** frames to the TNC carry AX.25 **without FCS** — TheFirmware adds the CRC on air. HyBBX strips FCS from built frames before KISS encode.

**TheFirmware UI TX:** HyBBX may use host `UNPROTO` for reliable PTT on TNC-2 class firmware (KISS for RX). Requires MAX25 to have left the TNC in host-capable firmware.

## KISS entry / exit

| INI key | Values | Notes |
|---------|--------|-------|
| `kiss_entry` | `none` (default), `kiss_on`, `esc_at_k`, `auto` | `none` = attach after MAX25 prep |
| `kiss_exit` | `none` (default with `kiss_entry=none`), `kiss_off`, `kiss_frame`, `auto` | HyBBX shutdown only |

Legacy standalone (no MAX25): set `kiss_entry=kiss_on` or `auto` per profile — HyBBX enters KISS itself.

- **`none`** — TNC already in KISS; HyBBX attaches only (MAX25 / manual prep).
- **`kiss_on`** — send `kiss on` (TheFirmware, TNC2C, PK-232, Kantronics).
- **`esc_at_k`** — send ESC + `@K` (older TheFirmware / Landolt TF2.7 style).
- **`auto`** — try `kiss on`, then ESC+`@K` (`tnc2`, `generic`, `mfj1278`).
- **`kiss_frame`** exit — send `0xC0 0xFF 0xC0` (KISS Return); used when entry was ESC+`@K` and `kiss_exit=auto`.

## TNC profiles (`tnc=`)

| Profile | Aliases | Host serial | KISS attach | RTS/DTR | Typical host baud |
|---------|---------|-------------|-------------|---------|-------------------|
| `tnc2c` | `tnc2-c` | 7E1 | `none` (MAX25) | boot-wait | 2400 (TERM jumper) |
| `tnc2` | `pktnc2`, `pk-tnc2`, `tnc-2`, `tapr`, `tf`, `thefirmware` | 8N1 | `none` (MAX25) | boot-wait | 9600 / 2400 |
| `pk232` | `pk-232`, `aea`, `pk87` | 8N1 | `none` (MAX25) | off | 9600 |
| `mfj1278` | `mfj-1278`, `mfj` | 7E1 | `none` (MAX25) | off | 4800 |
| `kantronics` | `kpc`, `kpc3`, `kpc9612` | 8N1 | `none` (MAX25) | off | 9600 |
| `baycom` | `bay-com` | 8N1 KISS only | `none` (MAX25) | off | 2400 |
| `pccom` | `pc-com`, `albrecht` | 8N1 | — (hostmode) | off | 2400 |
| `generic` | `tnc` | 8N1 | `none` (MAX25) | off | 2400 |

For native SER12/PAR96 (kernel hdlcdrv), use the [`baycom`](BAYCOM.md) plugin — not `packet_radio`.

Override serial line: `serial_line=7e1|8n1` or `data_bits`, `parity`, `stop_bits`, `rts_dtr`.

Radio speed on the TNC (1200 / 9600 AFSK or G3RUH) is set on the **hardware** or by MAX25 prep. HyBBX `radio_baud` is QoS metadata for the HBX link.

## Hardware notes

### Landolt TNC2C

7E1, RTS/DTR during MAX25 boot-wait. See Landolt TNC2C manual (V24) and MAX25 `stacks/tncs/`.

### PK-TNC2 / TNC-2 class (TheFirmware TF2.7)

8N1. Terminal baud via board jumpers (1200–19200). MAX25 boot-wait + MYCALL + KISS entry.

### AEA PK-232 / PK-87

8N1 @ 9600 typical. MAX25 handles PK-232 host prep before KISS.

### MFJ-1278 (TNC-2 converted)

Often fixed **4800 7E1**. Profile `mfj1278`.

### Kantronics KPC-3 / KPC-9612

KISS in standard firmware. 8N1.

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
- TNC boot-wait, power-on DTR sequencing, or host prep without MAX25 — use MAX25 or set `kiss_entry` explicitly for legacy attach.

## References

- KISS protocol: Phil Karn / Mike Chepponis (1987).
- TheFirmware TF2.7: NORD-LINK e.V.
- PK-TNC2: Neuner-Funk / TNC-2 class documentation.
- TNC2C: Landolt Computer (7E1 host).
- MAX25: MainAX25-Stack `docs/HYBBX.md`, `docs/PACKET-RADIO.md`.
