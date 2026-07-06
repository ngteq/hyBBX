# WebSocket transport

HyBBX `websocket` plugin = **forward-proxy only** (bytes ↔ session core).
Browser UI and public TLS live on **nginx / Apache / lighttpd**.

## Install layout

```
~/hybbx/                         HYBBX_ROOT
├── hybbx / hybbx-start / hybbx.ini
├── keys/                        hybbx_ws.* + SSH host keys
├── hybbx-websocket/             HTTP UI only (reverse-proxy serves GET)
│   ├── index.php
│   └── hybbx-terminal.js
└── reverse-proxy/               nginx / apache2 / lighttpd examples
```

```sh
cd ~/hybbx && ./hybbx-start
sockstat -4 -l | grep 4591    # 127.0.0.1:4591
```

## Roles

| Piece | Role |
|-------|------|
| `hybbx` `:4591` `/hybbx` | WebSocket forward-proxy (loopback, wss + self-signed `keys/`) |
| `hybbx-websocket/` | Static/PHP terminal UI — **httpd fetches files here only** |
| `/hybbx-websocket/ws` | Public WebSocket URL — **httpd forward-proxies to hybbx** |

```
Browser  --GET-->  /hybbx-websocket/     -->  files in ~/hybbx/hybbx-websocket/
Browser  --wss-->  /hybbx-websocket/ws   -->  wss://127.0.0.1:4591/hybbx  -->  hybbx
```

## INI

```ini
[networks]
websocket = yes

[transport.websocket]
bind = 127.0.0.1
port = 4591
path = /hybbx
cert_dir = keys
ipv4 = yes
ipv6 = no
```

## Reverse proxy

Copy and edit `~/hybbx/reverse-proxy/*.conf.example` — set `HYBBX_ROOT`.

**nginx** (summary):

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
    alias /home/hybbx/hybbx/hybbx-websocket/;
    index index.php;
}
```

`index.php` sets `wss://<host>/hybbx-websocket/ws` automatically.

## Test

```sh
wscat -c wss://127.0.0.1:4591/hybbx --no-check
wscat -c wss://your-host/hybbx-websocket/ws --no-check
```

See [MANUAL.md](MANUAL.md#transportwebsocket) · `share/reverse-proxy/`
