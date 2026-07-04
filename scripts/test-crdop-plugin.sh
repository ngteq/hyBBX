#!/usr/bin/env bash
# Smoke test: mock ARDOPC + HyBBX crdop plugin (CB defaults, no Main circuit required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"
HYBBX="${BUILD}/src/hybbx"
MOCK_PORT=18517
TMPDIR="${ROOT}/local/test-crdop-$$"
mkdir -p "${TMPDIR}"

cleanup() {
    kill "${MOCK_PID:-}" 2>/dev/null || true
    kill "${CIRCUIT_PID:-}" 2>/dev/null || true
    kill "${HYBBX_PID:-}" 2>/dev/null || true
    rm -rf "${TMPDIR}"
}
trap cleanup EXIT

if [[ ! -x "${HYBBX}" ]]; then
    echo "Build hybbx first: cmake --build build" >&2
    exit 1
fi

cat > "${TMPDIR}/hybbx.ini" <<EOF
[service]
name = test-crdop

[storage]
path = ${TMPDIR}/data

[auth]
auto_login = no

[networks]
ax25 = no
ardop = no
crdop = yes
circuit = no

[transport.telnet]
enabled = no

[transport.crdop]
enabled = yes
modem_host = 127.0.0.1
modem_port = ${MOCK_PORT}
mycall = CB-0
listen = yes
circuit_host = 127.0.0.1
circuit_port = 17325
link_id = test-crdop
EOF

python3 "${ROOT}/scripts/mock-ardopc.py" "${MOCK_PORT}" &
MOCK_PID=$!
sleep 0.3

nc -l 127.0.0.1 17325 >/dev/null 2>&1 &
CIRCUIT_PID=$!
sleep 0.2

LOG="${TMPDIR}/hybbx.log"
stdbuf -oL timeout 5 "${HYBBX}" -c "${TMPDIR}/hybbx.ini" >"${LOG}" 2>&1 &
HYBBX_PID=$!
sleep 3
kill "${HYBBX_PID}" 2>/dev/null || true
wait "${HYBBX_PID}" 2>/dev/null || true
kill "${CIRCUIT_PID}" 2>/dev/null || true

grep -q '\[crdop\] connected to external CRDOPC' "${LOG}" || {
    echo "FAIL: no CRDOP CRDOPC connect log" >&2
    cat "${LOG}" >&2
    exit 1
}
grep -q '\[crdop\] modem VERSION' "${LOG}" || {
    echo "FAIL: no CRDOP VERSION log" >&2
    cat "${LOG}" >&2
    exit 1
}
grep -q '\[crdop\] host init sent' "${LOG}" || {
    echo "FAIL: no CRDOP host init log" >&2
    cat "${LOG}" >&2
    exit 1
}
grep -q '\[crdop\] CB half-duplex profile' "${LOG}" || {
    echo "FAIL: no CRDOP CB profile log" >&2
    cat "${LOG}" >&2
    exit 1
}
grep -q 'profile=cb' "${LOG}" || {
    echo "FAIL: crdop profile not cb" >&2
    cat "${LOG}" >&2
    exit 1
}

echo "OK: crdop plugin smoke test passed"
grep '\[crdop\]' "${LOG}"
