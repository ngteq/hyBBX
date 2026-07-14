# Commands · HyBBX 2.4.0

Slash-command reference — five access levels: Sysop → Admin → Mod → User → Guest.

## Level matrix

| Level | Typical commands |
|-------|------------------|
| Sysop | `/broadcast`, `/broadcast ax25`, `/shutdown` |
| Admin | `/usercreate`, `/activate`, `/promote`, `/demote` |
| Mod | moderation subset |
| User | `/mail`, `/chat`, `/who` |
| Guest | `/help`, `/menu` (filtered) |

## RF command matrix

| Command | Scope | Level |
|---------|-------|-------|
| `/broadcast <msg>` | Local online users | Sysop |
| `/broadcast ax25` | Sequential RF beacon | Sysop |

## Registry matrix

| Item | Path |
|------|------|
| Command definitions | `share/commands.yaml` |
| Area definitions | `share/areas.yaml` |
| `/index` | Lists all commands (every account) |
| `/who` (v2.4.0) | Interactive users only — not RF sessions |

## Related

| Goal | Doc |
|------|-----|
| Manual | [MANUAL.md](MANUAL.md) |
| Security | [SECURITY.md](SECURITY.md) |
