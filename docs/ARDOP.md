# ARDOP plugin

**Status:** Built in v1.0.0 — **not live RF verified**. Standalone sources: `plugins/ardop/`. External modem: ARDOPC or ardopcf (not in HyBBX tree).

Amateur-band digital bridge. For CB use **`crdop`** + CRDOPC — [CRDOP.md](CRDOP.md).

## Model

| Layer | Component |
|-------|-----------|
| HyBBX | `ardop` plugin → HBX `terminal` on circuit |
| Modem | **ARDOPC** / **ardopcf** (operator starts separately) |
| Wire | ARDOP host TCP — control port **N**, data **N+1** |

## INI (Secondary typical)

```ini
[networks]
ardop = yes

[transport.ardop1]
enabled = yes
ardop_host = 127.0.0.1
ardop_port = 8515
mycall = CALL-0
arq_bandwidth = 1000MAX
listen = yes
circuit_host = main.example.com
circuit_port = 7323
link_id = secondary-ardop
link_password = secret
link_role = link
```

Main bridge registry: same `link_id` / password in `[transport.ardop1]` (metadata only).

Keys: [MANUAL.md](MANUAL.md#transportpacket_radion-bridge-registry).

## Source tree

```
plugins/ardop/
  ardop.c, ardop_config.c    Plugin entry + INI parse
  ardop_link.c, ardop_host.c Host TCP + HBX client
  ardop_crc.c                CRC-16 (0x8810)
```

`hybbx_ardop_common` may also link from `plugins/crdop/` today — protocols diverge later.

**CMake:** `HYBBX_PLUGIN_ARDOP=ON`. Transport kind: `HYBBX_TRANSPORT_ARDOP` (4).

## Local test

```bash
cmake --build build
./scripts/test-ardop-plugin.sh
```

Uses `scripts/mock-ardopc.py` — host TCP only, not RF.

## Licensing

Plugin: GPL-3.0. ARDOPC licensing varies by upstream — [LICENSING.md](LICENSING.md).

## See also

[CRDOP.md](CRDOP.md) · [FEATURES.md](FEATURES.md) · [ROADMAP.md](ROADMAP.md)
