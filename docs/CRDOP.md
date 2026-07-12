# CRDOP plugin

**v2.0.0** · sources: `plugins/crdop/`. External modem: **CRDOPC** (not in HyBBX tree). Verify RF on site after deploy.

CB-oriented digital bridge. Separate protocol from ARDOP — [ARDOP.md](ARDOP.md).

## Model

| Layer | Component |
|-------|-----------|
| HyBBX | `crdop` plugin → HBX `terminal` |
| Modem | **CRDOPC** (external build) |
| Wire | ARDOP-compatible host TCP — control **N**, data **N+1** |

Plugin applies CB defaults (`500MAX`, half-duplex QoS, `crdop-link`).

**RF prep:** CRDOPC build and lifecycle live in **MainAX25-Stack (MAX25)** `hardware/soft-modems` — not in HyBBX. Start MAX25 (or CRDOPC) before enabling this plugin.

## CRDOPC (external)

Build and install CRDOPC via MAX25 soft-modem prep (or upstream CRDOP), then start it **before** HyBBX (default control port **8515**).

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

Host TCP via shared `hybbx_ardop_common` helpers.

**CMake:** `HYBBX_PLUGIN_CRDOP=ON`. CRDOP-only: `-DHYBBX_PLUGIN_ARDOP=OFF`. Kind: `HYBBX_TRANSPORT_CRDOP` (5).

## Local test

```bash
./scripts/test-crdop-plugin.sh
```

Mock host TCP only — not RF.

## Licensing

Plugin: GPL-3.0. CRDOPC: MIT — [LICENSING.md](LICENSING.md).

## See also

[ARDOP.md](ARDOP.md) · [MANUAL.md](MANUAL.md) · [BUILD.md](BUILD.md)
