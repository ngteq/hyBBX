# WebSocket · HyBBX 2.4.0

WebSocket forward-proxy for browser and rich clients — core stays text-first.

## Transport matrix

| Item | Value |
|------|-------|
| Plugin | `HYBBX_PLUGIN_WEBSOCKET=ON` (default) |
| Default port | `4591` (plain); wss with OpenSSL |
| INI | `[networks] websocket=yes`, `[transport.websocket]` |
| Reverse proxy | nginx/caddy TLS termination typical |

## Security matrix

| Item | Value |
|------|-------|
| User auth | Same as telnet — session login |
| TLS | Optional `-DHYBBX_CRYPTO_OPENSSL=ON` for wss |
| Link auth | Separate from HBX circuit auth |

## Related

| Goal | Doc |
|------|-----|
| Clients | [CLIENTS.md](CLIENTS.md) |
| Security | [SECURITY.md](SECURITY.md) |
