# WebSocket transport

**v1.7.5** — session forward-proxy on loopback. Does **not** serve PHP, JS, or static files.

Browser UI files live in your **httpd document root** (`HTTPD_DOCROOT`). Copy from
`~/hybbx/reverse-proxy/docroot/hybbx-websocket/`. Public TLS terminates on
nginx, Apache httpd, or lighttpd.

## Steps

1. **`hybbx.ini`** — enable `[transport.websocket]` (below).
2. **`./hybbx-start`** — listens `127.0.0.1:4591`, path `/hybbx`.
3. **Copy UI** to `HTTPD_DOCROOT`:
   ```sh
   HTTPD_DOCROOT=/srv/www    # your httpd document root
   cp -r ~/hybbx/reverse-proxy/docroot/hybbx-websocket "$HTTPD_DOCROOT/"
   ```
4. **httpd snippet** — one section below; set paths to match `HTTPD_DOCROOT`.

## Layout

```
~/hybbx/                              HYBBX install
├── hybbx / hybbx.ini / keys/
└── reverse-proxy/
    ├── docroot/hybbx-websocket/      → copy to HTTPD_DOCROOT
    ├── nginx.conf.example
    ├── apache2.conf.example
    └── lighttpd.conf.example

$HTTPD_DOCROOT/hybbx-websocket/       served by httpd
├── index.php
└── hybbx-terminal.js
```

## hybbx.ini

```ini
[networks]
websocket = yes

[transport.websocket]
bind = 127.0.0.1
port = 4591
path = /hybbx
cert_dir = keys
max_connections = 10
ipv4 = yes
ipv6 = no
```

| Key | Default | Role |
|-----|---------|------|
| `port` | `4591` | Loopback listen |
| `path` | `/hybbx` | Upgrade path (httpd proxies `{url}/ws` here) |
| `max_connections` | `10` | Simultaneous WebSocket clients |

| Public URL | Handler |
|------------|---------|
| `GET /hybbx-websocket/` | files from **HTTPD_DOCROOT** |
| `WS /hybbx-websocket/ws` | forward-proxy → `wss://127.0.0.1:4591/hybbx` |

No OpenSSL in HyBBX → use `http://127.0.0.1:4591/hybbx` in httpd snippets.
HyBBX sends WebSocket ping keepalives on idle connections; keep proxy idle
timeouts at `3600s` or similar to avoid premature disconnects.

---

## nginx

Set `root` to `HTTPD_DOCROOT` (examples use `/srv/www`).

```nginx
location = /hybbx-websocket/ws {
    proxy_pass https://127.0.0.1:4591/hybbx;
    proxy_ssl_verify off;
    proxy_http_version 1.1;
    proxy_socket_keepalive on;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}

location /hybbx-websocket/ {
    root /srv/www;
    index index.php;
    location ~ \.php$ {
        include fastcgi_params;
        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
        fastcgi_pass unix:/run/php-fpm/www.sock;
    }
}
```

```sh
nginx -t && systemctl reload nginx
```

PHP-FPM socket path varies — check your `php-fpm` pool config.

---

## Apache httpd

```apache
<IfModule mod_proxy.c>
    ProxyPreserveHost On
    ProxyTimeout 3600
    SSLProxyEngine On
    SSLProxyVerify none
    SSLProxyCheckPeerCN off
    SSLProxyCheckPeerName off

    ProxyPass "/hybbx-websocket/ws" "wss://127.0.0.1:4591/hybbx" timeout=3600 keepalive=On
    ProxyPassReverse "/hybbx-websocket/ws" "wss://127.0.0.1:4591/hybbx"
</IfModule>

Alias /hybbx-websocket /srv/www/hybbx-websocket
<Directory /srv/www/hybbx-websocket>
    DirectoryIndex index.php
    Require all granted
    <FilesMatch \.php$>
        SetHandler application/x-httpd-php
    </FilesMatch>
</Directory>
```

```sh
apachectl configtest && systemctl reload httpd
```

Service name may be `apache2` on your system.

---

## lighttpd

```lighttpd
# Raise idle limits for upgraded WebSocket connections.
server.max-read-idle = 3600
server.max-write-idle = 3600

$HTTP["url"] == "/hybbx-websocket/ws" {
    proxy.server = ( "" => (
        "hybbx" => (
            "proxy"   => "https://127.0.0.1:4591/hybbx",
            "upgrade" => "enable"
        )
    ))
    proxy.header = ( "upgrade" => "enable" )
}

alias.url += ( "/hybbx-websocket/" => "/srv/www/hybbx-websocket/" )
index-file.names += ( "index.php" )

fastcgi.server += ( ".php" =>
    ( "localhost" =>
        ( "socket" => "/run/php-fpm/www.sock" )
    )
)
```

```sh
systemctl reload lighttpd
```

---

## Test

```sh
ss -lntp | grep 4591
wscat -c wss://127.0.0.1:4591/hybbx --no-check
wscat -c wss://your-host/hybbx-websocket/ws --no-check
```

Browser: `https://your-host/hybbx-websocket/`

The browser UI shows WebSocket close codes/reasons and retries automatically
after short disconnects.

See [MANUAL.md](MANUAL.md#transportwebsocket)
