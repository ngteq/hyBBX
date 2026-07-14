# Clients · HyBBX 2.4.0

User-facing connection clients for HyBBX sessions.

## Client matrix

| Client | Transport | Default port |
|--------|-----------|--------------|
| `hybbx-telnet` | TCP telnet | 2323 |
| `hybbx-ssh` | SSH (libssh) | 3232 |
| `hybbx-terminal` | HBX circuit | 7323 |
| Web browser | WebSocket via reverse proxy | site-specific |

## Session matrix

| Item | Value |
|------|-------|
| Wire format | Plain text lines + `/` commands |
| ANSI | `[traffic] ansi=yes` optional |
| Guest login | `[service] auto_login=yes` |
| Registered users | `/login` after connect |

## Related

| Goal | Doc |
|------|-----|
| WebSocket setup | [WEBSOCKET.md](WEBSOCKET.md) |
| Manual | [MANUAL.md](MANUAL.md) |
