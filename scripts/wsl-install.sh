#!/usr/bin/env bash
# Build newisp somewhere the interactive WSL user can run it.
#
# The project's own wsl-build.sh installs under $HOME, which is correct for a
# developer whose shell owns that home directory. When the build is driven from
# Windows (wsl -d ... -- bash ...) the shell may run as a different user, and
# the binary ends up in a home the interactive login cannot read.
#
# This script therefore builds into a world-readable location and prints the
# exact command to run, so the output can be pasted straight into the user's own
# terminal.
set -euo pipefail

SRC=/mnt/c/newisp-cli
LANG_CHOICE="${1:-zh-CN}"
OUT_ROOT=/opt/newisp
BUILD="$OUT_ROOT/build-$(printf '%s' "$LANG_CHOICE" | tr '[:upper:]' '[:lower:]')"
BIN="$OUT_ROOT/newisp"

echo "=== configure (NEWISP_LANG=$LANG_CHOICE) ==="
cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DNEWISP_LANG="$LANG_CHOICE" \
    -DNEWISP_BUILD_TESTS=ON 2>&1 | tail -8

echo
echo "=== build ==="
cmake --build "$BUILD" -j"$(nproc)" 2>&1 | grep -E "error|Built target|Linking" | tail -8

echo
echo "=== install to $BIN ==="
install -m 0755 "$BUILD/newisp" "$BIN"
ls -l "$BIN"

echo
echo "run it with:"
echo "    $BIN"
