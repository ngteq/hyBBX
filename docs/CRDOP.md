# CRDOP plugin

**Status:** Built — **not live RF verified**. Standalone sources: `plugins/crdop/`. External modem: [CRDOPC](https://github.com/ngteq/CRDOP) (MIT, not in HyBBX tree).

CB-oriented digital bridge. Separate protocol from ARDOP — [ARDOP.md](ARDOP.md).

## Model

| Layer | Component |
|-------|-----------|
| HyBBX | `crdop` plugin → HBX `terminal` |
| Modem | **CRDOPC** (build from ngteq/CRDOP) |
| Wire | ARDOP-compatible host TCP — control **N**, data **N+1** |

Plugin applies CB defaults (`500MAX`, half-duplex QoS, `crdop-link`).

## CRDOPC (external)

```bash
git clone https://github.com/ngteq/CRDOP.git
cd CRDOP && ./scripts/build-crdop.sh
./scripts/crdopc    # default control 8515
```

Start CRDOPC **before** HyBBX.

## INI (Secondary typical)

```ini
[networks]
crdop = yes

[transport.crdop1]
enabled = yes
modem_host = 127.0.0.1
modem_port = 8515
mycall = CB-0
listen = yes
circuit_host = main.example.com
circuit_port = 7323
link_id = secondary-crdop
link_password = secret
```

Main: matching `[transport.crdop1]` bridge entry.

## Source tree

```
plugins/crdop/
  crdop.c, crdop_config.c    Plugin + CB INI parse
include/hybbx/crdop.h
```

Links `hybbx_ardop_common` for host TCP today — temporary; stacks diverge later.

**CMake:** `HYBBX_PLUGIN_CRDOP=ON`. CRDOP-only: `-DHYBBX_PLUGIN_ARDOP=OFF`. Kind: `HYBBX_TRANSPORT_CRDOP` (5).

## Local test

```bash
./scripts/test-crdop-plugin.sh
```

Mock host TCP only — not RF.

## Licensing

Plugin: GPL-3.0. CRDOPC: MIT — [LICENSING.md](LICENSING.md).

## See also

[ARDOP.md](ARDOP.md) · [FEATURES.md](FEATURES.md) · [ROADMAP.md](ROADMAP.md)
