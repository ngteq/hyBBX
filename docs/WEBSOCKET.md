# WebSocket transport

HyBBX = forward-proxy on loopback. Browser UI + public TLS = reverse-proxy only.

## Operator steps

1. Edit **`hybbx.ini`** `[transport.websocket]` (defaults below).
2. **`cd ~/hybbx && ./hybbx-start`** — writes `hybbx-websocket/hybbx-ws.json`.
3. Copy **`~/hybbx/reverse-proxy/nginx.conf.example`** (or apache2/lighttpd),
   set `HYBBX_ROOT`, reload httpd.

No PHP or JS edits.

## Layout

```
~/hybbx/
├── hybbx.ini
├── hybbx-websocket/          httpd serves GET from here
│   ├── index.php             reads hybbx-ws.json (auto)
│   ├── hybbx-terminal.js
│   └── hybbx-ws.json         generated on start
└── reverse-proxy/            httpd snippets
```

## hybbx.ini

```ini
[networks]
websocket = yes

[transport.websocket]
bind = 127.0.0.1
port = 4591
path = /hybbx
public_prefix = /hybbx-websocket
cert_dir = keys
ipv4 = yes
ipv6 = no
```

| Key | Default | Role |
|-----|---------|------|
| `port` | `4591` | Loopback listen |
| `path` | `/hybbx` | HyBBX upgrade path (proxy backend) |
| `public_prefix` | `/hybbx-websocket` | Public UI path; WS = `{prefix}/ws` |

## Reverse proxy (must match `public_prefix`)

| Public URL | httpd |
|------------|-------|
| `GET /hybbx-websocket/` | files from `$HYBBX_ROOT/hybbx-websocket/` |
| `WS /hybbx-websocket/ws` | `https://127.0.0.1:4591` + `path` |

```nginx
location = /hybbx-websocket/ws {
    proxy_pass https://127.0.0.1:4591/hybbx;
    proxy_ssl_verify off;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_read_timeout 3600s;
}
location /hybbx-websocket/ {
    root /home/hybbx/hybbx;
    index index.php;
}
```

If you change `public_prefix` in ini, use the same paths in httpd and restart hybbx.

See `share/reverse-proxy/` · [MANUAL.md](MANUAL.md#transportwebsocket)
