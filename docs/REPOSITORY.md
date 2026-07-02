# HyBBX repository layout

Source tree. API: `include/hybbx/`. Config: `share/hybbx.ini.example`.

```
hyBBX/
  include/hybbx/          Public C API (config, session, plugin, circuit, tnc, …)
  src/
    core/                 Main: sessions, storage, crypto, circuit hub
    clients/              Standalone CLI clients (hybbx-telnet, hybbx-terminal)
    main.c                Entry point, plugin registration
  plugins/
    telnet/               TCP/IPv4+IPv6 telnet link adapter
    packet_radio/         Secondary: AX.25 + TNC stack; HBX client to Main circuit
  third_party/            Bundled crypto (tinysha256, Monocypher, tiny-AES-c)
  text/                   banner.txt, motd.txt, news.txt, rules.txt
  share/                  hybbx.ini.example (Main), hybbx-secondary.ini.example (Secondary)
  local/                  Local dev config and data (not required for install)
  scripts/                hybbx.sh, dev-setup.sh
  cmake/                  CMake modules, platform toolchains (AmigaOS 3.9+)
  docs/                   Documentation (INDEX, FEATURES, PLATFORMS, …)
  .github/                Issue and PR templates
  .cursor/rules/          Cursor AI project rules
  AGENTS.md               AI agent onboarding
  CONTRIBUTING.md         Human contributor guide
  .editorconfig           Editor formatting defaults
```

## Main components

| Path | Role |
|------|------|
| `src/core/service.c` | Config apply, transport plugins, circuit hub, session attachment |
| `src/core/circuit*.c` | HBX framing and internal TCP hub (IPv4/IPv6) |
| `src/core/session.c` | Per-connection line-oriented session, areas, I/O |
| `src/core/command.c` | `/` command dispatch and privileges |
| `src/core/storage_flatfile.c` | INI user shards, legacy migration |
| `plugins/packet_radio/` | KISS, 6PACK, TNC2 host mode, AX.25, TNC profiles |
| `plugins/telnet/` | Minimal RFC telnet over TCP |

Plugin interface: `include/hybbx/plugin.h` (`hybbx_transport_plugin_t`).

Build system: top-level `CMakeLists.txt`, `src/CMakeLists.txt`, per-plugin `CMakeLists.txt`.

See also: [BUILD.md](BUILD.md), [DEVELOPMENT.md](DEVELOPMENT.md), [MANUAL.md — Architecture](MANUAL.md#architecture).
