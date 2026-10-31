#!/usr/bin/env bash
# Report the WSL build environment for newisp.
#
# Kept as a script rather than an inline command so the quoting survives the
# trip through Windows PowerShell into bash.

set -u

echo "=== distribution ==="
. /etc/os-release 2>/dev/null && echo "$PRETTY_NAME" || echo "unknown"

echo
echo "=== toolchain ==="
for tool in cmake g++ gcc make pkg-config git; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '%-12s %s\n' "$tool" "$("$tool" --version 2>&1 | head -1)"
    else
        printf '%-12s MISSING\n' "$tool"
    fi
done

echo
echo "=== hidapi (required: the 8051U family has no serial ISP) ==="
if pkg-config --exists hidapi-hidraw 2>/dev/null; then
    printf 'hidapi-hidraw %s\n' "$(pkg-config --modversion hidapi-hidraw)"
elif pkg-config --exists hidapi 2>/dev/null; then
    printf 'hidapi        %s\n' "$(pkg-config --modversion hidapi)"
elif [ -f /usr/include/hidapi/hidapi.h ]; then
    echo "header present, pkg-config entry missing"
else
    echo "MISSING"
fi

echo
echo "=== cmake version (needs 3.20 or newer) ==="
cmake --version 2>/dev/null | head -1 || echo "cmake MISSING"

echo
echo "=== source tree ==="
cd /mnt/c/newisp-cli 2>/dev/null || { echo "repository not visible at /mnt/c/newisp-cli"; exit 1; }
echo "path: $(pwd)"
echo "top level:"
ls -1 | head -20

echo
echo "=== USB passthrough state ==="
if command -v usbipd.exe >/dev/null 2>&1 || [ -x /mnt/c/Program\ Files/usbipd-win/usbipd.exe ]; then
    echo "usbipd-win present on the Windows side"
else
    echo "usbipd-win not found"
fi
