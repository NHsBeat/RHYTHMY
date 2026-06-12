#!/bin/bash
# RHYTHMY v1.0 - Debug launcher
GAMEDIR="$(dirname "$(readlink -f "$0")")/RHYTHMY"
cd "$GAMEDIR"

# Write all output to log file so we can read it from Windows
exec > "$GAMEDIR/launch.log" 2>&1

echo "=== RHYTHMY Launch Log ==="
echo "Date: $(date)"
echo "GAMEDIR: $GAMEDIR"
echo ""

# Check binary
echo "--- Binary ---"
ls -la ./rhythmy
file ./rhythmy 2>/dev/null
echo ""

# Check SDL2 on device
echo "--- SDL2 libraries ---"
find /usr /lib /opt -name "libSDL2*.so*" 2>/dev/null
echo ""

# Check all dependencies
echo "--- Dependencies (ldd) ---"
ldd ./rhythmy 2>&1
echo ""

# Try launch
echo "--- Launch ---"
export SDL_AUDIODRIVER=alsa
export AUDIODEV=default
./rhythmy
echo "Exit code: $?"
