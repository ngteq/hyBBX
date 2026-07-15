# HyBBX · 2.4.0

C99 BBX daemon: text wire, `/` commands, mail, chat, conference. Main + Secondary topology over HBX; RF via external modems and MAX25 stack.

## Feature matrix

| Feature | Main | Secondary | Standalone |
|---------|------|-----------|------------|
| User login (telnet/SSH/WS) | yes | no | yes |
| Mail / chat / conference | yes | via Main | yes |
| HBX circuit hub | yes | client | optional |
| `packet_radio` / ARDOP / CRDOP | via plugin | yes | yes |
| MAX25 attach (`[max25]`) | optional | typical | optional |
| `mains_proxy` | opt-in build | opt-in | opt-in |

## Transport matrix

| Transport | Port (default) | Notes |
|-----------|----------------|-------|
| Telnet | 2323 | Plain text |
| SSH | 3232 | Requires libssh build |
| WebSocket | configurable | Menus / clients |
| HBX | 7323 | Link auth + `link_password` |
| MAX25 KISS | via max25d :7325 | v2.4.0 attach-only |

## v2.4.0 RF boundary matrix

| Rule | Value |
|------|-------|
| `tnc=baycom\|pccom` in `packet_radio` | **rejected** |
| `kiss_entry` (local TNC) | `none` (MAX25 prep assumed) |
| `[max25] check` | `yes` (default) — start fails if max25d down |
| `[networks] baycom` | `no` (default) — BayCom hardware via MAX25 |

## Build · test · run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
cmake -B build-test -DHYBBX_BUILD_TESTS=ON && cmake --build build-test && ctest --test-dir build-test
./scripts/hybbx.sh
```

## Config (one path)

1. Pick template under `share/hybbx-*.ini.example`.
2. Copy to **`local/hybbx.ini`** (gitignored).
3. Run vault `local-generate-secrets.sh` — live secrets in `./local/` only.

## Related

| Goal | Doc |
|------|-----|
| Operate | [MANUAL.md](docs/MANUAL.md) |
| RF + MAX25 order | [TNCS.md](docs/TNCS.md) · [MASTER-GUIDE.md](docs/MASTER-GUIDE.md) |
| Full index | [docs/README.md](docs/README.md) |

<br><br>

<p align="center"><sub><strong>UN1TME/UN1T.me - BBX</strong><br>
| Protocol | Host | Port | Command / URL |
|----------|------|------|---------------|
| Telnet | `hybbx.un1t.me` | 2323 | `telnet hybbx.un1t.me 2323` |
| SSH | `hybbx.un1t.me` | 3232 | `ssh hybbx.un1t.me -p 3232` |
| HTTPS | `hybbx.un1t.me` | 443 | [https://hybbx.un1t.me/](https://hybbx.un1t.me/) |
| HTTP | `hybbx.un1t.me` | 80 | [http://hybbx.un1t.me/](http://hybbx.un1t.me/) |
Telnet **:2323** · SSH **:3232** · HTTP(S) **:80/:443** (proxy) · Primary RF **27.245 MHz / CB25**</sub></p>
