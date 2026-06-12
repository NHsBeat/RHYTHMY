#!/bin/bash
# Build RHYTHMY for R36S using the Bootlin GCC 8.4 / glibc 2.30 toolchain.
set -e

PROJECT="/mnt/c/Users/Haz/Desktop/projects/RHYTHMY"
BUILD_DIR="$HOME/rhythmy_build_bootlin"
OUT_DIR="$PROJECT/build_out"
TC="$HOME/toolchains/aarch64--glibc--stable-2020.02-2"
SYS="$TC/aarch64-buildroot-linux-gnu/sysroot"
BUSTER=/opt/buster-arm64

# --- 1. Make sure SDL2 (2.0.9) is present inside the Bootlin sysroot ---
if [ ! -e "$SYS/usr/include/SDL2/SDL.h" ]; then
    echo "=== Copying SDL2 2.0.9 into Bootlin sysroot ==="
    mkdir -p "$SYS/usr/include" "$SYS/usr/lib/aarch64-linux-gnu/pkgconfig"
    cp -a "$BUSTER/usr/include/SDL2"                              "$SYS/usr/include/"
    cp -a "$BUSTER"/usr/lib/aarch64-linux-gnu/libSDL2*.so*        "$SYS/usr/lib/aarch64-linux-gnu/"
    cp -a "$BUSTER/usr/lib/aarch64-linux-gnu/pkgconfig/sdl2.pc"   "$SYS/usr/lib/aarch64-linux-gnu/pkgconfig/"
fi
echo "SDL2 header: $(ls "$SYS/usr/include/SDL2/SDL.h")"
echo "SDL2 lib:    $(ls "$SYS"/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0)"

# --- 2. Configure + build ---
echo "=== Building RHYTHMY (Bootlin GCC 8.4 / glibc 2.30) ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT" \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT/cmake/toolchain-r36s-bootlin.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DR36S_BUILD=ON

cmake --build . -j$(nproc)

# --- 3. Report GLIBC / runtime deps ---
echo ""
echo "=================================================================="
echo " GLIBC VERSIONS REQUIRED (must ALL be <= 2.30):"
echo "=================================================================="
nm -D "$BUILD_DIR/rhythmy" 2>/dev/null | grep -o "GLIBC_[0-9.]*" | sort -V | uniq
echo "------------------------------------------------------------------"
echo " Anything > 2.30 (should be EMPTY):"
nm -D "$BUILD_DIR/rhythmy" 2>/dev/null | grep -E "GLIBC_2\.(31|32|33|34|35|36|37|38|39|4[0-9])" | sort -u || true
echo "------------------------------------------------------------------"
echo " NEEDED shared libraries:"
"$TC/bin/aarch64-linux-objdump" -p "$BUILD_DIR/rhythmy" 2>/dev/null | grep NEEDED
echo "=================================================================="
echo ""

# --- 4. Strip + deploy ---
"$TC/bin/aarch64-linux-strip" "$BUILD_DIR/rhythmy"
ls -lh "$BUILD_DIR/rhythmy"
file "$BUILD_DIR/rhythmy"

mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/rhythmy" "$OUT_DIR/rhythmy"
echo ""
echo "=== Binary copied to: $OUT_DIR/rhythmy ==="
if [ -d /mnt/f/ports/RHYTHMY ]; then
    cp "$BUILD_DIR/rhythmy" /mnt/f/ports/RHYTHMY/rhythmy
    sync
    echo "=== Also copied to SD card ==="
fi
echo "DONE."
