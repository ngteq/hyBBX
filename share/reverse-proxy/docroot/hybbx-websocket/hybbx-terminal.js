(function () {
  'use strict';

  var term = document.getElementById('term');
  var status = document.getElementById('status');
  var wsUrl = window.HYBBX_WS_URL;
  var ws = null;
  var line = '';

  function append(text) {
    term.textContent += text;
    term.scrollTop = term.scrollHeight;
  }

  function setStatus(s) {
    status.textContent = s;
  }

  function connect() {
    setStatus('connecting');
    ws = new WebSocket(wsUrl);

    ws.onopen = function () {
      setStatus('connected');
    };

    ws.onmessage = function (ev) {
      append(ev.data);
    };

    ws.onclose = function () {
      setStatus('disconnected');
      ws = null;
    };

    ws.onerror = function () {
      setStatus('error');
    };
  }

  document.addEventListener('keydown', function (ev) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      return;
    }

    if (ev.key === 'Enter') {
      ws.send(line + '\r');
      line = '';
      ev.preventDefault();
      return;
    }

    if (ev.key === 'Backspace') {
      if (line.length > 0) {
        line = line.slice(0, -1);
      }
      ev.preventDefault();
      return;
    }

    if (ev.key.length === 1 && !ev.ctrlKey && !ev.metaKey) {
      line += ev.key;
      ev.preventDefault();
    }
  });

  connect();
})();
