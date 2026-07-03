# CRDOP — CB Radio Digital Open Protocol (Level 2, experimental)

**Status:** Experimental **Level 2 development** — planned **after HyBBX v1.0.0** (or later). Not part of the v1.0.0 release scope.

**CRDOP** is a HyBBX-side name for a **CB-applicable digital protocol stack**, inspired by [ARDOP](https://github.com/g8bpq/ardop) ideas but **not a 1:1 copy**. It targets global CB packet techniques where legally permitted, with **configurable** profiles rather than fixed amateur-band assumptions.

Licensing: see [LICENSING.md](LICENSING.md). HyBBX remains **GPL-3.0**.

---

## HyBBX is not a modem service

**HyBBX is plugin-only** — the default for this entire project, not only ARDOP/CRDOP. Session core plus host-client bridge plugins. Modems, TNCs, sound-card software, ARDOPC, and CRDOPC are **external**. Embedded sound-modem or RF DSP inside the daemon = **different project, not HyBBX**.

All RF and modem work lives in **external processes** the operator runs:

| External service | HyBBX side |
|------------------|------------|
| Serial/USB TNC (KISS, TNC2 host mode) | `packet_radio` plugin (byte stream to device) |
| **ARDOPC** / ardopcf (sound-card or other I/O) | `ardop` plugin (Host-Client TCP) |
| **CRDOPC** (planned standalone, Level 2) | `ardop` / future `crdop` plugin (same host TCP model) |

HyBBX plugins are **link adapters only** — session core, HBX/TCP bridge, INI config. **ARDOP** and **CRDOP** are usable with HyBBX only through these plugins talking to external modem/TNC services.

---

## Relationship to ARDOP and HyBBX

```
  Phase 0 (0.8.x)     External ARDOPC + HyBBX ardop plugin (Host-Client TCP)
  Phase 1 (v1.0.0)    HyBBX stable; ARDOP bridge remains optional / experimental
  Phase 2 (post-1.0)  CRDOP spec + standalone external CRDOPC (not inside HyBBX)
```

| Layer | 0.8.x (today) | CRDOP (Level 2) |
|-------|---------------|-----------------|
| RF / sound modem | **External** ARDOPC | **External** CRDOPC (separate program; fork of ardopcf/GPL ARDOPC, or clean-room) |
| Host interface | ARDOP TNC TCP subset | CRDOP host profile (ARDOP-compatible where useful) |
| HyBBX bridge | `plugins/ardop` → HBX `terminal` | `transport.crdop` or extended `ardop` with `protocol=crdop` |
| CB defaults | `radio_profile=cb` in INI | Bandwidth, half-duplex, MHz, callsign rules |

---

## Design principles

1. **Configurable** — every CB-applicable ARDOP element is optional via INI (`radio_profile`, `arq_bandwidth`, `listen`, `peer_call`, `frequency_mhz`, …).
2. **Not 1:1** — skip amateur-only or non-CB features unless an operator enables them.
3. **Error detection + recovery** — **CRC-16** (poly `0x8810`, same family as ARDOP host frames) for detection; **ARQ** for retransmission-based recovery (not FEC-first on CB).
4. **Half-duplex first** — CB is shared medium; full-duplex only when hardware and law allow.
5. **External sound-card modems** — ALSA/Pulse/WASAPI and audio DSP stay in **CRDOPC** (or ARDOPC), same as today; HyBBX never opens a sound device for modem use.
6. **Legal devices only** — operator configures band and hardware; HyBBX does not bypass type approval.

---

## ARDOP elements → CRDOP applicability (configurable)

| ARDOP element | CB use | CRDOP default | Config key |
|---------------|--------|---------------|------------|
| Host TCP (port + port+1) | Yes | Yes | `ardop_host`, `ardop_port` |
| CRC-16 host frames | Yes | Yes | (always on wire) |
| `INITIALIZE` / `MYCALL` | Yes | Yes | `mycall` |
| `PROTOCOLMODE ARQ` | Yes | Yes | fixed ARQ for data bridge |
| `ARQBW` (200/500/1000/2000…) | Yes | `500MAX` on CB | `arq_bandwidth` |
| `LISTEN` / `ARQCALL` | Yes | Yes | `listen`, `peer_call` |
| `d:ARQ` data | Yes | Yes | payload path |
| FEC / OFDM modes | Limited | Off | future `fec_mode` |
| PTC / Dragon emulation | No | Off | not planned for CRDOP |
| Hamlib CAT / rig control | Optional | Off | future plugin, not core |

---

## `radio_profile` (0.8.x preview)

The `ardop` plugin accepts an experimental profile for early CB testing against **external** ARDOPC:

```ini
[transport.ardop1]
radio_profile = cb      ; cb | amateur (default: amateur)
arq_bandwidth = 500MAX  ; cb default when profile=cb and unset
frequency_mhz = 27.205
```

CB profile forces **half-duplex** QoS on the HBX circuit link and logs a warning if `arq_bandwidth` exceeds `1000MAX`.

Full **CRDOPC** modem software is **not** part of HyBBX and is **not** planned inside the HyBBX tree — only the host-client plugin and protocol docs.

---

## Fork / merge sources (for standalone CRDOPC, external)

| Source | License | CRDOP strategy (external daemon) |
|--------|---------|----------------------------------|
| [pflarue/ardop](https://github.com/pflarue/ardop) (ardopcf) | **MIT** | Preferred base for a **separate** CRDOPC binary; HyBBX plugin unchanged (TCP host) |
| [g8bpq/ardop](https://github.com/g8bpq/ardop) (ARDOPC) | **GPL-3.0** (packager consensus; no root LICENSE file) | GPL fork → standalone CRDOPC under GPL-3.0 |
| Clean-room implementation | HyBBX GPL for plugin only | New external CRDOPC + host subset in HyBBX; no upstream paste |

---

## Documentation map

| Doc | Content |
|-----|---------|
| [LICENSING.md](LICENSING.md) | All licenses |
| [MANUAL.md](MANUAL.md#ardop-external-ardopc--hybbx-host-client) | Operator INI |
| [ROADMAP.md](ROADMAP.md) | Schedule |
| [FEATURES.md](FEATURES.md) | Shipped vs planned |

---

## Trademark note

“ARDOP” and “ARDOPC” refer to the amateur-radio digital open protocol and its reference implementations. **CRDOP** is a HyBBX working name for a CB-oriented profile/fork; use distinct branding in releases to avoid implying endorsement by upstream authors.
