#!/usr/bin/env bash
# Configure and build newisp on Linux from inside WSL.
#
# The tree lives on the Windows filesystem, which is visible under /mnt/c but is
# slow for builds and does not preserve the executable bit. The build therefore
# happens in a directory under $HOME and only the source is read from the mount.
#
#   scripts/wsl-build.sh            build the default language (zh-CN)
#   scripts/wsl-build.sh en         build the English variant
#
# The language is a compile-time choice (see NEWISP_LANG in CMakeLists.txt), so
# each language gets its own build directory.
set -euo pipefail

SRC="${NEWISP_SRC:-/mnt/c/newisp-cli}"
LANG_CHOICE="${1:-zh-CN}"
BUILD="${WSL_BUILD_DIR:-$HOME/newisp-build}/$(printf '%s' "$LANG_CHOICE" | tr '[:upper:]' '[:lower:]')"

echo "=== configure (NEWISP_LANG=$LANG_CHOICE) ==="
cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DNEWISP_LANG="$LANG_CHOICE" \
    -DNEWISP_BUILD_TESTS=ON 2>&1 | tail -12

echo
echo "=== build ==="
cmake --build "$BUILD" -j"$(nproc)" 2>&1 | tail -40

echo
echo "=== result ==="
ls -l "$BUILD/newisp" 2>/dev/null || echo "binary NOT produced"
