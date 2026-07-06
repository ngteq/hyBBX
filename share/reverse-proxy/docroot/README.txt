Copy docroot/hybbx-websocket/ to HTTPD_DOCROOT (not HYBBX_ROOT).
HyBBX websocket plugin ships session data only on loopback :4591.

  HTTPD_DOCROOT=/srv/www
  cp -r docroot/hybbx-websocket $HTTPD_DOCROOT/

Match URL paths in hybbx.ini and the httpd snippets in docs/WEBSOCKET.md.
