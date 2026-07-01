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
    DESCRIPTION "BBS/mailbox-inspired service for multiple connection types"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(HYBBX_BUILD_PLUGINS      "Build transport plugins"           ON)
option(HYBBX_PLUGIN_TELNET      "Build TCP/IP telnet transport"     ON)
option(HYBBX_PLUGIN_PACKET_RADIO "Build packet radio transport"     ON)
option(HYBBX_BUILD_TESTS        "Build unit tests"                  OFF)
option(HYBBX_HARDENING          "Enable compiler security hardening" ON)
option(HYBBX_WARNINGS_AS_ERRORS "Treat warnings as errors"           OFF)
option(HYBBX_CRYPTO_OPENSSL     "Optional OpenSSL crypto backends"   OFF)
option(HYBBX_CRYPTO_LIBSODIUM   "Optional libsodium crypto backends" OFF)

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
# Platform detection
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")
    set(HYBBX_PLATFORM_AMIGAOS TRUE)
    message(STATUS "HyBBX: targeting AmigaOS")
else()
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
