<!-- AUTO-SYNC 2026-07-15 — vault: projects/integration/2026-07-13-master-operator-guide.md -->
<!-- Re-sync: /home/akb/Code/0-RESEARCHES/tools/vault-sync-slave-docs.sh -->

# Dual-TNC operator guide · HyBBX

**Shipped guide** — canonical edits in research vault `projects/integration/2026-07-13-master-operator-guide.md`; run sync script after change.

## Summary

Linear operator sequence for a **standalone Main** host with **one or two local TNCs** (dual-TNC pattern): MAX25 prep first, HyBBX second, RF verification last. Applies to any dual-TNC reference station — adjust device paths and INI per site.

---

## 1. Scope

| In scope | Out of scope |
|----------|--------------|
| Single-host standalone Main + local `packet_radio` | Remote Secondary + Main split (see HyBBX TOPOLOGY) |
| TNC2C / PK-TNC2 class (TheFirmware) | BayCom-only or CRDOP-only sites (see MAX25 master) |
| MAX25 prep → HyBBX → `/broadcast ax25` | mains_proxy mesh |

Boundary SSoT: [2026-07-12-max25-hybbx-boundary-final.md](2026-07-12-max25-hybbx-boundary-final.md).

---

## 2. Responsibility split

| Phase | MAX25 | HyBBX |
|-------|-------|-------|
| TNC cold boot (DTR+RTS at power-on) | `tnc2c-boot-wait.sh` / max25d | — |
| Host MYCALL, TXDELAY, `kiss on` | max25d or boot-wait | — |
| Serial port ownership in operation | max25d (optional) **or** HyBBX after prep | Opens `/dev/ttyS*` after prep |
| max25d TCP probe | Listens `:7325` | `[max25] check=yes` |
| KISS DATA / UI frames / PTT path | — | `packet_radio` plugin |
| User sessions, `/broadcast ax25` | — | Main + HBX loopback |

**One process per `/dev/tty*`** — never minicom + boot-wait + HyBBX concurrently.

---

## 3. Pre-flight checklist

| # | Check | Pass criterion |
|---|-------|----------------|
| 1 | MAX25 scripts installed | `MAX25-Stack/stacks/tncs/tnc2c-boot-wait.sh` executable |
| 2 | HyBBX built with `packet_radio` | `hybbxd` starts, plugin loads |
| 3 | INI merged | Vault template: `share/` INI examples |
| 4 | `[broadcast] ax25_mycall` set | Valid callsign for on-air source |
| 5 | `kiss_entry=none` on each `[transport.packet_radioN]` | MAX25 owns KISS entry |
| 6 | Unique `link_id` per transport | Matches circuit bridge registry |
| 7 | Radio wired, antenna safe | Bench or low-power test first |
| 8 | Radio squelch profile | **Primary:** SQ **on** (manual/ASQ) while local DCD/decode OK; open SQ only if needed; fringe/DX → Secondary or second Main — RFG/FMQ **off**; SSoT: [../../hardware/radio-equipment/2026-07-14-packet-radio-squelch-rfg-profiles.md](../../hardware/radio-equipment/2026-07-14-packet-radio-squelch-rfg-profiles.md) |

Extended checklist: [2026-07-12-max25-tnc-prep-check.md](2026-07-12-max25-tnc-prep-check.md).

---

## 4. Linear start sequence

### Step A — Stop conflicting owners

```bash
pkill hybbxd || true
# ensure no minicom/screen on /dev/ttyS4 / ttyS5
```

### Step B — MAX25 TNC prep (per port)

For each TNC (example ports — replace per site):

```bash
cd /path/to/MAX25-Stack/stacks/tncs

# TNC unit A (cold boot or stuck host)
./tnc2c-boot-wait.sh --device /dev/ttyS4 --recover-only
# or full boot-wait with power cycle if DTR was low at cold start

# TNC unit B (PK-TNC2)
./pktnc2-boot-wait.sh --device /dev/ttyS5 --recover-only
```

**Optional:** start `max25d` instead of one-shot scripts — enables serial watch and M25/1:

```bash
max25-ctl start --hardware tncs --device tnc2c
ss -ltn | grep 7325    # expect LISTEN on 7325
```

Note: one `max25d` instance = one RF backend in v1; dual-TNC sites often use **scripts per port** then HyBBX as serial owner. See vault prep check for dual-TNC pattern.

### Step C — Verify MAX25 reachability

```bash
nc -zv 127.0.0.1 7325    # when max25d running
# or: confirm boot-wait log shows TheFirmware banner + KISS ready
```

### Step D — Start HyBBX

```bash
hybbxd -c /path/to/hybbx.ini
# or: hybbx-start
```

**Expected log sequence:**

1. `max25d reachable` (if `[max25] check=yes`)
2. `KISS attach (MAX25 prep assumed)` per `[transport.packet_radioN]`
3. Circuit hub listening `:7323`

### Step E — RF verification

| Test | Command / action | Pass |
|------|------------------|------|
| Local user session | `hybbx-telnet -H 127.0.0.1` | Login OK |
| Manual RF beacon | `/broadcast ax25` (Sysop) | Log `RF TX`; PTT at radio |
| On-air decode | Remote monitor or second radio | AX.25 UI frame with `ax25_mycall` |
| Dual link gap | Two transports configured | ~60 s between links (sequential) |

RF debug timeline: [../hybbx/2026-07-12-site-rf-broadcast-investigation.md](../hybbx/2026-07-12-site-rf-broadcast-investigation.md).

---

## 5. INI pointers (dual local TNC)

Site INI: merge `share/` examples per deployment.

| Section | Keys to verify |
|---------|----------------|
| `[networks]` | `circuit=yes`, `ax25=yes` |
| `[transport.circuit]` | `port=7323`, `max_links` ≥ transport count |
| `[transport.packet_radio1]` | `device`, `tnc`, `link_id`, `link_password`, `kiss_entry=none`, `persist=255` |
| `[transport.packet_radio2]` | same pattern — different `device`, `link_id` |
| `[broadcast]` | `ax25_mycall`, `ax25_auto_message`, `ax25_dest` |
| `[max25]` | `check=yes`, `host=127.0.0.1`, `port=7325` |

HyBBX shipped templates: `hyBBX/share/hybbx-standalone.ini.example`.

---

## 6. Recovery without full host reboot

| Symptom | Action |
|---------|--------|
| Echo-only on one TNC | Stop HyBBX → `--recover-only` on that port → restart HyBBX |
| max25d `error-host` | Serial watch auto-repair; or manual `tnc2c-host-reset.sh --kiss` |
| No PTT, log shows `RF TX` | Check FCS strip, `persist=255`, MYCALL prep — vault RF investigation |
| Stuck after HyBBX crash | KISS return frame before re-prep; do not skip MAX25 step |

Runbook: [operations/runbooks/tnc-recovery-without-power-cycle.md](../../operations/runbooks/tnc-recovery-without-power-cycle.md).

---

## 7. Reboot order (reference station)

After host power cycle:

1. **Physical:** TNC power with DTR high (boot-wait script running **before** or **during** TNC power-on)
2. **MAX25:** boot-wait per TNC (mandatory for TNC2C cold boot)
3. **HyBBX:** start immediately after successful prep (HyBBX re-asserts RTS/DTR)
4. **Verify:** `/broadcast ax25` within 5 min of uptime

Do **not** start HyBBX before TNC host mode is confirmed — HyBBX cannot recover Landolt cold-boot without DTR sequencing.

---

## Related

| Topic | Path |
|-------|------|
| MAX25 ↔ HyBBX boundary | [2026-07-12-max25-hybbx-boundary-final.md](2026-07-12-max25-hybbx-boundary-final.md) |
| TNC prep checklist | [2026-07-12-max25-tnc-prep-check.md](2026-07-12-max25-tnc-prep-check.md) |
| HyBBX master guide | [../hybbx/2026-07-13-master-documentation.md](../hybbx/2026-07-13-master-documentation.md) |
| MAX25 master guide | [../max25-stack/2026-07-13-master-documentation.md](../max25-stack/2026-07-13-master-documentation.md) |
| RF investigation | [../hybbx/2026-07-12-site-rf-broadcast-investigation.md](../hybbx/2026-07-12-site-rf-broadcast-investigation.md) |
| Production INI | `share/` INI examples |
| TNC recovery runbook | [operations/runbooks/tnc-recovery-without-power-cycle.md](../../operations/runbooks/tnc-recovery-without-power-cycle.md) |
| TNC2multi 1200/9600 (Amateurfunk) | [hardware/tnc2multi/2026-07-14-tnc2multi-modulation-1200-9600.md](../../hardware/tnc2multi/2026-07-14-tnc2multi-modulation-1200-9600.md) |
| Packet Squelch/RFG (Primary/Secondary) | [hardware/radio-equipment/2026-07-14-packet-radio-squelch-rfg-profiles.md](../../hardware/radio-equipment/2026-07-14-packet-radio-squelch-rfg-profiles.md) |
