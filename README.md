# HyBBX

**HyBBX** is a plugin-based **service solution for different connection types** — telnet, amateur **packet radio**, and future links. It is **based on and inspired by** classic BBS and mailbox culture (sessions, MOTD, chat, slow-link terminal UX). HyBBX is **not** a new BBS or mailbox product; it is a transport-oriented platform that carries that spirit across modern and radio links.

Plain **C99**, **INI** configuration, **40 columns** and **2400 baud** pacing for slow links.

Builds with **GCC** and **LLVM/Clang** on **Linux, BSD, macOS 10+, Windows 10+, AmigaOS 3.9+**, and other POSIX systems ([docs/PLATFORMS.md](docs/PLATFORMS.md)).

**Version 0.1.0** (early development)

## At a glance

- **Telnet** over TCP/IPv4 and IPv6 (port **2323**)
- **Packet radio** — TNC2C, BayCom, PC-COM; KISS, host mode, 6PACK; AX.25 over an internal HBX/TCP circuit
- **BBS-inspired session core** — guests, registration, multi-channel chat, MOTD/news, staff commands
- **Flat-file storage** with INI user shards; bundled crypto (optional OpenSSL/libsodium)
- **Standalone CLI clients** — `hybbx-telnet` and `hybbx-terminal` (parameters/env only; no INI) build without the server ([docs/CLIENTS.md](docs/CLIENTS.md))

For scope, status, and the **1× central / N× gateway–digipeater–repeater–link** layout see **[docs/FEATURES.md](docs/FEATURES.md)** and **[docs/ROADMAP.md](docs/ROADMAP.md)**.

## Try it

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
./build/src/clients/hybbx-telnet
```

More detail: **[docs/QUICKSTART.md](docs/QUICKSTART.md)**

## Documentation

| Document | Purpose |
|----------|---------|
| **[docs/FEATURES.md](docs/FEATURES.md)** | All functional features (read this for scope & status) |
| **[docs/QUICKSTART.md](docs/QUICKSTART.md)** | Build, install, first session |
| **[docs/MANUAL.md](docs/MANUAL.md)** | Full operator & developer reference |
| **[docs/INDEX.md](docs/INDEX.md)** | Documentation index |
| **[docs/ROADMAP.md](docs/ROADMAP.md)** | **1× central**; gateway, digipeater, repeater, link |
| **[docs/PLATFORMS.md](docs/PLATFORMS.md)** | GCC/Clang targets: Win10+, macOS, AmigaOS 3.9+, Linux/BSD |
| **[docs/CLIENTS.md](docs/CLIENTS.md)** | Standalone `hybbx-telnet` / `hybbx-terminal` builds |
| **[CONTRIBUTING.md](CONTRIBUTING.md)** | How to contribute (humans) |
| **[AGENTS.md](AGENTS.md)** | Agent & developer quick guide (AI) |

Example config: `share/hybbx.ini.example`

## Contributing

We welcome patches and docs improvements. See **[CONTRIBUTING.md](CONTRIBUTING.md)** and **[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)**. AI tools: start with **[AGENTS.md](AGENTS.md)**.

## License

**GPL-3.0** — see [LICENSE.txt](LICENSE.txt).
