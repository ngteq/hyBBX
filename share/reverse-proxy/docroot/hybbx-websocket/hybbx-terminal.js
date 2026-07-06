(function () {
  'use strict';

  var term = document.getElementById('term');
  var status = document.getElementById('status');
  var form = document.getElementById('input-bar');
  var input = document.getElementById('cmd');
  var wsUrl = window.HYBBX_WS_URL;
  var ws = null;

  function append(text) {
    term.textContent += text;
    term.scrollTop = term.scrollHeight;
  }

  function setStatus(s) {
    status.textContent = s;
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
      setStatus('connected');
      setInputEnabled(true);
    };

    ws.onmessage = function (ev) {
      append(ev.data);
    };

    ws.onclose = function () {
      setStatus('disconnected');
      ws = null;
      setInputEnabled(false);
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
