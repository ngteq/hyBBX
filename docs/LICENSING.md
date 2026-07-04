# HyBBX licensing

**HyBBX** (not a separate “HyBBS” product) is a single GPL-3.0 project. This document lists licenses that apply today and licenses relevant to **ARDOP** integration and the planned **CRDOP** (CB Radio Digital Open Protocol) fork.

> **Not legal advice.** Confirm distribution and fork plans with counsel before shipping **external** modem binaries (ARDOPC, CRDOPC). HyBBX itself does not ship modem DSP code.

---

## External modem rule

**HyBBX is plugin-only** — the default for the whole project. Modems, TNCs, and sound-card software are **external services**; HyBBX ships host-client bridge plugins only. Embedded modem or sound-card DSP inside the daemon is a **different product**, not HyBBX.

---

## HyBBX project license

| Item | License |
|------|---------|
| HyBBX core, plugins, clients, docs (ngteq) | **GNU GPL v3** — [LICENSE.txt](../LICENSE.txt) |

GPL-3.0 requires that distributed binaries include source (or a written offer) and that derivative works of GPL-covered combined works remain under GPL-compatible terms.

---

## Bundled third-party code (compiled into HyBBX)

| Component | Path | License | Notes |
|-----------|------|---------|-------|
| Monocypher | `third_party/monocypher/` | **BSD-2-Clause OR CC0-1.0** | SPDX in headers; pick one when redistributing |
| tiny-AES-c | `third_party/tinyaes/` | **Public domain** | [kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c) |
| tinysha256 | `third_party/tinysha256/` | **The Unlicense** | Adapted from [983/SHA-256](https://github.com/983/SHA-256) |

Optional **external** backends (not vendored): OpenSSL, libsodium — their licenses apply only when you link them at build time (`CMake` options).

---

## ARDOP / ARDOPC upstream (external modem — not shipped with HyBBX)

HyBBX **0.8.x** does **not** bundle ARDOPC. The `ardop` plugin is a **clean-room Host-Client** over TCP — see [ARDOP.md](ARDOP.md).

| Upstream | Repository | Stated / inferred license | HyBBX use |
|----------|------------|---------------------------|-----------|
| **ARDOPC** (John Wiseman G8BPQ) | [g8bpq/ardop](https://github.com/g8bpq/ardop) | **No `LICENSE` file in repo**; Linux packagers (e.g. NixOS `ardopc`) treat ARDOPC as **GPL-3.0-only** | Operator runs separately; HyBBX connects as host |
| **ardopcf** (Peter LaRue et al.) | [pflarue/ardop](https://github.com/pflarue/ardop) | **MIT** (Rick Muething, John Wiseman, Peter LaRue) | Alternative external modem; same host TCP model |
| **hamarituc/ardop** fork | Packaging fork of g8bpq tree | **GPL-3.0-only** (package metadata) | Same as ARDOPC |

### HyBBX `plugins/ardop/` (Host-Client only)

| File | Origin | License |
|------|--------|---------|
| `ardop.c`, `ardop_config.c`, `ardop_host.c`, `ardop_host.h` | HyBBX (ngteq) | **GPL-3.0** (project default) |
| `ardop_crc.c` | Algorithm reimplemented from public ARDOPC reference (CRC-16 poly `0x8810`) | **GPL-3.0** (HyBBX authorship; not a verbatim copy of ARDOPC sources) |

**CRC note:** CRC-16 provides **error detection** on frames. **Error recovery** on ARDOP is via **ARQ retransmission** (link-layer), not CRC alone.

### Integration models (legal + practical)

| Model | When | License impact |
|-------|------|----------------|
| **A — External ARDOPC** (current 0.8.x) | Modem runs as separate process | HyBBX plugin stays GPL-3.0; no ARDOP source in HyBBX tree; operator supplies ARDOPC under its own terms |
| **B — External CRDOPC** | [ngteq/CRDOP](https://github.com/ngteq/CRDOP) 1.0.0 | CRDOPC **MIT**; HyBBX plugin GPL-3.0; no linking |
| **C — External CRDOPC from g8bpq fork** | GPL ARDOPC lineage | CRDOPC as **GPL-3.0** standalone; HyBBX plugin unchanged |
| **D — Clean-room CRDOPC** | New external modem + HyBBX host client | HyBBX GPL for plugin; CRDOPC licensed separately |

**Recommended path for HyBBX:** **A** for amateur ARDOP; **B** for CRDOP ([ngteq/CRDOP](https://github.com/ngteq/CRDOP)). Never vend DSP inside HyBBX.

---

## CRDOP / CRDOPC (external modem + HyBBX plugin)

**CRDOP** = CB-oriented digital open protocol — **not** a 1:1 ARDOP clone. Modem: **[ngteq/CRDOP](https://github.com/ngteq/CRDOP)** **1.0.0** (MIT). HyBBX bridge: [CRDOP.md](CRDOP.md), [ARDOP.md](ARDOP.md).

| Component | License |
|-----------|---------|
| HyBBX `plugins/crdop/` (host bridge) | **GPL-3.0** |
| **CRDOPC** ([ngteq/CRDOP](https://github.com/ngteq/CRDOP)) | **MIT** (ngteq + ardopcf upstream notices — [NOTICE.md](https://github.com/ngteq/CRDOP/blob/main/NOTICE.md)) |
| HyBBX CRDOP docs | GPL-3.0 (in this repo) |

**Integration model:** external CRDOPC process + HyBBX `crdop` plugin over host TCP — same as ARDOP/ARDOPC (**no** modem code linked into `hybbx`).

---

## Deprecated note

Earlier docs referred to CRDOP as “planned after v1.0.0” only. The **modem** is now released separately at [ngteq/CRDOP](https://github.com/ngteq/CRDOP). HyBBX live RF verification remains post–v1.0.0.

---

## Device and software-modem compatibility (operator responsibility)

HyBBX works with **any legally usable** external packet-radio device or sound-card **modem program** the operator runs (serial TNC, KISS, external ARDOPC/CRDOPC, etc.). HyBBX plugins do not implement modem DSP. **Radio regulations** (band, power, emission type) are always the operator’s duty.

---

## Attribution checklist for distributors

1. Ship [LICENSE.txt](../LICENSE.txt) (GPL-3.0).
2. Include third-party notices for Monocypher, tiny-AES-c, tinysha256 (see table above).
3. If you distribute **CRDOPC** or ARDOPC alongside HyBBX, ship that program’s `LICENSE`/`NOTICE` with it.
4. If distributing **only** HyBBX + external ARDOPC, document that ARDOPC is a separate program (GPL-3.0 or MIT depending on build).

---

## References

- [MANUAL.md — Licensing](MANUAL.md#licensing)
- [CRDOP.md](CRDOP.md) · upstream modem [ngteq/CRDOP](https://github.com/ngteq/CRDOP)
- [FEATURES.md](FEATURES.md)
- ARDOPC operator doc: [cantab.net ARDOPC](https://www.cantab.net/users/john.wiseman/Documents/ARDOPC.html)
- ardopcf MIT: [pflarue/ardop LICENSE](https://github.com/pflarue/ardop/blob/master/LICENSE)
