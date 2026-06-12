# Cross-compile toolchain for R36S (RK3326, armhf/ARMv7-A)
#
# Prerequisites on Ubuntu/Debian host:
#   sudo dpkg --add-architecture armhf
#   sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
#   sudo apt install libsdl2-dev:armhf
#
# Usage:
#   cmake -B build_r36s -S . \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-r36s.cmake \
#         -DCMAKE_BUILD_TYPE=Release \
#         -DR36S_BUILD=ON
#   cmake --build build_r36s -j$(nproc)

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_STRIP        arm-linux-gnueabihf-strip)

# On Ubuntu multiarch the cross libs live under /usr/arm-linux-gnueabihf/
# and /usr/lib/arm-linux-gnueabihf/ — no separate sysroot needed.
# Setting CMAKE_SYSROOT causes double-path problems with linker scripts.
set(CMAKE_FIND_ROOT_PATH  /usr/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Tell pkg-config where to find armhf .pc files
set(ENV{PKG_CONFIG_LIBDIR}  "/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
