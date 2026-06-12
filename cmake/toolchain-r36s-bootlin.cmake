# Cross-compile toolchain for R36S using the Bootlin aarch64 toolchain.
#   GCC 8.4.0 + glibc 2.30 + matching libstdc++/crt  ==> binary needs GLIBC <= 2.30
# The device (Ubuntu 19.10 / GLIBC 2.30) can then run it.
#
# Toolchain root resolved from $BOOTLIN_ROOT, else ~/toolchains/<name>.
#
# Build:
#   cmake -B build -S . \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-r36s-bootlin.cmake \
#         -DCMAKE_BUILD_TYPE=Release -DR36S_BUILD=ON

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(DEFINED ENV{BOOTLIN_ROOT})
    set(TC "$ENV{BOOTLIN_ROOT}")
else()
    set(TC "$ENV{HOME}/toolchains/aarch64--glibc--stable-2020.02-2")
endif()

set(CMAKE_SYSROOT "${TC}/aarch64-buildroot-linux-gnu/sysroot")

set(CMAKE_C_COMPILER   "${TC}/bin/aarch64-linux-gcc")
set(CMAKE_CXX_COMPILER "${TC}/bin/aarch64-linux-g++")
set(CMAKE_STRIP        "${TC}/bin/aarch64-linux-strip")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# SDL2 (2.0.9) headers/lib were copied into the Bootlin sysroot's multiarch dir.
set(ENV{PKG_CONFIG_LIBDIR}
    "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
