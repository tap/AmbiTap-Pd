#!/usr/bin/env bash
# Headless load smoke test: launch Pd with no GUI/audio, load the ambitap
# library, instantiate every ambitap.*~ object (tests/smoke.pd), turn DSP on,
# and quit. Fails if the library doesn't load, if any object can't be created,
# or if Pd crashes/hangs. Complements the build check: proves the externals
# actually instantiate and compile their DSP, not just that they link.
#
# Needs `pd` on PATH (Pd >= 0.54 for the multichannel API). The external is
# taken from the first argument or ./externals.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
ext="${1:-$root/externals}"
patch="$here/smoke.pd"

command -v pd >/dev/null || { echo "::error::pd not found on PATH"; exit 1; }
[ -e "$ext"/ambitap.pd_* ] || [ -e "$ext"/ambitap.dll ] || {
  echo "::error::no ambitap external in $ext"; exit 1; }

log="$(mktemp)"
rc=0
# The patch enables DSP and then quits itself (loadbang -> pd dsp 1 -> pd quit);
# timeout is only a backstop against a hang.
timeout 60 pd -nogui -noprefs -stderr -noaudio \
  -path "$ext" -lib ambitap -open "$patch" > "$log" 2>&1 || rc=$?

echo "----- pd output -----"
cat "$log"
echo "---------------------"

# The library must have actually loaded (its setup banner).
grep -q "higher-order ambisonics externals" "$log" \
  || { echo "::error::ambitap library did not load (no setup banner)"; exit 1; }

# Any object that failed to instantiate.
if grep -iE "couldn't create|can't load|couldn't open" "$log"; then
  echo "::error::one or more ambitap objects failed to load"
  exit 1
fi

# A clean run quits via [; pd quit( and returns 0; anything else is a crash or
# a hang the timeout killed.
if [ "$rc" -ne 0 ]; then
  echo "::error::pd exited abnormally (code $rc) — crash or hang during load/DSP"
  exit 1
fi

echo "load smoke test OK: library loaded, all objects instantiated, DSP compiled, clean exit"
