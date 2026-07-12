# Entertain Area — plugins

**v2.0.0** · operator INI: [MANUAL.md](MANUAL.md) · code: [DEVELOPMENT.md](DEVELOPMENT.md)

The **Entertain Area** is a set of optional, user-facing applications on **Main** (telnet/SSH/WebSocket sessions). Games and similar apps are **not** part of `src/core/`.

## Rule

**Every Entertain application is a plugin** under `plugins/`. Core keeps sessions, storage, commands registry, and HBX only — no game logic, boards, or entertain wire formats in `src/core/`.

| Layer | Responsibility |
|-------|----------------|
| `src/core/` | Sessions, command dispatch, shared limits |
| `plugins/<entertain>/` | App state, `/` verbs, multi-user room logic |

Same opt-in model as other plugins: CMake `HYBBX_PLUGIN_*`, `[networks]` enable flag, register in `src/main.c`.

## First app: text chess

Planned as an entertain plugin (working name `plugins/chess` or `plugins/entertain_chess`):

- ASCII board in the terminal
- Multi-user: two players, observers in the same entertain session/room
- Commands owned by the plugin (registered in [commands.yaml](../share/commands.yaml) or plugin loader)
- No RF, no HBX circuit, no proxy fan-out

Other entertain apps (future plugins) follow the same pattern — one plugin per app or one `entertain` plugin with sub-modules, but always under `plugins/`, never core.

## Operator

Entertain plugins run on **Main** only (where user sessions live). Secondary remains RF/HBX edge infrastructure.

Enable per plugin when shipped, e.g.:

```ini
[networks]
chess = yes
```

Exact keys follow each plugin’s MANUAL section when the plugin lands.

## Developer checklist

1. New directory under `plugins/`
2. CMake option `HYBBX_PLUGIN_<NAME>` (default OFF until stable)
3. No KISS/AX.25/telnet wire parsing in core
4. Commands: two-line help, Sysop/Admin/Mod/User/Guest rules per app
5. Docs: this file + MANUAL subsection + `share/*.ini.example` when INI keys exist

## See also

[TOPOLOGY.md](TOPOLOGY.md) · [COMMANDS.md](COMMANDS.md) · [DEVELOPMENT.md](DEVELOPMENT.md)
