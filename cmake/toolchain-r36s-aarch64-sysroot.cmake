# Cross-compile toolchain for R36S (aarch64 / Ubuntu 19.10 / GLIBC 2.30)
# Uses a Debian Buster arm64 sysroot (GLIBC 2.28) so the binary runs on the device.
#
# One-time sysroot setup (run in Ubuntu-22.04 WSL):
#   sudo apt install -y debootstrap qemu-user-static binfmt-support
#   sudo debootstrap --arch=arm64 buster /opt/buster-arm64 http://archive.debian.org/debian
#   sudo cp /usr/bin/qemu-aarch64-static /opt/buster-arm64/usr/bin/
#   printf 'deb http://archive.debian.org/debian buster main\n' | \
#       sudo tee /opt/buster-arm64/etc/apt/sources.list
#   sudo chroot /opt/buster-arm64 apt-get update -qq
#   sudo chroot /opt/buster-arm64 apt-get install -y libsdl2-dev pkg-config
#
# Build:
#   cmake -B build_aarch64 -S . \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-r36s-aarch64-sysroot.cmake \
#         -DCMAKE_BUILD_TYPE=Release -DR36S_BUILD=ON
#   cmake --build build_aarch64 -j$(nproc)

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSROOT          /opt/buster-arm64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_STRIP        aarch64-linux-gnu-strip)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config must search inside the sysroot only
set(ENV{PKG_CONFIG_LIBDIR}      "/opt/buster-arm64/usr/lib/aarch64-linux-gnu/pkgconfig:/opt/buster-arm64/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "/opt/buster-arm64")
