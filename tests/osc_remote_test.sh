#!/usr/bin/env bash
# Headless end-to-end test of the ambitap.ui-remote abstraction: launch Pd
# with tests/osc_remote.pd (which prints each outlet and self-quits after
# 3 s), fire real binary OSC datagrams at it — the same messages the browser
# widgets emit through ui/scripts/osc-bridge.mjs — and assert the routed
# ambitap.*~ messages came out of the right outlets with the right values.
#
# Needs `pd` on PATH and python3 (the OSC sender). Pure vanilla receive path:
# netreceive -u -b + oscparse (Pd >= 0.46).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"

command -v pd >/dev/null || { echo "::error::pd not found on PATH"; exit 1; }
command -v python3 >/dev/null || { echo "::error::python3 not found"; exit 1; }

log="$(mktemp)"
timeout 30 pd -nogui -noprefs -stderr -noaudio \
  -path "$root/abstractions" -open "$here/osc_remote.pd" > "$log" 2>&1 &
pd_pid=$!

sleep 1 # let netreceive bind

python3 - << 'EOF'
import socket, struct

def osc(address, floats):
    def pad(b):
        return b + b"\0" * (4 - len(b) % 4)
    msg = pad(address.encode())
    msg += pad(b"," + b"f" * len(floats))
    for f in floats:
        msg += struct.pack(">f", f)
    return msg

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
for _ in range(3):  # a small burst: UDP offers no delivery guarantee
    s.sendto(osc("/ambitap/source/1/direction", [1.5708, 0.25]), ("127.0.0.1", 7501))
    s.sendto(osc("/ambitap/source/2/direction", [-0.7854, 0.0]), ("127.0.0.1", 7501))
    s.sendto(osc("/ambitap/orientation", [-1.5708, 0.1, 0.05]), ("127.0.0.1", 7501))
EOF

wait "$pd_pid" || { echo "::error::pd exited abnormally"; cat "$log"; exit 1; }

echo "----- pd output -----"
cat "$log"
echo "---------------------"

fail=0
expect() {
  grep -qF "$1" "$log" || { echo "::error::missing: $1"; fail=1; }
}
expect "src1: azimuth 1.5708"
expect "src1: elevation 0.25"
expect "src2: azimuth -0.7854"
expect "src2: elevation 0"
expect "orient: yaw -1.5708"
expect "orient: pitch 0.1"
expect "orient: roll 0.05"

[ "$fail" -eq 0 ] || exit 1
echo "osc remote test OK: browser-format OSC routed to per-source and orientation messages"
