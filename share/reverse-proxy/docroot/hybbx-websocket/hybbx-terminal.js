(function () {
  'use strict';

  var term = document.getElementById('term');
  var status = document.getElementById('status');
  var form = document.getElementById('input-bar');
  var input = document.getElementById('cmd');
  var wsUrl = window.HYBBX_WS_URL;
  var ws = null;
  var reconnectTimer = null;
  var reconnectDelayMs = 2000;
  var reconnectAttempt = 0;

  function append(text) {
    term.textContent += text;
    term.scrollTop = term.scrollHeight;
  }

  function setStatus(s) {
    status.textContent = s;
  }

  function scheduleReconnect() {
    if (reconnectTimer) {
      return;
    }

    reconnectAttempt += 1;
    setStatus('reconnecting in ' + (reconnectDelayMs / 1000) + 's');
    reconnectTimer = window.setTimeout(function () {
      reconnectTimer = null;
      connect();
    }, reconnectDelayMs);
  }

  function setInputEnabled(enabled) {
    input.disabled = !enabled;
    if (enabled) {
      input.focus();
    }
  }

  function sendLine(text) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      return;
    }

    ws.send(text + '\r');
  }

  function connect() {
    setStatus('connecting');
    setInputEnabled(false);
    ws = new WebSocket(wsUrl);

    ws.onopen = function () {
      reconnectAttempt = 0;
      setStatus('connected');
      setInputEnabled(true);
    };

    ws.onmessage = function (ev) {
      append(ev.data);
    };

    ws.onclose = function (ev) {
      var reason = ev.reason ? ' (' + ev.reason + ')' : '';

      append('\n[websocket disconnected: code ' + ev.code + reason + ']\n');
      setStatus('disconnected: ' + ev.code);
      ws = null;
      setInputEnabled(false);
      scheduleReconnect();
    };

    ws.onerror = function () {
      setStatus('error');
      setInputEnabled(false);
    };
  }

  form.addEventListener('submit', function (ev) {
    ev.preventDefault();

    if (!ws || ws.readyState !== WebSocket.OPEN) {
      return;
    }

    sendLine(input.value);
    input.value = '';
    input.focus();
  });

  connect();
})();
