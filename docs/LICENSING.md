# Licensing · HyBBX 2.4.0

HyBBX and shipped components — license reference.

## License matrix

| Component | License |
|-----------|---------|
| HyBBX core + plugins | GPL-3.0 — [LICENSE.txt](../LICENSE.txt) |
| Bundled crypto (tinysha256, tinyaes, monocypher) | respective upstream licenses |
| Optional OpenSSL / libsodium | upstream licenses when enabled |

## Distribution matrix

| Item | Rule |
|------|------|
| Source offer | GPL-3.0 compliance required for derivatives |
| Operator config | `./local/` — not part of distribution |
| Third-party deps | Document in build output / package metadata |

## Related

| Goal | Doc |
|------|-----|
| Build options | [BUILD.md](BUILD.md) |
| Root README | [../README.md](../README.md) |
