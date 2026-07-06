Copy docroot/hybbx-websocket/ to your httpd document root (not HYBBX_ROOT).
HyBBX websocket plugin ships session data only on loopback :4591.

  cp -r docroot/hybbx-websocket /usr/local/www/

Match URL paths in hybbx.ini path and the httpd snippets in WEBSOCKET.md.
