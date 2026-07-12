if(HYBBX_BUILD_DAEMON)
    add_library(hybbx_core STATIC
        core/registry.c
        core/service.c
        core/config.c
        core/circuit.c
        core/circuit_tcp.c
        core/circuit_bridge.c
        core/circuit_balance.c
        core/bandwidth_policy.c
        core/broadcast.c
        core/link.c
        core/command.c
        core/command_parse.c
        core/commands_registry.c
        core/auth.c
        core/password.c
        core/crypto_config.c
        core/crypto_backends.c
        core/crypto.c
        core/traffic.c
        core/storage.c
        core/storage_flatfile.c
        core/storage_sql.c
        core/mail_sql.c
        core/proxymail.c
        core/proxychat.c
        core/session.c
        core/chat.c
        core/conference.c
        core/mail.c
        core/mains_proxy.c
        core/networks.c
        core/crdop.c
        core/terminal.c
        core/texts.c
        core/util.c
        core/log.c
        core/security.c
        core/security_ban.c
        clients/circuit_client.c
        clients/link_auth.c
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

    if(HYBBX_HAVE_SQLITE)
        target_compile_definitions(hybbx_core PRIVATE HYBBX_HAVE_SQLITE)
        target_include_directories(hybbx_core PRIVATE ${SQLITE3_INCLUDE_DIRS})
        target_link_libraries(hybbx_core PRIVATE ${SQLITE3_LIBRARIES})
    endif()

    add_executable(hybbx main.c)
    target_link_libraries(hybbx PRIVATE hybbx_core)
    hybbx_apply_hardening_executable(hybbx)

    install(TARGETS hybbx hybbx_core
        EXPORT HyBBXTargets
        RUNTIME DESTINATION ${HYBBX_INSTALL_HOME}
        LIBRARY DESTINATION ${HYBBX_INSTALL_HOME}/lib
        ARCHIVE DESTINATION ${HYBBX_INSTALL_HOME}/lib
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/hybbx
        DESTINATION ${HYBBX_INSTALL_HOME}/include
        FILES_MATCHING PATTERN "*.h"
    )

    install(FILES
        ${CMAKE_SOURCE_DIR}/share/hybbx.ini.example
        ${CMAKE_SOURCE_DIR}/share/hybbx-standalone.ini.example
        ${CMAKE_SOURCE_DIR}/share/hybbx-main.ini.example
        ${CMAKE_SOURCE_DIR}/share/hybbx-mesh.ini.example
        ${CMAKE_SOURCE_DIR}/share/hybbx-secondary.ini.example
        DESTINATION ${HYBBX_INSTALL_HOME}
    )

    install(FILES ${CMAKE_SOURCE_DIR}/share/hybbx-main.ini.example
        DESTINATION ${HYBBX_INSTALL_HOME}
        RENAME hybbx.ini
    )

    install(FILES ${CMAKE_SOURCE_DIR}/share/commands.yaml
        DESTINATION ${HYBBX_INSTALL_HOME}
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/text/
        DESTINATION ${HYBBX_INSTALL_HOME}/text
        FILES_MATCHING PATTERN "*.txt"
    )

    install(FILES ${CMAKE_SOURCE_DIR}/share/THIRD_PARTY_NOTICES.txt
        DESTINATION ${HYBBX_INSTALL_HOME}
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share/reverse-proxy/
        DESTINATION ${HYBBX_INSTALL_HOME}/reverse-proxy
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share/cgi/
        DESTINATION ${HYBBX_INSTALL_HOME}/cgi
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

    install(FILES ${CMAKE_SOURCE_DIR}/share/data.gitkeep
        DESTINATION ${HYBBX_INSTALL_HOME}/data
        RENAME .gitkeep
    )

    install(FILES ${CMAKE_SOURCE_DIR}/share/logs.gitkeep
        DESTINATION ${HYBBX_INSTALL_HOME}/logs
        RENAME .gitkeep
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share/fail2ban/
        DESTINATION ${HYBBX_INSTALL_HOME}/fail2ban
    )

    install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/hybbx.sh
        DESTINATION ${HYBBX_INSTALL_HOME}
        RENAME hybbx-start
    )

    install(EXPORT HyBBXTargets
        FILE HyBBXTargets.cmake
        NAMESPACE HyBBX::
        DESTINATION ${HYBBX_INSTALL_HOME}/lib/cmake/HyBBX
    )
endif()

if(HYBBX_BUILD_CLIENTS)
    add_subdirectory(clients)
endif()
