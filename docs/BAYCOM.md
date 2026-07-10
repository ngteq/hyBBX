# BayCom PR-Stack — baycom plugin

**v2.0.0** · INI: [MANUAL.md](MANUAL.md) · transport: `baycom`

**Opt-in:** not built or started by default. Enable at compile time (`-DHYBBX_PLUGIN_BAYCOM=ON`) and in INI (`[networks] baycom=yes`).

HyBBX `baycom` implements the Linux equivalent of the DOS BayCom PR-Stack **L2** path: HDLC + AX.25 UI frames on BayCom modems, bridged to Main over HBX.

For async serial TNCs (TNC2C, PK-TNC2, PK-232, …) use the [`packet_radio`](TNCS.md) plugin instead.

## Backends

| `backend` | Hardware | Notes |
|-----------|----------|-------|
| `kernel` (default) | SER12, PAR96, EPP | Thomas Sailer `baycom_*` kernel drivers + hdlcdrv |
| `kiss` | BayCom firmware on serial | 8N1 KISS @ `serial_baud` (1200 typical) |

**SER12 is not async serial.** The UART is bit-banged by the kernel driver (`8250`/`16450`/`16550` class ports). USB adapters are not supported on the native path.

## Kernel modem modes (`mode=`)

| `mode` | Driver module | Default netdev |
|--------|---------------|----------------|
| `ser12` | `baycom_ser_fdx` | `bcsf0` |
| `ser12*` | `baycom_ser_fdx` | `bcsf0` — software DCD (recommended) |
| `ser12+` | `baycom_ser_fdx` | `bcsf0` — inverted hardware DCD |
| `ser12hdx` | `baycom_ser_hdx` | `bcsh0` |
| `par96` | `baycom_par` | `bcp0` |
| `par96*` | `baycom_par` | `bcp0` |
| `epp` / `par97` | `baycom_epp` | `bce0` |

Load the module before start, or set `kernel_autoload = yes` (requires root/CAP_SYS_MODULE):

```bash
sudo modprobe baycom_ser_fdx mode=ser12* iobase=0x3f8 irq=4 baud=1200
```

HyBBX then configures channel access (`txdelay`, `slot`, `persist`, …) via hdlcdrv ioctl and exchanges AX.25 frames on the netdev (`AF_PACKET` / `ETH_P_AX25`).

## Secondary example (SER12)

```ini
[networks]
baycom = yes
ax25 = no

[transport.baycom1]
backend = kernel
mode = ser12*
interface = bcsf0
iobase = 0x3f8
irq = 4
radio_baud = 1200
txdelay = 30
slot = 10
persist = 128
txtail = 1
circuit_host = main.example.com
circuit_port = 7323
link_id = baycom-1
link_password = changeme
frequency_mhz = 27.205
```

## KISS serial example

```ini
[transport.baycom1]
backend = kiss
device = /dev/ttyS0
serial_baud = 1200
txdelay = 30
slot = 10
persist = 128
circuit_host = main.example.com
link_id = baycom-kiss
link_password = changeme
```

## Multi-instance

Up to **4** `[transport.baycomN]` sections per Secondary (`HYBBX_BAYCOM_MAX_INSTANCES`). Each needs a unique `link_id` and RF interface/device.

## Build

```bash
cmake -B build -DHYBBX_PLUGIN_BAYCOM=ON
cmake --build build
```

Transport kind: `HYBBX_TRANSPORT_BAYCOM` (8). Default install omits this plugin.

## Requirements

- Linux with in-tree `baycom_*` + AX.25 netdev support (kernel `hdlcdrv` path).
- Root or capabilities for `modprobe`, `SETMODEMPAR` / `SETMODE`, and `IFF_UP` when the netdev is not already configured.
- Optional: `sethdlc` from ax25-tools for manual setup/debug.
