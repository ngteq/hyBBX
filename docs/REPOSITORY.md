# HyBBX repository layout

API: `include/hybbx/`. Config: `share/hybbx.ini.example`.

```
hyBBX/
  include/hybbx/       Public C API
  src/core/            Sessions, storage, crypto, circuit hub, commands
  src/clients/         hybbx-telnet, hybbx-terminal
  src/main.c           Entry, plugin registration
  plugins/telnet/      TCP telnet adapter
  plugins/packet_radio/  AX.25, TNC, HBX client
  third_party/         Bundled crypto
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

Plugin API: [plugin.h](../include/hybbx/plugin.h). Build: top-level + `src/` + per-plugin `CMakeLists.txt`.

[BUILD.md](BUILD.md) · [DEVELOPMENT.md](DEVELOPMENT.md) · [MANUAL.md](MANUAL.md)
