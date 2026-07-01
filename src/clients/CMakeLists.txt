# Standalone CLI clients (hybbx-telnet, hybbx-terminal).
# No centralized daemon; flags/env only — see src/clients/*.c for options.

add_library(hybbx_client STATIC
    ${CMAKE_SOURCE_DIR}/src/core/util.c
    ${CMAKE_SOURCE_DIR}/src/core/circuit.c
    ${CMAKE_SOURCE_DIR}/src/core/traffic.c
    ${CMAKE_SOURCE_DIR}/src/core/terminal.c
    ${CMAKE_SOURCE_DIR}/plugins/packet_radio/ax25.c
    circuit_client.c
    link_auth.c
    client_display.c
)

target_include_directories(hybbx_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/plugins/packet_radio
)

target_compile_definitions(hybbx_client PRIVATE HYBBX_CLIENT_BUILD=1)
hybbx_apply_hardening(hybbx_client)

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
    install(TARGETS hybbx-telnet RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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
    install(TARGETS hybbx-terminal RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(TARGETS hybbx_client
    EXPORT HyBBXTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
