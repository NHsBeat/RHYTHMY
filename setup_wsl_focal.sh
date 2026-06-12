#!/bin/bash
# Run this inside Ubuntu-20.04 WSL to set up aarch64 cross-compile environment.
# Usage: bash /mnt/c/Users/Haz/Desktop/projects/RHYTHMY/setup_wsl_focal.sh
set -e

echo "=== Step 1: Add arm64 multiarch ==="
sudo dpkg --add-architecture arm64

echo "=== Step 2: Fix apt sources for arm64 ==="
# Restrict existing sources to amd64 only
sudo sed -i 's/^deb /deb [arch=amd64] /' /etc/apt/sources.list
sudo sed -i 's/^deb \[arch=amd64\] \[/deb [arch=amd64,/' /etc/apt/sources.list

# Add arm64 from ports.ubuntu.com
sudo tee /etc/apt/sources.list.d/arm64-ports.list > /dev/null << 'EOF'
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal           main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal-updates   main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal-security  main restricted universe multiverse
EOF

echo "=== Step 3: Install packages ==="
sudo apt update
sudo apt install -y \
    cmake \
    crossbuild-essential-arm64 \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libsdl2-dev:arm64 \
    pkg-config

echo "=== Versions ==="
aarch64-linux-gnu-gcc --version | head -1
cmake --version | head -1
echo "SDL2:"
dpkg -l libsdl2-dev:arm64 | grep "^ii" | awk '{print $3}'

echo ""
echo "=== Setup complete! Now run: ==="
echo "  bash /mnt/c/Users/Haz/Desktop/projects/RHYTHMY/build_aarch64.sh"
