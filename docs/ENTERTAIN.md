# Entertain Area — plugins

**v2.0.0** · operator INI: [MANUAL.md](MANUAL.md) · code: [DEVELOPMENT.md](DEVELOPMENT.md)

Optional user-facing apps on **Main** (telnet/SSH/WebSocket). Games and similar tools are **plugins only** — not in `src/core/`.

Entertain apps use the **text-first** model (ASCII boards, `/` move commands). Plugins may add menus or client graphics; core stays line-oriented for RF and low-bandwidth links.

## Rule

**Every Entertain app is a plugin** under `plugins/`. Core holds sessions, storage, commands registry, and HBX — no game logic in `src/core/`.

| Layer | Responsibility |
|-------|----------------|
| `src/core/` | Sessions, command dispatch, shared limits |
| `plugins/<entertain>/` | App state, `/` verbs, room logic |

Opt-in like other plugins: CMake `HYBBX_PLUGIN_*`, `[networks]` flag, register in `src/main.c`.

## Operator

Entertain plugins run on **Main** only. Secondary remains RF/HBX edge infrastructure.

Enable per plugin when installed:

```ini
[networks]
chess = yes
```

Exact keys follow each plugin’s MANUAL section.

## Developer checklist

1. New directory under `plugins/`
2. CMake option `HYBBX_PLUGIN_<NAME>` (default OFF)
3. No KISS/AX.25/telnet wire parsing in core
4. Commands: two-line help; Sysop/Admin/Mod/User/Guest rules per app
5. Docs: this file + MANUAL subsection + `share/*.ini.example` when INI keys exist

## See also

[TOPOLOGY.md](TOPOLOGY.md) · [COMMANDS.md](COMMANDS.md) · [DEVELOPMENT.md](DEVELOPMENT.md)
