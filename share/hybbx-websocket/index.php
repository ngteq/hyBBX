<?php
/**
 * HyBBX browser terminal — static UI for reverse-proxy deployments.
 * WebSocket sessions use /hybbx-websocket/ws (proxied to hybbx :4591/hybbx).
 * See ../reverse-proxy/ for nginx, Apache, lighttpd examples.
 */
$scheme = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off') ? 'wss' : 'ws';
$host = $_SERVER['HTTP_HOST'] ?? 'localhost';
$ws_url = $scheme . '://' . $host . '/hybbx-websocket/ws';
?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HyBBX</title>
  <style>
    body { margin: 0; background: #0a0a12; color: #c8d0d8; font-family: monospace; }
    #term { box-sizing: border-box; width: 100%; height: 100vh; padding: 1rem;
            white-space: pre-wrap; overflow-y: auto; }
    #status { position: fixed; top: 0; right: 0; padding: .5rem 1rem; font-size: .8rem;
              background: #1a1a28; }
  </style>
</head>
<body>
  <div id="status">disconnected</div>
  <pre id="term"></pre>
  <script>
    window.HYBBX_WS_URL = <?php echo json_encode($ws_url, JSON_UNESCAPED_SLASHES); ?>;
  </script>
  <script src="hybbx-terminal.js"></script>
</body>
</html>
