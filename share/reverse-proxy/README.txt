HyBBX reverse-proxy — match paths to hybbx.ini [transport.websocket]:
  public_prefix = /hybbx-websocket   →  UI at /hybbx-websocket/
  public_prefix + /ws                →  proxy to 127.0.0.1:port + path

Edit hybbx.ini only, start hybbx (writes hybbx-websocket/hybbx-ws.json), then
copy the snippet for your httpd and set HYBBX_ROOT.
