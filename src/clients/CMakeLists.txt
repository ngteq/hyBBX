# Standalone CLI clients (hybbx-telnet, hybbx-terminal, hybbx-ssh).
# No centralized daemon; flags/env only — see src/clients/*.c for options.

if(HYBBX_PLATFORM_AMIGAOS)
    set(_hybbx_client_sources
        ${CMAKE_SOURCE_DIR}/src/core/util.c
        ${CMAKE_SOURCE_DIR}/src/core/traffic.c
        ${CMAKE_SOURCE_DIR}/src/core/terminal.c
        client_line.c
        client_display.c
    )
else()
    set(_hybbx_client_sources
        ${CMAKE_SOURCE_DIR}/src/core/util.c
        ${CMAKE_SOURCE_DIR}/src/core/circuit.c
        ${CMAKE_SOURCE_DIR}/src/core/traffic.c
        ${CMAKE_SOURCE_DIR}/src/core/terminal.c
        ${CMAKE_SOURCE_DIR}/plugins/packet_radio/ax25.c
        circuit_client.c
        link_auth.c
        client_line.c
        client_display.c
    )
endif()

add_library(hybbx_client STATIC ${_hybbx_client_sources})

target_include_directories(hybbx_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/plugins/packet_radio
)

target_compile_definitions(hybbx_client PRIVATE HYBBX_CLIENT_BUILD=1)
hybbx_apply_hardening(hybbx_client)

if(HYBBX_PLATFORM_AMIGAOS)
    target_compile_options(hybbx_client PRIVATE -mcrt=clib2)
endif()

add_library(HyBBX::client ALIAS hybbx_client)

if(HYBBX_BUILD_CLIENT_TELNET)
    add_executable(hybbx-telnet
        hybbx_telnet.c
        ${CMAKE_SOURCE_DIR}/plugins/telnet/telnet_proto.c
    )
    target_include_directories(hybbx-telnet PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/plugins/telnet
    )
    target_link_libraries(hybbx-telnet PRIVATE hybbx_client)
    hybbx_apply_hardening_executable(hybbx-telnet)
    if(HYBBX_PLATFORM_AMIGAOS)
        target_compile_options(hybbx-telnet PRIVATE -mcrt=clib2)
        target_link_options(hybbx-telnet PRIVATE -mcrt=clib2)
        target_link_libraries(hybbx-telnet PRIVATE net unix)
    endif()
    install(TARGETS hybbx-telnet RUNTIME DESTINATION ${HYBBX_INSTALL_HOME})
endif()

if(HYBBX_BUILD_CLIENT_TERMINAL)
    add_executable(hybbx-terminal hybbx_terminal.c)
    target_include_directories(hybbx-terminal PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/plugins/packet_radio
    )
    target_link_libraries(hybbx-terminal PRIVATE hybbx_client)
    hybbx_apply_hardening_executable(hybbx-terminal)
    install(TARGETS hybbx-terminal RUNTIME DESTINATION ${HYBBX_INSTALL_HOME})
endif()

if(HYBBX_BUILD_CLIENT_SSH AND HYBBX_HAVE_LIBSSH)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSSH REQUIRED IMPORTED_TARGET libssh)

    add_executable(hybbx-ssh hybbx_ssh.c)
    target_include_directories(hybbx-ssh PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(hybbx-ssh PRIVATE hybbx_client PkgConfig::LIBSSH)
    hybbx_apply_hardening_executable(hybbx-ssh)
    install(TARGETS hybbx-ssh RUNTIME DESTINATION ${HYBBX_INSTALL_HOME})
endif()

install(TARGETS hybbx_client
    EXPORT HyBBXTargets
    ARCHIVE DESTINATION ${HYBBX_INSTALL_HOME}/lib
)
