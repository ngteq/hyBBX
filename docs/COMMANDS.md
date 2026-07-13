# Command registry and help layout

**v2.0.0** — layout: [share/areas.yaml](../share/areas.yaml); commands: [share/commands.yaml](../share/commands.yaml). Operator summary: [MANUAL.md](MANUAL.md).

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
| `/help`, `/menu` | Filtered menu for this session (from `areas.yaml` + access) |
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

Layout and wording are identical across account types; `/index` lists every command. `/help` and `/menu` show only areas and commands allowed for the session level.

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
Help: Local announce to online users. More: /announce /broadcast ax25

/broadcast ax25
Help: Sequential RF beacon (ax25_auto_message; 60s between links).
```

## `/broadcast` (announce)

- Alias: **`/announce`**
- **Sysop** only

### `/broadcast <message>`

- Instant message to every **logged-in online user** on the local Main (telnet/SSH/WebSocket)
- Guests and registered users receive it; **circuit/TNC bridge sessions do not**
- Not RF, not HBX circuit fan-out, not the proxy network

### `/broadcast ax25`

- **No custom RF text** — uses INI `ax25_auto_message` (`@service@` token)
- Qualifying packet-radio links TX **one after another** (minimum 60 s between links; per-link minimum 900 s between any AX.25 broadcast)
- Separate from periodic INI **`ax25_auto`** beacons (sequential multi-link cycle; see [MANUAL.md](MANUAL.md) `[broadcast]`)

INI `[broadcast]` **ax25_auto** is background infrastructure — see [MANUAL.md](MANUAL.md).

## Proxy network

Primary term: **proxy network** (`mains_proxy` plugin, HBX/Circuit between Mains).

| Over proxy | Not over proxy |
|------------|----------------|
| `/proxymail`, `/proxychat` | Sysop, Admin, Mod actions |
| Additional plugin user services | `/broadcast`, `/shutdown`, account admin |

No administrative commands cross proxy links.

## Registry

`share/areas.yaml` (loaded first): headers, user groups, area layout, per-level `/help`/`/menu`, and full `/index`.

`share/commands.yaml`: command help lines, aliases, `min`/`max`/`only` access, and `rights` for account admin actions (`commands_registry.c`).

When changing commands: update both YAML files, then [MANUAL.md](MANUAL.md) and this file.

## See also

[AGENTS.md](../AGENTS.md) · [DEVELOPMENT.md](DEVELOPMENT.md) · [TOPOLOGY.md](TOPOLOGY.md) · [MAINS_PROXY.md](MAINS_PROXY.md)
