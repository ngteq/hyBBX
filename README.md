# HyBBX

**HyBBX** is a plugin-based **service solution for different connection types** — telnet, amateur **packet radio**, and future links. It is **based on and inspired by** classic BBS and mailbox culture (sessions, MOTD, chat, slow-link terminal UX). HyBBX is **not** a new BBS or mailbox product; it is a transport-oriented platform that carries that spirit across modern and radio links.

Plain **C99**, **INI** configuration, **40 columns** and **2400 baud** pacing for slow links.

**Version 0.1.0** (early development)

## At a glance

- **Telnet** over TCP/IPv4 and IPv6 (port **2323**)
- **Packet radio** — TNC2C, BayCom, PC-COM; KISS, host mode, 6PACK; AX.25 over an internal HBX/TCP circuit
- **BBS-inspired session core** — guests, registration, multi-channel chat, MOTD/news, staff commands
- **Flat-file storage** with INI user shards; bundled crypto (optional OpenSSL/libsodium)

For the complete, maintained capability list see **[docs/FEATURES.md](docs/FEATURES.md)**.

## Try it

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

More detail: **[docs/QUICKSTART.md](docs/QUICKSTART.md)**

## Documentation

| Document | Purpose |
|----------|---------|
| **[docs/FEATURES.md](docs/FEATURES.md)** | All functional features (read this for scope & status) |
| **[docs/QUICKSTART.md](docs/QUICKSTART.md)** | Build, install, first session |
| **[docs/MANUAL.md](docs/MANUAL.md)** | Full operator & developer reference |
| **[docs/INDEX.md](docs/INDEX.md)** | Documentation index |

Example config: `share/hybbx.ini.example`

## License

**GPL-3.0** — see [LICENSE.txt](LICENSE.txt).
