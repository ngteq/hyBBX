# CRDOP — HyBBX host-client plugin

**Status:** Experimental Level 2 (0.8.x) — CB host-TCP bridge implemented; live RF integration tests planned after v1.0.0.

Standalone reference for **CRDOP** (CB Radio Digital Open Protocol), the **`crdop`** transport plugin, external **CRDOPC** modem, INI, and developer layout. For amateur-band ARDOP use **`ardop`** — see [ARDOP.md](ARDOP.md).

---

## What this is

**CRDOP** is a CB-oriented digital protocol — configurable merge of ARDOP-applicable elements, **not** a 1:1 ARDOP clone. HyBBX implements **only** the host-client bridge:

1. Same **ARDOP TNC Host Interface** over TCP (control **N**, data **N+1**).
2. HBX **Secondary** client to Main (`circuit_host` / `[circuit]`).
3. **CB defaults** fixed in the `crdop` plugin (`500MAX`, half-duplex QoS, `crdop-link`).

**CRDOPC** (external modem) owns sound-card DSP, ARQ over-the-air, and RF. HyBBX never embeds a modem.

| Layer | Where |
|-------|--------|
| Session core | Main HyBBX |
| HBX circuit hub | Main `[circuit]` |
| **`crdop` plugin** | Secondary HyBBX (typical) |
| **CRDOPC** | External process (operator starts first) |
| RF / 11 m CB | External modem + radio |

---

## Topology

```
  Secondary (remote)                         Main
  ┌─────────────────────┐                   ┌──────────────────────────┐
  │ [transport.crdopN]  │  HBX/TCP :7323    │ [circuit] hub            │
  │  crdop plugin       │ ────────────────► │  telnet :2323 (users)    │
  │       │             │  LINK_AUTH        │  storage, mail, chat     │
  │       │ TCP N/N+1   │                   │ [transport.crdopN]       │
  │       ▼             │                   │  bridge registry         │
  │  CRDOPC (external)  │                   └──────────────────────────┘
  └─────────────────────┘
         │
         ▼ RF (CB)
      Radio
```

CRDOP and ARDOP can coexist on one Secondary only with **separate modem processes** and **different TCP ports** (separate `link_id`s on Main).

---

## External modem (CRDOPC)

CRDOPC is a **standalone modem program** (not in the HyBBX tree). It speaks ARDOP-compatible host TCP so HyBBX needs no modem source inside the repo.

Start CRDOPC **before** HyBBX on the configured control port:

```bash
crdopc 8515 127.0.0.1
# control 8515, data 8516
```

Default plugin target: `127.0.0.1:8515`.

### VERSION strings

CRDOPC may report e.g. `VERSION crdopc_0.1.0-l2-cb`. HyBBX **does not parse or whitelist** version tokens — it acks with **`RDY`** and continues. No HyBBX change is required when CRDOPC revs its version string.

---

## INI configuration

### `[networks]`

```ini
[networks]
crdop = yes
circuit = no    ; Secondary
```

| Key | Default | Effect |
|-----|---------|--------|
| `crdop` | `no` | Enable `crdop` plugin sections |

Sections: `[transport.crdop]` or `[transport.crdopN]`.

### Secondary — `[transport.crdopN]`

```ini
[transport.crdop1]
enabled = yes
modem_host = 127.0.0.1
modem_port = 8515
mycall = CALL-0
arq_bandwidth = 500MAX
listen = yes
circuit_host = main.example.com
circuit_port = 7323
link_id = secondary-1-crdop
link_password = changeme
link_role = link
frequency_mhz = 27.205
```

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | — | `yes` to start |
| `modem_host` | `127.0.0.1` | Alias: `ardop_host` |
| `modem_port` | `8515` | Alias: `ardop_port`; data = port + 1 |
| `mycall` | `NOCALL` | `MYCALL` to modem |
| `arq_bandwidth` | `500MAX` | CB default; warns if above 1000MAX class |
| `listen` | `yes` | Listen for RF connects |
| `peer_call` | — | Optional `ARQCALL` |
| `circuit_host` | `127.0.0.1` | Main HBX host |
| `circuit_port` | `7323` | Main HBX port |
| `link_id` | `crdop-link` | Matches Main registry |
| `link_password` | — | HBX `LINK_AUTH` |
| `link_role` | `link` | Bridge metadata |
| `frequency_mhz` | — | CB MHz (e.g. 27.205) |

**Profile:** `radio_profile` is **always CB** in the `crdop` plugin (not configurable to amateur).

### Main — bridge registry

```ini
[transport.crdop1]
link_id = secondary-1-crdop
link_password = changeme
link_role = link
frequency_mhz = 27.205
```

Templates: `share/hybbx-secondary.ini.example`, `share/hybbx.ini.example`.

---

## Host interface

Identical wire model to ARDOP host TCP — see [ARDOP.md](ARDOP.md#host-interface-hybbx-subset) for command/data tables and CRC **0x8810**.

HyBBX `crdop` uses the same `ardop_host.c` implementation via `hybbx_ardop_common`. Log tag is **`[crdop]`** (not `[ardop]`).

---

## HBX bridge

- Payload path: CRDOPC `d:ARQ` ↔ HBX `terminal` on circuit TCP.
- QoS: half-duplex / low bandwidth class on `LINK_AUTH` (CB-oriented).
- Flow control: Main `FLOW_CTRL` pause/cancel respected in `ardop_link.c`.

---

## Source tree (developer)

```
plugins/crdop/
  crdop.c           Plugin wrapper (HYBBX_TRANSPORT_CRDOP)
  crdop_config.c    hybbx_crdop_config_parse() — CB defaults
  CMakeLists.txt    hybbx_plugin_crdop → links hybbx_ardop_common

plugins/ardop/      Shared host TCP stack (not duplicated)
  ardop_link.c/h
  ardop_host.c/h
  ardop_crc.c
  ardop_config.c    Also used by ardop plugin

include/hybbx/
  crdop.h           CRDOP parse API, profile helpers
  ardop.h           hybbx_ardop_config_t wire struct

src/core/
  networks.c        [networks] crdop
  circuit_bridge.c  [transport.crdopN]
  crdop.c           hybbx_crdop_profile_* helpers

src/main.c          hybbx_plugin_crdop registration
```

**CMake:** `HYBBX_PLUGIN_CRDOP=ON` (default). `add_subdirectory(ardop)` runs when ARDOP **or** CRDOP is enabled (common library).

**Transport kind:** `HYBBX_TRANSPORT_CRDOP` (= 5).

---

## Build and test

```bash
cmake -B build -DHYBBX_PLUGIN_CRDOP=ON
cmake --build build
./scripts/test-crdop-plugin.sh    # mock host TCP, no RF
```

CRDOP-only build: `-DHYBBX_PLUGIN_ARDOP=OFF -DHYBBX_PLUGIN_CRDOP=ON`.

**Pre–v1.0.0:** mock smoke only. **Post–v1.0.0:** AX.25 RF tests first; CRDOP live RF after ([ROADMAP.md](ROADMAP.md#verification)).

---

## CRDOP vs ARDOP (HyBBX view)

| | ARDOP | CRDOP |
|---|--------|--------|
| Plugin | `ardop` | `crdop` |
| External modem | ARDOPC, ardopcf | CRDOPC |
| INI section | `[transport.ardopN]` | `[transport.crdopN]` |
| Profile | amateur (default); optional `radio_profile=cb` on `ardop` | CB fixed |
| Default `link_id` | `ardop-link` | `crdop-link` |
| Typical bands | Amateur HF/VHF | 11 m CB |

Both share **`hybbx_ardop_common`** — changes to host TCP affect both plugins.

---

## Limits and future work

| Topic | Notes |
|-------|--------|
| Level 2 experimental | Protocol elements still evolving in CRDOPC |
| Singleton link layer | One active `ardop_link` context per process — separate ports/processes for dual bridges |
| Embedded modem | **Out of scope** for HyBBX |
| Global CB profiles INI | Partial — plugin forces CB; broader profile registry post–v1.0.0 |

---

## Licensing

HyBBX `crdop` plugin: **GPL-3.0**. CRDOPC is a **separate binary** (MIT or GPL lineage depending on upstream) — [LICENSING.md](LICENSING.md).

---

## Related HyBBX docs

| Doc | Use |
|-----|-----|
| [ARDOP.md](ARDOP.md) | Amateur ARDOP (`ardop` + ARDOPC/ardopcf) |
| [MANUAL.md](MANUAL.md) | Operator INI and commands |
| [FEATURES.md](FEATURES.md) | Shipped vs planned |
| [ROADMAP.md](ROADMAP.md) | Level 2 roadmap, verification |

*Last aligned with HyBBX 0.8.x.*
