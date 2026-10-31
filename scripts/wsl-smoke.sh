#!/usr/bin/env bash
# Smoke-test the freshly built binary. Nothing here needs a real chip:
# every case exercises argument handling, the device list or the error paths.
#
#   scripts/wsl-smoke.sh [lang]      default zh-CN
#
# The binary path honours WSL_BUILD_DIR the same way wsl-build.sh does.

set -u

LANG_CHOICE="${1:-zh-CN}"
BIN="${WSL_BUILD_DIR:-$HOME/newisp-build}/$(printf '%s' "$LANG_CHOICE" | tr '[:upper:]' '[:lower:]')/newisp"

if [ ! -x "$BIN" ]; then
    echo "binary not found at $BIN -- run scripts/wsl-build.sh first" >&2
    exit 1
fi

run() {
    echo "=============================================================="
    echo "\$ newisp $*"
    echo "--------------------------------------------------------------"
    "$BIN" "$@"
    rc=$?
    echo "--- exit code: $rc ---"
    echo
    return 0
}

run -version
run -help
run list
run detect -device /dev/nonexistent-tty

# Translation, in both spellings, all of which must be accepted.
run detect -s -device /dev/nonexistent-tty
run detect -serial -device /dev/nonexistent-tty
run detect -SERIAL -device /dev/nonexistent-tty

# detect only identifies a chip, so programming options are refused there.
run detect -F 24M -device /dev/nonexistent-tty

run burn
run burn -f /nonexistent.hex -device /dev/nonexistent-tty
run burn -f /nonexistent.hex -baud notanumber
run badcommand

echo "=============================================================="
echo "all smoke cases executed"
