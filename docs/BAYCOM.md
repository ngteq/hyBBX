# BayCom/based · HyBBX

Public mark: **BayCom/based**. Never **Konverter** / **converter**.

Optional **transport plugin** (`plugins/baycom/`) for non-UN1TME sites. **SER12 L1 (HDLC + UART modem-control) lives in MAX25** (`bcpr` → device **`max25e0`**) — not inside HyBBX `packet_radio`.

| Item | Value |
|------|-------|
| Build | `HYBBX_PLUGIN_BAYCOM=ON` (default in CMake) |
| Example INI | `[networks] baycom=no` in `share/hybbx-*.ini.example` |
| Enable (optional sites) | After MAX25 bcpr bring-up: `[networks] baycom=yes` + `[transport.baycom]` **or** prefer `[max25]` attach to max25d |
| Preferred path | HyBBX **attach-only** → max25d → **`max25e0`** (BayCom/based via bcpr) |
| **UN1TME** | **TNCs only** — no BayCom/based / PC-COM RF on that station |

`packet_radio` rejects `tnc=baycom` / `tnc=pccom` (not TNC profiles). See `share/hybbx/baycom-ser12-host.ini.example` only when enabling the optional plugin on a non-UN1TME site.

## Related

| Goal | Doc |
|------|-----|
| TNC serial (`packet_radio`) | [TNCS.md](TNCS.md) |
| Manual | [MANUAL.md](MANUAL.md) |
| MAX25 BayCom/based | MAX25-Stack `docs/BAYCOM.md` · device `max25e0` |
