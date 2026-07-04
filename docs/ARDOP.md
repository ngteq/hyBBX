# ARDOP — HyBBX host-client plugin

**Status:** Partial (0.8.x) — host-TCP bridge implemented; live RF integration tests planned after v1.0.0.

Standalone reference for the **`ardop`** transport plugin, external ARDOP modems, INI, wire protocol subset, and developer layout. For CB digital use **CRDOP** instead — see [CRDOP.md](CRDOP.md).

---

## What this is

**ARDOP** (Amateur Radio Digital Open Protocol) digital links use an **external modem program** (ARDOPC or ardopcf) for sound-card / serial RF. HyBBX provides only a **host-client bridge plugin** that:

1. Speaks the ARDOP **TNC Host Interface** over TCP (control port **N**, data port **N+1**).
2. Connects as an HBX **Secondary** client to Main (`circuit_host` / `[circuit]` hub).
3. Bridges RF payload bytes: modem `d:ARQ` ↔ HBX `terminal` protocol.

HyBBX **never** runs modem DSP, FEC/OFDM, PTC, CAT, or audio capture. That stays in ARDOPC/ardopcf.

| Layer | Where |
|-------|--------|
| Session core (mail, chat, `/` commands) | Main HyBBX |
| HBX circuit hub | Main `[circuit]` |
| **`ardop` plugin** | Secondary HyBBX (typical) |
| **ARDOPC / ardopcf** | External process (operator starts first) |
| RF / sound card | External modem + radio |

---

## Topology

```
  Secondary (remote)                         Main
  ┌─────────────────────┐                   ┌──────────────────────────┐
  │ [transport.ardopN]  │  HBX/TCP :7323    │ [circuit] hub            │
  │  ardop plugin       │ ────────────────► │  telnet :2323 (users)    │
  │       │             │  LINK_AUTH        │  storage, mail, chat     │
  │       │ TCP N/N+1   │                   │ [transport.ardopN]       │
  │       ▼             │                   │  bridge registry         │
  │  ARDOPC / ardopcf   │                   └──────────────────────────┘
  │  (external modem)   │
  └─────────────────────┘
         │
         ▼ RF
      Radio
```

Parallel **AX.25** on the same Secondary uses a separate `link_id` and `[transport.packet_radioN]` on Main.

---

## External modem

Operator must start the modem **before** HyBBX connects.

| Program | Upstream | License (external) | Host TCP |
|---------|----------|----------------------|----------|
| **ARDOPC** | [g8bpq/ardop](https://github.com/g8bpq/ardop) | GPL-3.0 (packager metadata) | control **8515**, data **8516** (typical) |
| **ardopcf** | [pflarue/ardop](https://github.com/pflarue/ardop) | MIT | same model |

Example (control 8515, bind loopback):

```bash
ardopc TCPIP 8515 127.0.0.1
# or ardopcf equivalent for your build
```

Default plugin target: `127.0.0.1:8515` (data **8516**).

### VERSION strings

HyBBX **does not whitelist** modem `VERSION` lines. Any ARDOP-compatible host string (e.g. from ardopcf or CRDOPC if misconfigured on the `ardop` plugin) is acked with **`RDY`** and the session continues. Use the **`crdop`** plugin with CRDOPC for CB — see [CRDOP.md](CRDOP.md).

---

## INI configuration

### `[networks]`

```ini
[networks]
ardop = yes
circuit = no    ; Secondary: outbound client to Main
```

| Key | Default (Secondary template) | Effect |
|-----|------------------------------|--------|
| `ardop` | `no` | Enable `ardop` plugin sections |

Section names: `[transport.ardop]` or `[transport.ardopN]` (numbered for multi-bridge).

### Secondary — `[transport.ardopN]`

```ini
[transport.ardop1]
enabled = yes
ardop_host = 127.0.0.1
ardop_port = 8515
mycall = CALL-0
arq_bandwidth = 500MAX
radio_profile = amateur
listen = yes
; peer_call = REMOTE-0
circuit_host = main.example.com
circuit_port = 7323
link_id = secondary-1-ardop
link_password = changeme
link_role = link
frequency_mhz = 7.100
```

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | — | `yes` to start this section |
| `ardop_host` | `127.0.0.1` | ARDOPC control host |
| `ardop_port` | `8515` | Control port (data = port + 1) |
| `mycall` | `NOCALL` | Sent as `MYCALL` to modem |
| `arq_bandwidth` | profile-based | e.g. `500MAX` → `ARQBW` |
| `radio_profile` | `amateur` | `amateur` or `cb` (CB preview on `ardop`; prefer `crdop` for CB) |
| `listen` | `yes` | `LISTEN TRUE` / `FALSE` |
| `peer_call` | — | Optional outbound `ARQCALL` |
| `circuit_host` | `127.0.0.1` | Main HBX hub host |
| `circuit_port` | `7323` | Main HBX hub port |
| `link_id` | `ardop-link` | Must match Main bridge registry |
| `link_password` | — | HBX `LINK_AUTH` |
| `link_role` | `link` | Bridge metadata (`link`, `gateway`, …) |
| `frequency_mhz` | — | MHz for QoS / broadcast (not channel numbers) |

### Main — bridge registry

```ini
[transport.ardop1]
link_id = secondary-1-ardop
link_password = changeme
link_role = link
frequency_mhz = 7.100
```

Main section **matches** Secondary `link_id` / password; no modem keys on Main.

Templates: `share/hybbx-secondary.ini.example`, `share/hybbx.ini.example`.

---

## Host interface (HyBBX subset)

CRC-16 polynomial **0x8810** on control and data frames. Implementation: `plugins/ardop/ardop_crc.c`.

### Host → modem (control port)

| Command | Purpose |
|---------|---------|
| `INITIALIZE` | Session start |
| `MYCALL` | Local callsign |
| `PROTOCOLMODE ARQ` | ARQ mode |
| `ARQBW` | Bandwidth class |
| `LISTEN TRUE` / `FALSE` | Listen for connects |
| `ARQCALL` | Outbound connect (optional) |
| `DISCONNECT` | Drop RF link |
| `RDY` | Ack to modem status |

### Modem → host

| Message | HyBBX action |
|---------|--------------|
| `RDY` | Mark TNC ready; complete init if pending |
| `CONNECTED` / `DISCONNECTED` | RF link up/down events |
| `FAULT`, `CRCFAULT` | Log; ack `RDY` |
| `BUSY`, `BUFFER`, `STATUS`, … | Ack `RDY` |
| **`VERSION …`** | Ack `RDY` (not validated) |
| `d:ARQ` data frames | Payload → HBX `terminal` uplink |

### Data port

Binary frames: `D:` host TX, `d:ARQ` modem RX (see `ardop_host.c`).

**Not in HyBBX:** FEC/OFDM modes, PTC, radio CAT, sound-card I/O.

---

## HBX bridge

- Uplink: modem RX → `HYBBX_CIRCUIT_PROTO_TERMINAL` on circuit TCP.
- Downlink: circuit `terminal` → modem `D:` when RF link up.
- Auth: `LINK_AUTH` with `link_id`, `link_password`, QoS metadata.
- Flow control: `FLOW_CTRL` from Main can pause/cancel uplink (`ardop_link.c`).

---

## Source tree (developer)

```
plugins/ardop/
  ardop.c           Plugin wrapper (HYBBX_TRANSPORT_ARDOP)
  ardop_config.c    hybbx_ardop_config_parse()
  ardop_link.c/h    Shared link layer (poll thread, circuit, reconnect)
  ardop_host.c/h    TCP host client (control + data)
  ardop_crc.c       CRC-16
  CMakeLists.txt    hybbx_ardop_common + hybbx_plugin_ardop

include/hybbx/
  ardop.h           Config struct, parse/free API
  crdop.h           Profile helpers (shared with crdop plugin)

src/core/
  networks.c        [networks] ardop switch
  circuit_bridge.c  [transport.ardopN] on Main
  crdop.c           radio_profile parse helpers

src/main.c          hybbx_plugin_ardop registration
```

**Shared library:** `hybbx_ardop_common` (link + host + crc + config) is also linked by the **`crdop`** plugin. See [CRDOP.md](CRDOP.md) for the CB wrapper.

**CMake:** `HYBBX_PLUGIN_ARDOP=ON` (default). Builds common lib when `HYBBX_PLUGIN_ARDOP` or `HYBBX_PLUGIN_CRDOP` is on.

**Transport kind:** `HYBBX_TRANSPORT_ARDOP` (= 4) in `include/hybbx/types.h`.

---

## Build and test

```bash
cmake -B build -DHYBBX_PLUGIN_ARDOP=ON
cmake --build build
./scripts/test-ardop-plugin.sh    # mock ARDOPC, no RF
```

Smoke test uses `scripts/mock-ardopc.py` on a ephemeral port under `local/test-ardop-*/`.

**Pre–v1.0.0:** no live-modem CI. **Post–v1.0.0:** AX.25 RF tests first; ARDOP live RF later ([ROADMAP.md](ROADMAP.md#verification)).

---

## Limits and future work

| Topic | Notes |
|-------|--------|
| One plugin instance | `ardop_link.c` uses static singleton state — do not run two `ardop` sections in one process without refactor |
| `ardop` + `crdop` together | Use **separate TCP ports** and separate modem processes |
| FEC / OFDM / CAT | External modem only |
| Live RF verification | After v1.0.0 |

---

## Licensing

HyBBX plugin code: **GPL-3.0** ([LICENSE.txt](../LICENSE.txt)). ARDOPC/ardopcf are **separate programs** — see [LICENSING.md](LICENSING.md).

References:

- ARDOPC operator notes: [cantab.net ARDOPC](https://www.cantab.net/users/john.wiseman/Documents/ARDOPC.html)
- ardopcf: [pflarue/ardop](https://github.com/pflarue/ardop)

---

## Related HyBBX docs

| Doc | Use |
|-----|-----|
| [CRDOP.md](CRDOP.md) | CB digital (`crdop` plugin + CRDOPC) |
| [MANUAL.md](MANUAL.md) | Full operator INI and commands |
| [FEATURES.md](FEATURES.md) | Shipped vs planned |
| [ROADMAP.md](ROADMAP.md) | Release and test plan |

*Last aligned with HyBBX 0.8.x.*
