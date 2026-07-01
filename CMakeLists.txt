cmake_minimum_required(VERSION 3.16)

# Primary toolchain: GCC when CC / CMAKE_C_COMPILER are not set.
if(NOT DEFINED ENV{CC} AND NOT CMAKE_C_COMPILER)
    find_program(HYBBX_PREFERRED_GCC NAMES gcc)
    if(HYBBX_PREFERRED_GCC)
        set(CMAKE_C_COMPILER "${HYBBX_PREFERRED_GCC}"
            CACHE FILEPATH "HyBBX primary C compiler (GCC)" FORCE)
    endif()
    unset(HYBBX_PREFERRED_GCC)
endif()

project(HyBBX
    VERSION 0.1.0
    DESCRIPTION "Centralized daemon with link/repeater technologies for multiple connection types"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CMakeDependentOption)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(HYBBX_BUILD_DAEMON         "Build hybbx server daemon"         ON)
option(HYBBX_BUILD_CLIENTS        "Build standalone CLI clients"        ON)
option(HYBBX_CLIENTS_ONLY         "Clients only (no daemon, no plugins)" OFF)
option(HYBBX_BUILD_PLUGINS      "Build transport plugins"           ON)
option(HYBBX_PLUGIN_TELNET      "Build TCP/IP telnet transport"     ON)
option(HYBBX_PLUGIN_PACKET_RADIO "Build packet radio transport"     ON)
option(HYBBX_BUILD_TESTS        "Build unit tests"                  OFF)
option(HYBBX_HARDENING          "Enable compiler security hardening" ON)
option(HYBBX_WARNINGS_AS_ERRORS "Treat warnings as errors"           OFF)
option(HYBBX_CRYPTO_OPENSSL     "Optional OpenSSL crypto backends"   OFF)
option(HYBBX_CRYPTO_LIBSODIUM   "Optional libsodium crypto backends" OFF)

cmake_dependent_option(HYBBX_BUILD_CLIENT_TELNET
    "Build hybbx-telnet standalone client" ON "HYBBX_BUILD_CLIENTS" OFF)
cmake_dependent_option(HYBBX_BUILD_CLIENT_TERMINAL
    "Build hybbx-terminal standalone client" ON "HYBBX_BUILD_CLIENTS" OFF)

if(HYBBX_CLIENTS_ONLY)
    set(HYBBX_BUILD_DAEMON OFF CACHE BOOL "Clients-only build" FORCE)
    set(HYBBX_BUILD_PLUGINS OFF CACHE BOOL "Clients-only build" FORCE)
    set(HYBBX_BUILD_CLIENTS ON CACHE BOOL "Clients-only build" FORCE)
endif()

if(NOT HYBBX_BUILD_DAEMON)
    set(HYBBX_BUILD_PLUGINS OFF CACHE BOOL "No daemon — skip plugins" FORCE)
endif()

set(HYBBX_HAVE_OPENSSL FALSE)
if(HYBBX_CRYPTO_OPENSSL)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        set(HYBBX_HAVE_OPENSSL TRUE)
        message(STATUS "HyBBX: OpenSSL crypto backends enabled")
    else()
        message(WARNING "HYBBX_CRYPTO_OPENSSL=ON but OpenSSL was not found")
    endif()
endif()

set(HYBBX_HAVE_LIBSODIUM FALSE)
if(HYBBX_CRYPTO_LIBSODIUM)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSODIUM QUIET libsodium)
    endif()
    if(LIBSODIUM_FOUND)
        set(HYBBX_HAVE_LIBSODIUM TRUE)
        message(STATUS "HyBBX: libsodium crypto backends enabled")
    else()
        message(WARNING "HYBBX_CRYPTO_LIBSODIUM=ON but libsodium was not found")
    endif()
endif()

# ---------------------------------------------------------------------------
# Platform detection (see docs/PLATFORMS.md)
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")
    set(HYBBX_PLATFORM_AMIGAOS TRUE)
    message(STATUS "HyBBX: targeting AmigaOS 3.9+ (cross-GCC)")
elseif(WIN32)
    set(HYBBX_PLATFORM_WINDOWS TRUE)
    message(STATUS "HyBBX: targeting Windows (GCC/Clang via MinGW or LLVM-MinGW)")
elseif(APPLE)
    set(HYBBX_PLATFORM_MACOS TRUE)
    message(STATUS "HyBBX: targeting macOS (GCC / Clang)")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(HYBBX_PLATFORM_LINUX TRUE)
    message(STATUS "HyBBX: targeting Linux")
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    set(HYBBX_PLATFORM_BSD TRUE)
    message(STATUS "HyBBX: targeting BSD (${CMAKE_SYSTEM_NAME})")
else()
    message(STATUS "HyBBX: targeting ${CMAKE_SYSTEM_NAME} (POSIX/GCC/Clang)")
endif()

if(NOT DEFINED HYBBX_PLATFORM_AMIGAOS)
    set(HYBBX_PLATFORM_AMIGAOS FALSE)
endif()

# ---------------------------------------------------------------------------
# Install layout
#
# Self-contained tree under CMAKE_INSTALL_PREFIX:
#
#   bin/hybbx            executable
#   bin/hybbx-start      start script (CMake install only)
#   etc/hybbx.ini        configuration
#   text/                banner, motd, news
#   data/                runtime flat-file storage (created at install)
#   lib/…                libraries, plugins, CMake package files
#
# Recommended prefixes:
#   system:  sudo cmake --install build --prefix /usr/local/hybbx
#   user:         cmake --install build --prefix "$HOME/hybbx"
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

set(HYBBX_INSTALL_ETCDIR "etc"
    CACHE PATH "Config directory under CMAKE_INSTALL_PREFIX")
set(HYBBX_INSTALL_TEXTDIR "text"
    CACHE PATH "Text files directory under CMAKE_INSTALL_PREFIX")
set(HYBBX_INSTALL_STORAGEDIR "data"
    CACHE PATH "Default flat-file data directory under CMAKE_INSTALL_PREFIX")

set(HYBBX_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/hybbx/plugins"
    CACHE PATH "Directory for HyBBX transport plugins"
)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Compiler.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Hardening.cmake)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)

if(HYBBX_BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

if(HYBBX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Package config (for downstream consumers)
# ---------------------------------------------------------------------------
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HyBBXConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HyBBXConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/HyBBX"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HyBBXConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/HyBBXConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/HyBBXConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/HyBBX"
)
