cmake_minimum_required(VERSION 3.16)
project(RHYTHMY VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(R36S_BUILD "Target the R36S handheld (Linux/ARM, Nintendo button layout)" OFF)

# ── SDL2 ──────────────────────────────────────────────────────────────────────
if(WIN32)
    set(SDL2_DIR "${CMAKE_SOURCE_DIR}/lib/SDL2-2.30.10/cmake")
    find_package(SDL2 REQUIRED CONFIG)
    set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/lib/SDL2-2.30.10/include")
    set(SDL2_LINK_LIBS SDL2::SDL2main SDL2::SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    set(SDL2_LINK_LIBS ${SDL2_LIBRARIES})
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "src/*.cpp")
add_executable(rhythmy ${SRC})

target_include_directories(rhythmy PRIVATE
    src
    third_party
    ${SDL2_INCLUDE_DIRS}
)

if(NOT WIN32)
    target_link_directories(rhythmy PRIVATE ${SDL2_LIBRARY_DIRS})
endif()

target_link_libraries(rhythmy PRIVATE ${SDL2_LINK_LIBS})

# ── Compiler / linker flags ───────────────────────────────────────────────────
if(MSVC)
    target_compile_options(rhythmy PRIVATE /W3 /wd4244 /wd4267 /wd4996)
    set_target_properties(rhythmy PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
else()
    target_compile_options(rhythmy PRIVATE
        -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -O2
    )
    # miniaudio requires dl + pthread + m on Linux
    target_link_libraries(rhythmy PRIVATE dl pthread m)
    # std::filesystem lives in a separate lib on GCC < 9
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
       CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
        target_link_libraries(rhythmy PRIVATE stdc++fs)
    endif()
    # Static-embed C++ runtime when cross-compiling (device won't have host GCC libs)
    if(R36S_BUILD AND CMAKE_CROSSCOMPILING)
        target_link_options(rhythmy PRIVATE
            -static-libstdc++ -static-libgcc
            # libSDL2.so on the device pulls in alsa/pulse/X11/wayland itself.
            # Those aren't in our sysroot, so let the linker leave SDL2's own
            # shared-lib deps unresolved — they bind at runtime on the device.
            -Wl,--allow-shlib-undefined
            -Wl,--unresolved-symbols=ignore-in-shared-libs
        )
    endif()

    # Cortex-A35 (RK3326) optimisation flags when cross-compiling for R36S
    if(R36S_BUILD AND CMAKE_CROSSCOMPILING)
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            # ARM64 native build: Cortex-A35 is ARMv8.0-A
            target_compile_options(rhythmy PRIVATE -march=armv8-a)
        else()
            # ARM32 (armhf) build
            target_compile_options(rhythmy PRIVATE
                -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard
            )
        endif()
    endif()
endif()

# ── Platform definition ───────────────────────────────────────────────────────
if(R36S_BUILD)
    target_compile_definitions(rhythmy PRIVATE R36S_BUILD)
endif()

# ── Post-build: copy SDL2.dll + assets next to the exe on Windows ────────────
if(WIN32)
    add_custom_command(TARGET rhythmy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/lib/SDL2-2.30.10/lib/x64/SDL2.dll"
            $<TARGET_FILE_DIR:rhythmy>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:rhythmy>/assets"
        VERBATIM
    )
endif()
