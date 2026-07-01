add_library(hybbx_core STATIC
    core/registry.c
    core/service.c
    core/config.c
    core/circuit.c
    core/circuit_tcp.c
    core/command.c
    core/command_parse.c
    core/auth.c
    core/password.c
    core/crypto_config.c
    core/crypto_backends.c
    core/crypto.c
    core/traffic.c
    core/storage.c
    core/storage_flatfile.c
    core/storage_sql.c
    core/session.c
    core/chat.c
    core/terminal.c
    core/texts.c
    core/util.c
    ${CMAKE_SOURCE_DIR}/third_party/tinysha256/tinysha256.c
    ${CMAKE_SOURCE_DIR}/third_party/monocypher/monocypher.c
    ${CMAKE_SOURCE_DIR}/third_party/tinyaes/aes.c
    ${CMAKE_SOURCE_DIR}/third_party/tinyaes/aes256gcm.c
)

if(HYBBX_HAVE_OPENSSL)
    target_sources(hybbx_core PRIVATE core/crypto_openssl.c)
endif()

if(HYBBX_HAVE_LIBSODIUM)
    target_sources(hybbx_core PRIVATE core/crypto_libsodium.c)
endif()

add_library(HyBBX::core ALIAS hybbx_core)

target_include_directories(hybbx_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_SOURCE_DIR}/third_party/tinysha256
        ${CMAKE_SOURCE_DIR}/third_party/monocypher
        ${CMAKE_SOURCE_DIR}/third_party/tinyaes
)

target_compile_definitions(hybbx_core
    PRIVATE
        HYBBX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        HYBBX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        HYBBX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

hybbx_apply_hardening(hybbx_core)

find_package(Threads REQUIRED)
target_link_libraries(hybbx_core PUBLIC Threads::Threads)

if(HYBBX_HAVE_OPENSSL)
    target_compile_definitions(hybbx_core PRIVATE HYBBX_HAVE_OPENSSL)
    target_link_libraries(hybbx_core PRIVATE OpenSSL::Crypto)
endif()

if(HYBBX_HAVE_LIBSODIUM)
    target_compile_definitions(hybbx_core PRIVATE HYBBX_HAVE_LIBSODIUM)
    target_include_directories(hybbx_core PRIVATE ${LIBSODIUM_INCLUDE_DIRS})
    target_link_libraries(hybbx_core PRIVATE ${LIBSODIUM_LIBRARIES})
endif()

add_executable(hybbx main.c)
target_link_libraries(hybbx PRIVATE hybbx_core)
hybbx_apply_hardening_executable(hybbx)

install(TARGETS hybbx hybbx_core
    EXPORT HyBBXTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/hybbx
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(FILES ${CMAKE_SOURCE_DIR}/share/hybbx.ini.example
    DESTINATION ${HYBBX_INSTALL_ETCDIR}
    RENAME hybbx.ini
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/text/
    DESTINATION ${HYBBX_INSTALL_TEXTDIR}
    FILES_MATCHING PATTERN "*.txt"
)

install(FILES ${CMAKE_SOURCE_DIR}/share/data.gitkeep
    DESTINATION ${HYBBX_INSTALL_STORAGEDIR}
    RENAME .gitkeep
)

install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/hybbx.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME hybbx-start
)

install(EXPORT HyBBXTargets
    FILE HyBBXTargets.cmake
    NAMESPACE HyBBX::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HyBBX
)
