# Cross-compile toolchain for R36S (RK3326 aarch64 / Ubuntu 19.10 / GLIBC 2.30)
#
# Prerequisites on Ubuntu-20.04 host:
#   sudo dpkg --add-architecture arm64
#   # Add arm64 apt source (see setup_wsl_focal.sh)
#   sudo apt install crossbuild-essential-arm64
#   sudo apt install libsdl2-dev:arm64
#
# Usage:
#   cmake -B build_aarch64 -S . \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-r36s-aarch64.cmake \
#         -DCMAKE_BUILD_TYPE=Release \
#         -DR36S_BUILD=ON
#   cmake --build build_aarch64 -j$(nproc)

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_STRIP        aarch64-linux-gnu-strip)

# Ubuntu multiarch: cross libs live under /usr/aarch64-linux-gnu/
# and /usr/lib/aarch64-linux-gnu/ — no separate sysroot needed.
set(CMAKE_FIND_ROOT_PATH  /usr/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Tell pkg-config where to find arm64 .pc files
set(ENV{PKG_CONFIG_LIBDIR}  "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
