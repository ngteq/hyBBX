# tinysha256 (bundled)

Public-domain SHA-256 for HyBBX password hashing.

- `tinysha256.c` / `tinysha256.h` — compiled into `hybbx_core`
- Adapted from the [983/SHA-256](https://github.com/983/SHA-256) implementation (The Unlicense)

HyBBX stores passwords as `{sha256}` + 64 lowercase hex digits.
