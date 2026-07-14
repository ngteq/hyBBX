# Topology · HyBBX 2.4.0

Main, Secondary, and mains-proxy mesh layout.

## Role matrix

| Role | Process | Typical `[networks]` | Hosts |
|------|---------|----------------------|-------|
| **Main** | `hybbxd` | `circuit=yes` | Users, storage, HBX hub `:7323` |
| **Secondary** | `hybbxd` (remote) | `circuit=no`, `ax25=yes` | RF; HBX client to Main |
| **Standalone Main** | `hybbxd` | `ax25=yes` on same box | Lab / dual local TNC |

## RF attachment matrix

| Layout | TNC host | Main `[transport.packet_radioN]` |
|--------|----------|----------------------------------|
| Remote Secondary | Secondary (`device`, `tnc`, `circuit_host`) | Bridge registry only (`link_id`, password) |
| Local TNC on Main | Same box as users | Full TNC keys (`device`, `tnc`, …) |

## HBX/Circuit matrix

| Item | Value |
|------|-------|
| Protocol | HBX v1 — magic `HBX` |
| Default port | `7323` |
| Max links | 16 (default `max_links=8`) |
| Auth | Per-link `link_password` |
| Scope | Sole inter-node transport — core never sees raw KISS |

## Broadcast matrix

| Command | Scope | Level |
|---------|-------|-------|
| `/broadcast <msg>` | Local logged-in users | Sysop |
| `/broadcast ax25` | Sequential RF beacon (60 s link gap) | Sysop |

## Related

| Goal | Doc |
|------|-----|
| Manual | [MANUAL.md](MANUAL.md) |
| Mesh proxy | [MAINS_PROXY.md](MAINS_PROXY.md) |
| Dual-TNC | [AX25SRV-OPERATOR-GUIDE.md](AX25SRV-OPERATOR-GUIDE.md) |
