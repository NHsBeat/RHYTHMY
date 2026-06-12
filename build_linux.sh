#!/usr/bin/env bash
# RHYTHMY — Linux / R36S build script
# Run from the project root:
#   bash build_linux.sh          # native Linux build (e.g. desktop testing)
#   bash build_linux.sh r36s     # native Linux build with R36S flags (build ON the device)
#   bash build_linux.sh cross    # cross-compile from Linux host → ARM R36S

set -e
MODE=${1:-native}

case "$MODE" in
  cross)
    echo "=== Cross-compiling for R36S (arm-linux-gnueabihf) ==="
    echo "    Needs: gcc-arm-linux-gnueabihf + libsdl2-dev:armhf in sysroot"
    cmake -B build_r36s -S . \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-r36s.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DR36S_BUILD=ON
    cmake --build build_r36s -j"$(nproc)"
    arm-linux-gnueabihf-strip build_r36s/rhythmy
    echo ""
    echo "Binary: build_r36s/rhythmy"
    echo ""
    echo "Next step — build the self-extracting installer:"
    echo "  bash packaging/make_installer.sh"
    echo "  → produces Install_RHYTHMY.sh (upload this to your website)"
    ;;
  r36s)
    echo "=== Native R36S build (run this ON the device or a matching Linux) ==="
    cmake -B build_r36s_native -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DR36S_BUILD=ON
    cmake --build build_r36s_native -j"$(nproc)"
    strip build_r36s_native/rhythmy
    echo ""
    echo "Binary: build_r36s_native/rhythmy"
    ;;
  *)
    echo "=== Native Linux build (desktop, no R36S flags) ==="
    cmake -B build_linux -S . \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build build_linux -j"$(nproc)"
    echo ""
    echo "Binary: build_linux/rhythmy"
    ;;
esac

echo "Done."
