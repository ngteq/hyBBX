# Repository layout

API: `include/hybbx/`. Config templates: `share/`. Full INI keys: [MANUAL.md](MANUAL.md).

```
hyBBX/
  include/hybbx/         Public C API
  src/core/              Session, storage, circuit, commands
  src/clients/           hybbx-telnet, hybbx-terminal
  src/main.c             Daemon entry
  plugins/telnet/        Telnet (verified path)
  plugins/ssh/           SSH transport (libssh)
  plugins/websocket/     WebSocket forward-proxy
  plugins/packet_radio/  AX.25 / TNC (built; RF TBD)
  plugins/ardop/         ARDOP plugin (standalone)
  plugins/crdop/         CRDOP plugin (standalone)
  third_party/           Crypto (not modems)
  text/                  Runtime texts
  share/                 INI examples
  docs/                  Technical documentation
  scripts/               dev-setup, hybbx.sh, tests
```

| Module | File(s) |
|--------|---------|
| Service lifecycle | `service.c` |
| HBX hub | `circuit_tcp.c`, `circuit.c` |
| Sessions | `session.c` |
| Commands | `command.c` |
| Users | `storage_flatfile.c`, `auth.c` |

Plugin API: [plugin.h](../include/hybbx/plugin.h).
