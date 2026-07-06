# WebSocket transport

HyBBX = forward-proxy on loopback. Browser UI + public TLS = reverse-proxy only.

## Operator steps

1. Edit **`hybbx.ini`** `[transport.websocket]` (defaults below).
2. **`cd ~/hybbx && ./hybbx-start`** — writes `hybbx-websocket/hybbx-ws.json`.
3. Add one httpd snippet below (set `HYBBX_ROOT`), reload httpd.

No PHP or JS edits.

## Layout

```
~/hybbx/                         HYBBX_ROOT=/home/hybbx/hybbx
├── hybbx.ini
├── hybbx-websocket/             httpd GET only
│   ├── index.php                  reads hybbx-ws.json
│   ├── hybbx-terminal.js
│   └── hybbx-ws.json              generated on start
└── reverse-proxy/                 copy-paste snippets
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
| `public_prefix` | `/hybbx-websocket` | Public UI; WS URL = `{prefix}/ws` |

Paths in httpd **must match** `public_prefix` and `path`. Change ini → restart hybbx →
update httpd.

| Public URL | httpd |
|------------|-------|
| `GET /hybbx-websocket/` | files from `$HYBBX_ROOT/hybbx-websocket/` |
| `WS /hybbx-websocket/ws` | `wss://127.0.0.1:4591/hybbx` |

HyBBX without OpenSSL: use `http://127.0.0.1:4591/hybbx` / `ws://` in the examples below.

---

## nginx

Inside your public `server { }` TLS block. FreeBSD: `pkg install nginx`.

```nginx
# HYBBX_ROOT=/home/hybbx/hybbx

location = /hybbx-websocket/ws {
    proxy_pass https://127.0.0.1:4591/hybbx;
    proxy_ssl_verify off;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}

location /hybbx-websocket/ {
    root /home/hybbx/hybbx;
    index index.php;
    location ~ \.php$ {
        include fastcgi_params;
        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
        fastcgi_pass unix:/var/run/php-fpm.sock;
    }
}
```

```sh
nginx -t && service nginx reload
```

Full file: `~/hybbx/reverse-proxy/nginx.conf.example`

---

## Apache 2.4 (apache24)

Inside your `VirtualHost *:443`. FreeBSD: `pkg install apache24 mod_proxy_html`.

```apache
# HYBBX_ROOT=/home/hybbx/hybbx
# a2enmod proxy proxy_http proxy_wstunnel ssl  (Debian)
# LoadModule proxy_wstunnel_module ...          (FreeBSD httpd.conf)

<IfModule mod_proxy.c>
    ProxyPreserveHost On
    SSLProxyEngine On
    SSLProxyVerify none
    SSLProxyCheckPeerCN off
    SSLProxyCheckPeerName off

    ProxyPass "/hybbx-websocket/ws" "wss://127.0.0.1:4591/hybbx"
    ProxyPassReverse "/hybbx-websocket/ws" "wss://127.0.0.1:4591/hybbx"
</IfModule>

Alias /hybbx-websocket /home/hybbx/hybbx/hybbx-websocket
<Directory /home/hybbx/hybbx/hybbx-websocket>
    DirectoryIndex index.php
    Require all granted
    <FilesMatch \.php$>
        SetHandler application/x-httpd-php
    </FilesMatch>
</Directory>
```

```sh
apachectl configtest && service apache24 reload
```

Full file: `~/hybbx/reverse-proxy/apache2.conf.example`

---

## lighttpd

`server.modules` needs `mod_proxy`, `mod_fastcgi` (for PHP). FreeBSD: `pkg install lighttpd`.

```lighttpd
# HYBBX_ROOT=/home/hybbx/hybbx

$HTTP["url"] == "/hybbx-websocket/ws" {
    proxy.server = ( "" => (
        "hybbx" => (
            "proxy"   => "https://127.0.0.1:4591/hybbx",
            "upgrade" => "enable"
        )
    ))
    proxy.header = ( "upgrade" => "enable" )
}

alias.url += ( "/hybbx-websocket/" => "/home/hybbx/hybbx/hybbx-websocket/" )
index-file.names += ( "index.php" )

fastcgi.server += ( ".php" =>
    ( "localhost" =>
        ( "socket" => "/var/run/php-fpm.sock" )
    )
)
```

```sh
service lighttpd reload
```

Full file: `~/hybbx/reverse-proxy/lighttpd.conf.example`

---

## Test

```sh
sockstat -4 -l | grep 4591
cat ~/hybbx/hybbx-websocket/hybbx-ws.json

wscat -c wss://127.0.0.1:4591/hybbx --no-check
wscat -c wss://your-host/hybbx-websocket/ws --no-check
```

Browser: `https://your-host/hybbx-websocket/`

See [MANUAL.md](MANUAL.md#transportwebsocket)
