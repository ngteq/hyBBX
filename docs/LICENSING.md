# Licensing

**v2.0.0** · not legal advice.

## HyBBX

| Item | License |
|------|---------|
| Core, plugins, clients, docs | **GPL-3.0** — [LICENSE.txt](../LICENSE.txt) (copyright: HyBBX contributors) |

## Bundled in HyBBX binary

| Component | Path | License |
|-----------|------|---------|
| Monocypher | `third_party/monocypher/` | BSD-2 or CC0 |
| tiny-AES-c | `third_party/tinyaes/` | Public domain |
| tinysha256 | `third_party/tinysha256/` | Unlicense |

Optional link-time: OpenSSL, libsodium — their licenses if enabled.

## External modems (not shipped with HyBBX)

HyBBX is **plugin-only** — no ARDOPC/CRDOPC/TNC DSP in tree.

| Program | Typical license | HyBBX plugin |
|---------|-----------------|--------------|
| ARDOPC (g8bpq) | GPL-3.0 (packaging) | `ardop` — GPL-3.0 |
| ardopcf (pflarue) | MIT | `ardop` — GPL-3.0 |
| CRDOPC (external) | MIT | `crdop` — GPL-3.0 |

Plugins talk to external processes over TCP — no DSP linking.

## Distributor checklist

1. Ship [LICENSE.txt](../LICENSE.txt)
2. Include Monocypher / tiny-AES-c / tinysha256 notices
3. If bundling ARDOPC or CRDOPC separately, ship their LICENSE/NOTICE

Details: [ARDOP.md](ARDOP.md), [CRDOP.md](CRDOP.md).
