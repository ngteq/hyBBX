# CRDOP — CB host-client plugin (HyBBX)

**Status:** Experimental Level 2 — **HyBBX plugin only** (no separate CRDOP repo required).

The **`crdop`** transport is the CB-oriented host-client bridge. It uses the same
ARDOP-compatible TCP host interface as **`ardop`**, with **CB defaults** (`500MAX`,
half-duplex QoS, `crdop-link` id). RF/modem DSP stays in **external ARDOPC or ardopcf**
— HyBBX never embeds a sound-modem.

## Plugins compared

| Plugin | INI | Profile | Use |
|--------|-----|---------|-----|
| `ardop` | `[transport.ardopN]` | amateur (default) | General ARDOP bridge |
| `crdop` | `[transport.crdopN]` | **CB** (fixed) | 11 m / CB digital bridge |

Both can run on one Secondary (parallel `link_id`s) with one external modem process
each, or share one ARDOPC if ports differ.

## Secondary INI

```ini
[networks]
crdop = yes

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
frequency_mhz = 27.205
```

Keys `modem_host` / `modem_port` are aliases for `ardop_host` / `ardop_port`.

## Main bridge

```ini
[transport.crdop1]
link_id = secondary-1-crdop
link_password = changeme
link_role = link
frequency_mhz = 27.205
```

## External modem

Start ARDOPC or ardopcf before HyBBX, e.g.:

```bash
ardopc 8515 127.0.0.1
```

**Testing:** mock host-TCP smoke only before v1.0.0; live CB/modem RF verification after v1.0.0 (after AX.25 integration tests). See [ROADMAP.md](ROADMAP.md#verification).

## Docs

- [MANUAL.md](MANUAL.md) — operator reference
- [LICENSING.md](LICENSING.md) — GPL HyBBX + external modem licenses
