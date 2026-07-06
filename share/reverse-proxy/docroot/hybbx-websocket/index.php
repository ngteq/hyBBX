<?php
/**
 * HyBBX browser terminal — copy this folder to your httpd document root.
 * WebSocket data comes from hybbx only; this file is not part of the daemon.
 */
$base = rtrim(dirname($_SERVER['SCRIPT_NAME'] ?? ''), '/');
$ws_path = ($base !== '' ? $base : '/hybbx-websocket') . '/ws';
$scheme = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off') ? 'wss' : 'ws';
$host = $_SERVER['HTTP_HOST'] ?? 'localhost';
$ws_url = $scheme . '://' . $host . $ws_path;
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
