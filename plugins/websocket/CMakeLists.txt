add_library(hybbx_plugin_websocket STATIC
    websocket.c
    ws_config.c
    ws_proto.c
    ws_sha1.c
    ws_tls.c
)

find_package(Threads REQUIRED)

target_include_directories(hybbx_plugin_websocket
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(hybbx_plugin_websocket
    PRIVATE HYBBX_PLUGIN_BUILD
)

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_compile_definitions(hybbx_plugin_websocket PRIVATE HYBBX_WS_HAVE_TLS)
    target_link_libraries(hybbx_plugin_websocket PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "WebSocket plugin: OpenSSL found — wss + self-signed certs")
else()
    message(STATUS "WebSocket plugin: OpenSSL not found — plain ws only")
endif()

target_link_libraries(hybbx_plugin_websocket PRIVATE hybbx_core Threads::Threads)
hybbx_link_plugin_instances(hybbx_plugin_websocket HYBBX_HAVE_PLUGIN_WEBSOCKET)
hybbx_apply_hardening(hybbx_plugin_websocket)

install(TARGETS hybbx_plugin_websocket
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
