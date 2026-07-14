# Master operator guide · HyBBX 2.4.0

Single linear guide: install, configure, attach RF via MAX25 prep, run clients and commands.

## Role matrix

| Role | `[networks]` | Hosts |
|------|--------------|-------|
| **Main** | `circuit=yes`, `ax25=no` (typical) | Users, HBX hub `:7323` |
| **Secondary** | `circuit=no`, `ax25=yes` | RF near TNC; `circuit_host` → Main |
| **Standalone Main** | `ax25=yes` on same box | Lab / single-host |

## Install matrix

| Step | Action |
|------|--------|
| Build | `scripts/build.sh` or CMake per [BUILD.md](BUILD.md) |
| Binary | `hybbxd -c hybbx.ini` or `hybbx-start` |
| Detached | `hybbx-start --screen` / `--tmux` |
| INI templates | `share/hybbx-*.ini.example` → copy to `./local/hybbx.ini` |

## INI essentials matrix

| Section | Purpose |
|---------|---------|
| `[service]` | Name, session limit, prompt |
| `[storage]` | `flatfile` or `sqlite` |
| `[networks]` | Plugin enable flags |
| `[transport.telnet]` | Bind, port `2323` |
| `[transport.circuit]` | HBX hub `:7323`, `max_links` (8 default, 16 max) |
| `[transport.packet_radioN]` | TNC instance — `tnc=`, `protocol=kiss` |
| `[broadcast]` | `ax25_mycall`, `ax25_auto_message`, `ax25_dest` |
| `[max25]` | `check=yes` — probe max25d `:7325` before local serial |

## MAX25 attach matrix (v2.4.0)

| Layer | Owner | Action |
|-------|-------|--------|
| Boot-wait, DTR/RTS, MYCALL, `kiss on` | MAX25 | `max25-ctl start --hardware tncs` |
| max25d reachability | HyBBX `[max25] check=yes` | TCP `:7325` — **required** for local TNC |
| KISS attach | HyBBX `packet_radio` | `kiss_entry=none` |
| AX.25 UI, HBX bridge, broadcast | HyBBX | `packet_radio.c`, `broadcast.c` |
| BayCom / PC-COM hardware | **MAX25 only** | `[networks] baycom=no` default |

## Start order matrix

| # | Action |
|---|--------|
| 1 | MAX25 prep — boot-wait or `max25d` per TNC port |
| 2 | HyBBX — `hybbxd -c hybbx.ini` with `kiss_entry=none`, `[max25] check=yes` |
| 3 | Verify — log: max25d reachable → KISS attach → `RF TX` on `/broadcast ax25` |

## Related

| Goal | Doc |
|------|-----|
| Full manual | [MANUAL.md](MANUAL.md) |
| TNC profiles | [TNCS.md](TNCS.md) |
| Dual-TNC | [AX25SRV-OPERATOR-GUIDE.md](AX25SRV-OPERATOR-GUIDE.md) |
