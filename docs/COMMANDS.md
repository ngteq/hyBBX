# Command registry and help layout

**v1.7.5** — source of commands: [share/commands.yaml](../share/commands.yaml). Operator summary: [MANUAL.md](MANUAL.md).

## User groups

Exactly five levels (`include/hybbx/auth.h`). Use these names in help, menus, and docs — never **Staff**.

| Group | Stored | Notes |
|-------|--------|-------|
| Sysop | `sysop` | One protected account; `/shutdown`, `/broadcast`, `/deleteuser` |
| Admin | `admin` | `/usercreate`, `/activate`, `/promote`, `/demote`, `/delete`, `/changeuser` |
| Mod | `mod` | Same session commands as User today |
| User | `user` | Registered; mail, chat, conference |
| Guest | `guest` | Auto-login slots or login prompt |

## Command surfaces

| Input | Output |
|-------|--------|
| `/help`, `/menu` | Menu — commands **this session can use** |
| `/index` | Full command-index — **same list for every account** |
| `/alias` | Alias map |
| `/help <cmd>` | Two-line topic (see below) |

Headers (exact):

```
HyBBX commands /help <cmd> for more
HyBBX command-index /help <cmd> for more
HyBBX aliases /help <cmd> for more
```

Menu groups (fixed order): **General**, **Screen**, **Areas**, **Account**, **Admin**, **Sysop**.

Layout and wording are identical across account types; only which verbs appear changes by level.

## Help topic format

Two lines per command, each ≤80 ASCII columns. No square brackets in help or menu text.

```
/verb placeholders
Help: summary. More: subcommands or aliases
```

Same text for `/help <cmd>` and on-command usage hints. Use **for more** in headers; **More:** in topic lines when listing subcommands.

## Command output layout

When a session runs a `/command` (not chat, mail compose, or conference text):

1. **One blank line** before the command output (`hybbx_session_command_gap()`).
2. **No blank line** after the last output line (next input follows directly).

`/broadcast` and `/announce` use the same gap on every recipient session.

Examples:

```
/activate username
Help: Set username activated.

/proxymail
Help: mail to user@other-main. More: list read send delete recycle.

/broadcast message...
Help: Send message to all online users on this Main. More: /announce
```

## `/broadcast` (announce)

- Alias: **`/announce`**
- **Sysop** only
- Instant message to every **current online participant on the local Main**
- Not RF, not HBX circuit fan-out, not the proxy network

INI `[broadcast]` **ax25_auto** beacons are infrastructure (QST UI over qualifying HBX links) — not the `/broadcast` user command. See [MANUAL.md](MANUAL.md).

## Proxy network

Primary term: **proxy network** (`mains_proxy` plugin, HBX/Circuit between Mains).

| Over proxy | Not over proxy |
|------------|----------------|
| `/proxymail`, `/proxychat` | Sysop, Admin, Mod actions |
| Future user services when technically correct | `/broadcast`, `/shutdown`, account admin |

No administrative commands cross proxy links.

## Implementation status

| Piece | Status |
|-------|--------|
| `share/commands.yaml` | **Built** — registry source |
| Runtime loader from YAML | **Built** — `commands_registry.c`, loaded at startup |
| `/help`, `/menu`, `/index`, `/alias` | **Built** — registry-driven |
| Two-line `/help <cmd>` topics | **Built** — from YAML `line1` / `line2` |
| `/broadcast` local fan-out | **Built** — `hybbx_broadcast_announce()` |

When changing commands: update `commands.yaml`, then MANUAL.md and COMMANDS.md.

## See also

[AGENTS.md](../AGENTS.md) · [DEVELOPMENT.md](DEVELOPMENT.md) · [TOPOLOGY.md](TOPOLOGY.md) · [MAINS_PROXY.md](MAINS_PROXY.md)
