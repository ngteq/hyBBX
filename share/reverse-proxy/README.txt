HyBBX reverse-proxy snippets — install with hybbx to ~/hybbx/reverse-proxy/

Split roles:
  hybbx-websocket/   HTTP only — browser UI (GET static/PHP from install tree)
  /hybbx-websocket/ws   WebSocket only — forward-proxy to 127.0.0.1:4591/hybbx

Set HYBBX_ROOT in the examples to your install path (e.g. /home/hybbx/hybbx).
