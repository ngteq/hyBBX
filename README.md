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

<div align="center">
<sub>
<strong>UN1TME/UN1T.me · BBX</strong>
<br><br>
<table>
<tr><th>Protocol</th><th>Host</th><th>Port</th><th>Command / URL</th></tr>
<tr><td align="center">Telnet</td><td align="center"><code>hybbx.un1t.me</code></td><td align="center">2323</td><td align="center"><code>telnet hybbx.un1t.me 2323</code></td></tr>
<tr><td align="center">SSH</td><td align="center"><code>hybbx.un1t.me</code></td><td align="center">3232</td><td align="center"><code>ssh hybbx.un1t.me -p 3232</code></td></tr>
<tr><td align="center">HTTPS</td><td align="center"><code>hybbx.un1t.me</code></td><td align="center">443</td><td align="center"><a href="https://hybbx.un1t.me/">https://hybbx.un1t.me/</a></td></tr>
<tr><td align="center">HTTP</td><td align="center"><code>hybbx.un1t.me</code></td><td align="center">80</td><td align="center"><a href="http://hybbx.un1t.me/">http://hybbx.un1t.me/</a></td></tr>
</table>
<br>
<strong>hybbx.un1t.me</strong> Telnet <strong>:2323</strong> · SSH <strong>:3232</strong> · HTTP(S) <strong>:80/:443</strong> · Primary RF <strong>27.245 MHz / CB25</strong>
</sub>
<img width="80" height="60" alt="UN1TME/UN1T.me-BBX" src="https://github.com/user-attachments/assets/75bab047-12f8-4720-b1fa-2cd2dc86ad16" />
</div>
