# AmigaOS cross-compilation toolchain file (template).
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-AmigaOS.cmake ..
#
# Set AMIGA_SDK_PATH to your AmigaOS 3.x SDK / cross-compiler prefix.

set(CMAKE_SYSTEM_NAME AmigaOS)
set(CMAKE_SYSTEM_PROCESSOR m68k)

if(NOT DEFINED AMIGA_SDK_PATH)
    message(FATAL_ERROR
        "Set AMIGA_SDK_PATH to your AmigaOS cross-compiler prefix, e.g.\n"
        "  cmake -DAMIGA_SDK_PATH=/opt/amiga -DCMAKE_TOOLCHAIN_FILE=...")
endif()

set(CMAKE_C_COMPILER   "${AMIGA_SDK_PATH}/bin/m68k-amigaos-gcc")
set(CMAKE_AR           "${AMIGA_SDK_PATH}/bin/m68k-amigaos-ar")
set(CMAKE_RANLIB       "${AMIGA_SDK_PATH}/bin/m68k-amigaos-ranlib")

set(CMAKE_FIND_ROOT_PATH "${AMIGA_SDK_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
