# WebSocket transport

**v1.0.1** — session forward-proxy on loopback. Does **not** serve PHP, JS, or static files.

Browser UI files live in your **httpd document root** (copy from
`~/hybbx/reverse-proxy/docroot/hybbx-websocket/`). Public TLS terminates on
nginx, Apache 2.4, or lighttpd.

## Steps

1. **`hybbx.ini`** — enable `[transport.websocket]` (below).
2. **`./hybbx-start`** — listens `127.0.0.1:4591`, path `/hybbx`.
3. **Copy UI** to httpd docroot:
   ```sh
   cp -r ~/hybbx/reverse-proxy/docroot/hybbx-websocket /usr/local/www/
   ```
4. **httpd snippet** — one of the sections below (`NGINX_DOCROOT` = your www path).

## Layout

```
~/hybbx/                         HYBBX — data only
├── hybbx / hybbx.ini / keys/
└── reverse-proxy/
    ├── docroot/hybbx-websocket/   → copy to httpd document root
    ├── nginx.conf.example
    ├── apache2.conf.example
    └── lighttpd.conf.example

/usr/local/www/hybbx-websocket/  NGINX_DOCROOT (example)
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
| `GET /hybbx-websocket/` | files from **httpd docroot** |
| `WS /hybbx-websocket/ws` | forward-proxy → `wss://127.0.0.1:4591/hybbx` |

No OpenSSL in HyBBX → use `http://127.0.0.1:4591/hybbx` in httpd snippets.

---

## nginx

`NGINX_DOCROOT=/usr/local/www` (FreeBSD default; adjust to match `nginx.conf`).

```nginx
location = /hybbx-websocket/ws {
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

location /hybbx-websocket/ {
    root /usr/local/www;
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

---

## Apache 2.4 (apache24)

```apache
<IfModule mod_proxy.c>
    ProxyPreserveHost On
    SSLProxyEngine On
    SSLProxyVerify none
    SSLProxyCheckPeerCN off
    SSLProxyCheckPeerName off

    ProxyPass "/hybbx-websocket/ws" "wss://127.0.0.1:4591/hybbx"
    ProxyPassReverse "/hybbx-websocket/ws" "wss://127.0.0.1:4591/hybbx"
</IfModule>

Alias /hybbx-websocket /usr/local/www/hybbx-websocket
<Directory /usr/local/www/hybbx-websocket>
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

---

## lighttpd

```lighttpd
$HTTP["url"] == "/hybbx-websocket/ws" {
    proxy.server = ( "" => (
        "hybbx" => (
            "proxy"   => "https://127.0.0.1:4591/hybbx",
            "upgrade" => "enable"
        )
    ))
    proxy.header = ( "upgrade" => "enable" )
}

alias.url += ( "/hybbx-websocket/" => "/usr/local/www/hybbx-websocket/" )
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

---

## Test

```sh
sockstat -4 -l | grep 4591
wscat -c wss://127.0.0.1:4591/hybbx --no-check
wscat -c wss://your-host/hybbx-websocket/ws --no-check
```

Browser: `https://your-host/hybbx-websocket/`

See [MANUAL.md](MANUAL.md#transportwebsocket)
