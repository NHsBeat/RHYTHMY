#!/bin/bash
# One-time setup in Ubuntu-22.04 WSL.
# Usage: bash /mnt/c/Users/Haz/Desktop/projects/RHYTHMY/setup_wsl_jammy.sh
set -e

echo "=== [1/5] Installing host tools ==="
sudo apt-get update -qq
sudo apt-get install -y \
    cmake \
    debootstrap \
    qemu-user-static \
    binfmt-support \
    crossbuild-essential-arm64 \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu

echo ""
echo "=== [2/5] Creating Debian Buster arm64 sysroot (this takes ~5 min) ==="
sudo debootstrap --arch=arm64 buster /opt/buster-arm64 http://archive.debian.org/debian

echo ""
echo "=== [3/5] Setting up QEMU and apt sources in sysroot ==="
sudo cp /usr/bin/qemu-aarch64-static /opt/buster-arm64/usr/bin/

printf 'deb http://archive.debian.org/debian buster main\n' | \
    sudo tee /opt/buster-arm64/etc/apt/sources.list > /dev/null

echo ""
echo "=== [4/5] Installing SDL2 inside sysroot ==="
sudo chroot /opt/buster-arm64 /usr/bin/qemu-aarch64-static /bin/bash -c "
    apt-get update -qq 2>&1 | tail -3
    apt-get install -y --no-install-recommends libsdl2-dev pkg-config 2>&1 | tail -5
"

echo ""
echo "=== [5/5] Verifying ==="
aarch64-linux-gnu-gcc --version | head -1
cmake --version | head -1
ls /opt/buster-arm64/usr/lib/aarch64-linux-gnu/libSDL2* 2>/dev/null | head -3

echo ""
echo "=== Setup complete! Now run: ==="
echo "  bash /mnt/c/Users/Haz/Desktop/projects/RHYTHMY/build_aarch64.sh"
