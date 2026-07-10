# HyBBX

C99 **session daemon** for low-bandwidth links: local mail, chat, conference, and `/` commands over telnet, SSH, or WebSocket.

**v1.5.0** (testing) — [docs/RELEASE-1.5.0.md](docs/RELEASE-1.5.0.md) · [docs/TOPOLOGY.md](docs/TOPOLOGY.md)

## Live (hybbx.un1t.me)

| Access | How |
|--------|-----|
| **Browser** | [https://hybbx.un1t.me/](https://hybbx.un1t.me/) |
| **Telnet** | `telnet hybbx.un1t.me 2323` — guest auto-login |
| **SSH** | `ssh hybbx.un1t.me -p 3232` — wire auth only; `/login` for HyBBX accounts |

## Architecture

HyBBX splits **user sessions** from **RF / mesh infrastructure**.

```
                    ┌─────────────────────────────────────┐
  Users             │  Main (hybbx)                       │
  telnet :2323      │  storage · mail · chat · commands   │
  SSH :3232    ───► │  HBX hub :7323                      │
  WebSocket         └──────────────▲──────────────────────┘
                                   │ HBX v1 + LINK_AUTH
                    ┌──────────────┴──────────────────────┐
                    │  Secondary (hybbx, remote site)     │
                    │  packet_radio · ardop · crdop · …   │
                    └─────────────────────────────────────┘

  Optional (stub):  Main-A ◄── HBX circuit ──► Main-B   (mains_proxy)
```

### Main

Central site: registered users, flat-file or SQLite storage, telnet/SSH/WebSocket listeners, internal **HBX circuit hub** on `:7323`. Sysop mail, chat channels, `/broadcast` to all online users on this Main. Template: [share/hybbx.ini.example](share/hybbx.ini.example).

### Secondary

Remote **edge** only — no public user logins. Runs `hybbx` with `circuit=no`, connects to Main as an HBX client (`circuit_host`, `link_id`, `link_password`). Hosts packet radio, ARDOP, CRDOP, or BayCom plugins; modem/TNC stays **external**. Template: [share/hybbx-secondary.ini.example](share/hybbx-secondary.ini.example).

Multiple Secondaries on one Main need unique `link_id` values (up to 16 links).

### mains_proxy (Main-to-Main mesh)

**Stub in v1.5.0** — links two or more **Main** instances for shared `/proxymail` and `/proxychat` (also stubs). Opt-in: `-DHYBBX_PLUGIN_MAINS_PROXY=ON`, `[networks] mains_proxy=yes`.

Peers connect **only via HBX/Circuit** (`circuit_host`, `circuit_port`, `link_id`, `link_password`) — same security model as Secondary. No raw Main-to-Main TCP bypass. RF between sites may use a Secondary on each side; direct Main↔Main circuit is also allowed (full or half duplex). Detail: [docs/MAINS_PROXY.md](docs/MAINS_PROXY.md).

### HBX/Circuit — inter-node transport

All HyBBX-to-HyBBX paths use **HBX v1** frames on the circuit hub. The core never parses KISS, on-air AX.25, or serial. User wire protocols (telnet, SSH, WebSocket) are separate from link auth.

### Security

Built-in `[security]` covers **network protection** and **abuse** (same subsystem — see [docs/SECURITY.md](docs/SECURITY.md)):

- **No bans for normal spam** — `[traffic]`, `[chat]`, `[mail]` soft limits only (pace, truncate, caps).
- **Bans for abuse** — IP: login brute-force, connection flood; **CALLID** (AX.25 callsign, HBX `link_id`): circuit auth failures, `ban_callid=`; *(future)* excessive flood via `abuse_maxretry`.
- Short cool-down bans (default 10 min); `security.log`; optional `iptables`/`nftables`. External fail2ban in `share/fail2ban/` optional.

## Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/hybbx.sh
telnet 127.0.0.1 2323
```

[docs/QUICKSTART.md](docs/QUICKSTART.md)

## Documentation

| Audience | Start here |
|----------|------------|
| Operator | [docs/QUICKSTART.md](docs/QUICKSTART.md) → [docs/MANUAL.md](docs/MANUAL.md) |
| Topology | [docs/TOPOLOGY.md](docs/TOPOLOGY.md) |
| Features | [docs/FEATURES.md](docs/FEATURES.md) |
| WebSocket | [docs/WEBSOCKET.md](docs/WEBSOCKET.md) |
| Developer | [docs/BUILD.md](docs/BUILD.md) → [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) |
| Index | [docs/INDEX.md](docs/INDEX.md) |

GPL-3.0 — [LICENSE.txt](LICENSE.txt)
