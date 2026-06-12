#!/bin/bash
# Add libc6-dev + SDL2 (arm64, Debian Buster / GLIBC 2.28) into the sysroot
# by extracting .deb packages directly — NO qemu/chroot needed.
# Run in Ubuntu-22.04 WSL:
#   bash /mnt/c/Users/Haz/Desktop/projects/RHYTHMY/install_sdl_sysroot.sh
set -e

SYSROOT=/opt/buster-arm64
BASEURL=http://archive.debian.org/debian
WORK=/tmp/sdlsysroot

echo "=== Caching sudo credentials (enter 'build' password if asked) ==="
sudo -v

mkdir -p "$WORK"
cd "$WORK"

echo "=== Fetching arm64 package index ==="
wget -qO Packages.gz "$BASEURL/dists/buster/main/binary-arm64/Packages.gz"
gunzip -kf Packages.gz

fetch() {
    local pkg="$1"
    local fn base
    fn=$(awk -v p="$pkg" '$1=="Package:"{c=$2} c==p && $1=="Filename:"{print $2; exit}' Packages)
    if [ -z "$fn" ]; then
        echo "ERROR: package '$pkg' not found in index"; exit 1
    fi
    base=$(basename "$fn")
    echo "  -> $pkg ($base)"
    wget -qO "$base" "$BASEURL/$fn"
    sudo dpkg-deb -x "$base" "$SYSROOT"
}

echo "=== Downloading + extracting packages into sysroot ==="
fetch libc6-dev
fetch linux-libc-dev
fetch libsdl2-2.0-0
fetch libsdl2-dev

echo ""
echo "=== Verifying ==="
ok=1
check() { if [ -e "$1" ]; then echo "  OK   $1"; else echo "  MISS $1"; ok=0; fi; }
check "$SYSROOT/usr/lib/aarch64-linux-gnu/libc.so"
check "$SYSROOT/usr/lib/aarch64-linux-gnu/libSDL2.so"
check "$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig/sdl2.pc"
check "$SYSROOT/usr/include/SDL2/SDL.h"

echo ""
if [ "$ok" = "1" ]; then
    echo "=== SYSROOT READY. Now run: ==="
    echo "  bash /mnt/c/Users/Haz/Desktop/projects/RHYTHMY/build_aarch64.sh"
else
    echo "=== Some files missing — tell Claude ==="
fi
