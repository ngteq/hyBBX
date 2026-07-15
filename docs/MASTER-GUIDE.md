<!-- AUTO-SYNC 2026-07-15 — vault: projects/hybbx/2026-07-13-master-documentation.md -->
<!-- Re-sync: /home/akb/Code/0-RESEARCHES/tools/vault-sync-slave-docs.sh -->

# HyBBX — Master operator guide

**Shipped guide** — canonical edits in research vault `projects/hybbx/2026-07-13-master-documentation.md`; run sync script after change.

## Summary

Single linear guide: what HyBBX is, how to install and configure it, attach RF via MAX25 prep, run clients and commands. Shipped product docs in `docs/` remain **frozen reference** — depth investigations live.

---

## 1. Role and architecture

HyBBX is a **text-first BBX/mail/chat** daemon (`hybbxd`) with optional RF transports. Users connect via telnet (`:2323`), SSH (`:3232`), or WebSocket proxy. RF paths use transport plugins that bridge serial/KISS or kernel modems to **HBX** (Hybrid Bridge eXchange) on the circuit hub (`:7323`).

```
Users (telnet/SSH/WebSocket) ──► Main (storage, mail, HBX hub :7323)
                                        ▲
                                        │ HBX/TCP
                                   Secondary (packet_radio / baycom / crdop)
                                        │
                                   TNC / modem (after MAX25 prep)
```

| Role | `[networks]` | Hosts |
|------|--------------|-------|
| **Main** | `circuit=yes`, `ax25=no` (typical) | Users, HBX hub |
| **Secondary** | `circuit=no`, `ax25=yes` | RF near TNC; `circuit_host` → Main |
| **Standalone Main** | `ax25=yes` on same box | Lab / single-host (e.g. dual local TNC) |

**HBX/Circuit** — sole inter-node transport. Application core never sees raw KISS or on-air AX.25; only typed HBX frames on TCP.

Frozen reference: `docs/TOPOLOGY.md`, `docs/MANUAL.md`.

---

## 2. Install and build

| Step | Action |
|------|--------|
| Build | `hyBBX/scripts/build.sh` or CMake per `docs/BUILD.md` |
| Binary | `hybbxd` — start via `hybbx-start` or `hybbxd -c hybbx.ini` |
| Detached | `hybbx-start --screen` / `--tmux`; attach: `hybbxd --screen --attach` |
| INI templates | `hyBBX/share/hybbx-standalone.ini.example`, `hybbx-main.ini.example`, `hybbx-secondary.ini.example`, `hybbx-mesh.ini.example` |

First start creates Sysop in `users/users.ini` with a one-time random password on the console.

---

## 3. INI — operator essentials

Full key tables: frozen `docs/MANUAL.md`. Vault INI copy (reference station): `share/` INI examples.

### Core sections

| Section | Purpose |
|---------|---------|
| `[service]` | Name, session limit, prompt |
| `[storage]` | `flatfile` or `sqlite`; user shards under `users/` |
| `[networks]` | Enable plugins: `ax25`, `baycom`, `crdop`, `circuit`, `ssh`, `websocket` |
| `[transport.telnet]` | Bind, port `2323` |
| `[transport.circuit]` | HBX hub port `7323`, `max_links` (default 8, max 16) |
| `[transport.packet_radioN]` | TNC instance — device, `tnc=` profile, `protocol=kiss` |
| `[broadcast]` | `ax25_mycall`, `ax25_auto_message`, `ax25_dest` |
| `[max25]` | `check=yes` — probe max25d TCP `:7325` before local serial open |

### RF on Main vs Secondary

| Layout | TNC keys on Main `[transport.packet_radioN]` |
|--------|-----------------------------------------------|
| Remote Secondary | Bridge registry only (`link_id`, password) — no `device` |
| Local TNC (standalone) | Full keys: `device`, `tnc`, `baud`, `serial_line`, `rts_dtr` |

Bridge-registry rows without `device` are skipped at start (no serial open).

---

## 4. TNC attach (MAX25 contract) — v2.4.0

**Order:** MAX25 prep **before** HyBBX. One process per `/dev/tty*`. HyBBX is **attach-only** — no BayCom/PC-COM in `packet_radio` (`tnc=baycom|pccom` rejected).

| Layer | Owner | Action |
|-------|-------|--------|
| Boot-wait, DTR/RTS, MYCALL, `kiss on` | MAX25 | `max25-ctl start --hardware tncs` or `tnc2c-boot-wait.sh` |
| max25d reachability | HyBBX `[max25] check=yes` | TCP `:7325` — **required** for local TNC (start fails if down) |
| KISS attach | HyBBX `packet_radio` | `kiss_entry=none` (default with MAX25) |
| AX.25 UI build, HBX bridge, broadcast | HyBBX | `broadcast.c`, `packet_radio.c`, `tnc.c` |
| BayCom / PC-COM hardware | **MAX25 only** | HyBBX `[networks] baycom=no` default; enable BayCom in MAX25 `[features]` |

HyBBX builds outbound UI frames from `[broadcast] ax25_mycall` — it does **not** send host `MYCALL` to the TNC (MAX25 does that at prep).

| INI key | Production value | Notes |
|---------|------------------|-------|
| `kiss_entry` | `none` | TNC already in KISS after MAX25 |
| `kiss_exit` | `none` | Shutdown does not send `kiss off` |
| `persist` | `255` on CB | CSMA — avoid random defer on busy channel |
| `protocol` | `kiss` | Required for Secondary RF |
| `[max25] check` | `yes` | Local serial edges only |

TNC profiles in `packet_radio`: `tnc2c`, `tnc2`, `pk232`, … — **not** `baycom` / `pccom`. Frozen `docs/TNCS.md` may lag until doc sync.

Release prep: [2026-07-14-v2.4.0-release-prep.md](2026-07-14-v2.4.0-release-prep.md).

Recovery without power cycle: internal research note) · MAX25 `stacks/tncs/docs/TNC-RECOVERY.md`.

---

## 5. RF, broadcast, and HBX

### Auto-beacon and manual broadcast

| Command | Scope | Sysop |
|---------|-------|-------|
| `/broadcast <msg>` | Local logged-in users (telnet/SSH/WebSocket) | yes |
| `/broadcast ax25` | Sequential RF beacon per link (`ax25_auto_message`; 60 s gap) | yes |

RF path: Main → HBX → Secondary link → KISS → TNC → on-air.

###  production facts (vault)

Dual local TNC standalone Main — full timeline and fixes: [2026-07-12-site-rf-broadcast-investigation.md](2026-07-12-site-rf-broadcast-investigation.md).

| Issue class | Vault SSoT fix |
|-------------|----------------|
| No PTT despite `RF TX` log | FCS strip, MYCALL before KISS, UNPROTO path, `persist=255` |
| Circuit queue dropped beacons | Bypass low-prio queue for AX.25 broadcast TX |
| `ax25_dest=*` invalid | Map to `QST` |
| TNC stuck after crash | KISS return frame `0xC0 0xFF 0xC0` before prep |

### BayCom and CRDOP plugins

| MAX25 hardware | HyBBX plugin | INI |
|----------------|--------------|-----|
| `hardware/modems` | `baycom` | `[networks] baycom=yes` — merge `share/hybbx/baycom-ser12-host.ini.example` |
| `hardware/soft-modems` | `crdop` | `[networks] crdop=yes` — merge `share/hybbx/crdop-host.ini.example` |

Frozen: `docs/BAYCOM.md`, `docs/CRDOP.md` · MAX25 contract: `docs/HYBBX.md`.

---

## 6. Clients

| Client | Transport | Default port |
|--------|-----------|--------------|
| `hybbx-telnet` | TCP | 2323 |
| `hybbx-ssh` | SSH (libssh) | 3232 |
| `hybbx-terminal` | HBX circuit | 7323 |
| Web browser | WebSocket via reverse proxy | per site |

Frozen: `docs/CLIENTS.md`, `docs/WEBSOCKET.md`.

---

## 7. Commands and access levels

Five levels: Sysop → Admin → Mod → User → Guest. `/help`, `/menu` filtered by level; `/index` lists all commands for every account.

| Sysop RF | `/broadcast`, `/broadcast ax25`, `/shutdown` |
|----------|-----------------------------------------------|
| Admin | `/usercreate`, `/activate`, `/promote`, `/demote` |

Frozen: `docs/COMMANDS.md`, `share/commands.yaml`, `share/areas.yaml`.

---

## 8. Operator start order (with MAX25)

Linear sequence for standalone Main + local TNC:

1. **MAX25 prep** — boot-wait or `max25d` per TNC port (or verify `ss -ltn | grep 7325`)
2. **HyBBX** — `hybbxd -c hybbx.ini` with `kiss_entry=none`, `[max25] check=yes`
3. **Verify** — log: `max25d reachable` → `KISS attach (MAX25 prep assumed)` → `RF TX` on `/broadcast ax25`
4. **RF check** — PTT and on-air decode at remote station

Integration detail: [../integration/2026-07-13-master-operator-guide.md](../integration/2026-07-13-master-operator-guide.md).

---

## 9. Future roadmap (deferred)

Aligned with MAX25 [V2.0.0-SCOPE](../../10-PROJECTS/docs/V2.0.0-SCOPE.md) **Later** row — **no product commits** until operator orders a separate track.

| Later | Status |
|-------|--------|
| **`/aichat`** — AI / assistant session command | **deferred** |
| AI / assistant integration (backends, APIs) | **deferred** |
| like-features | **deferred** |

Full map: [2026-07-14-hybbx-future-roadmap.md](2026-07-14-hybbx-future-roadmap.md).

**v2.4.0:** `/who` lists **interactive users only** (telnet/SSH/WebSocket) — plugin/RF connection sessions are not users.

---

## Related

| Topic | Path |
|-------|------|
| RF investigation SSoT | [2026-07-12-site-rf-broadcast-investigation.md](2026-07-12-site-rf-broadcast-investigation.md) |
| tnc.c recovery integration | [2026-07-13-tnc-recovery-integration.md](2026-07-13-tnc-recovery-integration.md) |
| INI operator notes | [2026-07-12-ini-operator-notes.md](2026-07-12-ini-operator-notes.md) |
| MAX25 boundary | [../integration/2026-07-12-max25-hybbx-boundary-final.md](../integration/2026-07-12-max25-hybbx-boundary-final.md) |
| Dual-TNC operator flow | [DUAL-TNC-OPERATOR-GUIDE.md](DUAL-TNC-OPERATOR-GUIDE.md) |
| Frozen product docs | `docs/` (read-only) |
| Production INI (vault) | `share/` INI examples |
| TNC hardware SSoT | [hardware/tnc2c/CONFIRMED.md](../../hardware/tnc2c/CONFIRMED.md) |
