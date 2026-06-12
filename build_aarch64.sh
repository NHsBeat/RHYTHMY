#!/bin/bash
# Build RHYTHMY for R36S (aarch64 / Ubuntu 19.10 / GLIBC 2.30)
# Run from Ubuntu-22.04 WSL AFTER install_sdl_sysroot.sh
set -e

PROJECT="/mnt/c/Users/Haz/Desktop/projects/RHYTHMY"
# Build on the Linux filesystem — /mnt/c (NTFS) breaks CMake's chmod/configure_file
BUILD_DIR="$HOME/rhythmy_build_aarch64"
OUT_DIR="$PROJECT/build_out"

# pkg-config is required by CMake's FindPkgConfig
if ! command -v pkg-config >/dev/null 2>&1; then
    echo "=== Installing pkg-config (enter 'build' password if asked) ==="
    sudo apt-get install -y pkg-config
fi

echo "=== Building RHYTHMY for aarch64 (Debian Buster sysroot, GLIBC 2.28) ==="

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT" \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT/cmake/toolchain-r36s-aarch64-sysroot.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DR36S_BUILD=ON

cmake --build . -j$(nproc)

echo ""
echo "=================================================================="
echo " GLIBC VERSIONS REQUIRED BY BINARY (must ALL be <= 2.30):"
echo "=================================================================="
nm -D "$BUILD_DIR/rhythmy" 2>/dev/null \
    | grep -o "GLIBC_[0-9.]*" \
    | sort -V | uniq
echo "=================================================================="
echo " GLIBCXX/CXXABI required (static libstdc++ should mean NONE):"
nm -D "$BUILD_DIR/rhythmy" 2>/dev/null \
    | grep -oE "GLIBCXX_[0-9.]*|CXXABI_[0-9.]*" \
    | sort -V | uniq || echo "  (none)"
echo "=================================================================="
echo " Shared libraries needed at runtime (NEEDED):"
objdump -p "$BUILD_DIR/rhythmy" 2>/dev/null | grep NEEDED
echo "=================================================================="
echo ""

echo "=== Stripping binary ==="
aarch64-linux-gnu-strip "$BUILD_DIR/rhythmy"
ls -lh "$BUILD_DIR/rhythmy"
file "$BUILD_DIR/rhythmy"

# Copy stripped binary back to Windows project folder so it can be deployed
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/rhythmy" "$OUT_DIR/rhythmy"
echo ""
echo "=== Binary copied to: $OUT_DIR/rhythmy ==="

# Also copy straight to SD card if it's mounted
if [ -d /mnt/f/ports/RHYTHMY ]; then
    cp "$BUILD_DIR/rhythmy" /mnt/f/ports/RHYTHMY/rhythmy
    sync
    echo "=== Also copied to SD card: /mnt/f/ports/RHYTHMY/rhythmy ==="
fi
echo "DONE."
