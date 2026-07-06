# WebSocket transport

Forward-proxy only — RFC6455 to the session core. HyBBX auth via `hybbx.ini`
(same as telnet/SSH). Public TLS on nginx/Apache/lighttpd.

## Install layout

```
~/hybbx/                    # HYBBX_ROOT — always start from here
├── hybbx                   # binary
├── hybbx-start             # cd $HYBBX_ROOT && exec hybbx -c hybbx.ini
├── hybbx.ini
├── keys/                   # cert_dir + SSH host keys
│   ├── hybbx_ws.crt
│   └── hybbx_ws.key
├── data/  logs/  text/  web/
└── nginx/  apache2/  lighttpd/   # proxy examples
```

```sh
cd ~/hybbx && ./hybbx-start
# or: HYBBX_ROOT=~/hybbx ~/hybbx/hybbx -c ~/hybbx/hybbx.ini
```

Relative INI paths (`keys`, `data`, `text`) resolve under `HYBBX_ROOT`
(dirname of `hybbx.ini`).

## Topology

```
Browser  --wss://host/hybbx-telnet-->  nginx  --wss://127.0.0.1:4591/hybbx-->  hybbx
```

| Layer | Address |
|-------|---------|
| Public | `wss://your-host/hybbx-telnet` (your site cert) |
| HyBBX | `127.0.0.1:4591` path `/hybbx` (loopback, self-signed `keys/hybbx_ws.*`) |

Check: `sockstat -4 -l | grep 4591` → `127.0.0.1:4591`

## INI

```ini
[networks]
websocket = yes

[transport.websocket]
enabled = yes
bind = 127.0.0.1
port = 4591
path = /hybbx
cert_dir = keys
ipv4 = yes
ipv6 = no
```

OpenSSL build → auto-creates `keys/hybbx_ws.crt` + `.key` (5 years), listens **wss**.
No OpenSSL → plain **ws** on the same port.

## nginx (recommended)

Public path `/hybbx-telnet`, backend HyBBX path `/hybbx`:

```nginx
location /hybbx-telnet {
    proxy_pass https://127.0.0.1:4591/hybbx;
    proxy_ssl_verify off;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}
```

Alternative: set `path = /hybbx-telnet` in `hybbx.ini` and use
`proxy_pass https://127.0.0.1:4591;` (no URI suffix).

Full examples: `share/nginx/`, `share/apache2/`, `share/lighttpd/`.

## PHP terminal

`share/web/hybbx-terminal.php` — set:

```php
$ws_url = 'wss://your-host/hybbx-telnet';
```

## Test

```sh
wscat -c wss://127.0.0.1:4591/hybbx --no-check          # direct
wscat -c wss://your-host/hybbx-telnet --no-check      # via nginx
```

See [MANUAL.md](MANUAL.md#transportwebsocket) · [CLIENTS.md](CLIENTS.md)
