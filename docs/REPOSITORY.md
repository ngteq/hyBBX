# HyBBX repository layout

API: `include/hybbx/`. Config: `share/hybbx.ini.example`.

**Scope:** session core + `plugins/` (host-client bridges). Modems/TNCs/sound-card apps are **external** — not in this tree. See [AGENTS.md](../AGENTS.md#product-boundary-default--non-negotiable).

```
hyBBX/
  include/hybbx/       Public C API
  src/core/            Sessions, storage, crypto, circuit hub, commands
  src/clients/         hybbx-telnet, hybbx-terminal
  src/main.c           Entry, plugin registration
  plugins/telnet/      TCP telnet adapter
  plugins/packet_radio/  Serial TNC host-client (external device)
  plugins/ardop/       ARDOP host-client (ARDOPC/ardopcf)
  plugins/crdop/       CRDOP CB host-client (external CRDOPC)
  third_party/         Bundled crypto (crypto only — not modems)
  text/                banner, motd, news, rules (runtime texts)
  share/               INI examples, fail2ban
  local/               Dev config (not installed)
  scripts/             hybbx.sh, dev-setup.sh
  docs/                Documentation
```

## Key modules

| Path | Role |
|------|------|
| `service.c` | Config, plugins, circuit hub, sessions |
| `circuit*.c` | HBX framing, TCP hub |
| `session.c` | Line I/O, areas, echo |
| `command.c` | `/` dispatch, privileges |
| `storage_flatfile.c` | INI user shards (50/user file) |
| `plugins/packet_radio/` | KISS, 6PACK, TNC2, AX.25 |
| `plugins/ardop/` | ARDOP host TCP — see [ARDOP.md](ARDOP.md) |
| `plugins/crdop/` | CRDOP CB host TCP — see [CRDOP.md](CRDOP.md) |

Plugin API: [plugin.h](../include/hybbx/plugin.h). Build: top-level + `src/` + per-plugin `CMakeLists.txt`.

[BUILD.md](BUILD.md) · [DEVELOPMENT.md](DEVELOPMENT.md) · [MANUAL.md](MANUAL.md)
