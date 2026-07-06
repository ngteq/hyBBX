HyBBX reverse-proxy examples. WebSocket plugin = session data on loopback only.

1. HTTPD_DOCROOT=/srv/www   # your httpd document root
2. cp -r docroot/hybbx-websocket $HTTPD_DOCROOT/
3. Add nginx/apache/lighttpd snippet (adjust paths)
4. hybbx.ini [transport.websocket] — see docs/WEBSOCKET.md
