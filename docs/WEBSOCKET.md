# WebSocket transport

Browser and PHP frontends reach HyBBX through a **TLS reverse proxy**
(nginx, Apache httpd, or lighttpd). The `websocket` plugin is a **forward
proxy only** — it upgrades HTTP to WebSocket and relays raw bytes to the
session core. HyBBX login follows `hybbx.ini` (`[auth] auto_login` or
`/login`), same as telnet and SSH.

## Topology

```
Browser  --wss-->  nginx/Apache/lighttpd  --ws/wss-->  hybbx :591/hybbx  -->  session core
```

HyBBX binds **loopback** by default (`127.0.0.1` / `::1`). Public HTTP/HTTPS
and your site certificate live on the reverse proxy. The proxy forwards only
the WebSocket upgrade to HyBBX.

When the build links **OpenSSL**, HyBBX creates a self-signed certificate on
first start (`keys/hybbx_ws.crt` + `hybbx_ws.key`) and listens with **wss**
on port 591. Without OpenSSL, HyBBX serves plain **ws** on the same port.

## INI

```ini
[networks]
websocket = yes

[transport.websocket]
enabled = yes
bind = 127.0.0.1
bind6 = ::1
port = 591
path = /hybbx
cert_dir = keys
ipv4 = yes
ipv6 = yes
```

| Key | Default | Notes |
|-----|---------|-------|
| `port` | `591` | Loopback listen port |
| `path` | `/hybbx` | HTTP Upgrade URI path (must match proxy) |
| `cert_dir` | `keys` | Self-signed TLS files when OpenSSL is available |

## Reverse proxy examples

| Server | Example file |
|--------|----------------|
| nginx | `share/nginx/hybbx-websocket.conf.example` |
| Apache 2 | `share/apache2/hybbx-websocket.conf.example` |
| lighttpd | `share/lighttpd/hybbx-websocket.conf.example` |

Each file documents **plain ws** and **wss** backend variants. Match the
backend to what HyBBX prints at startup (`ws` or `wss`).

### nginx (plain ws backend)

```nginx
location /hybbx {
    proxy_pass http://127.0.0.1:591;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_read_timeout 3600s;
}
```

Use `wss://` on the public vhost with your TLS certificate. When HyBBX uses
local TLS, point `proxy_pass` at `https://127.0.0.1:591` and set
`proxy_ssl_verify off` for the self-signed cert.

## PHP frontend

Minimal terminal UI: `share/web/hybbx-terminal.php` + `hybbx-terminal.js`.

Set `$ws_url` in the PHP file to your public `wss://host/hybbx` endpoint.

## hybbx-telnet as CGI (optional)

For classic shared hosting without the WebSocket plugin, `share/cgi/hybbx-telnet.cgi.example`
shows a server-side bridge using the **hybbx-telnet** client to localhost telnet.
The WebSocket + reverse-proxy path is preferred (TLS at the edge, no long-lived CGI).

## Security

- No HyBBX credentials on the WebSocket wire — only RFC6455 framing.
- `/login` failures are logged to `security.log` (`transport=websocket`).
- fail2ban: enable `[hybbx-websocket]` in `share/fail2ban/jail.d/hybbx.local.example`.

See [MANUAL.md](MANUAL.md#transportwebsocket) · [CLIENTS.md](CLIENTS.md)
