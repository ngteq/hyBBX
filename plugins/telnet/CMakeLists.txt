add_library(hybbx_plugin_telnet STATIC
    telnet.c
    telnet_config.c
    telnet_proto.c
)

find_package(Threads REQUIRED)

target_include_directories(hybbx_plugin_telnet
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(hybbx_plugin_telnet
    PRIVATE HYBBX_PLUGIN_BUILD
)

target_link_libraries(hybbx_plugin_telnet PRIVATE hybbx_core Threads::Threads)
hybbx_link_plugin_instances(hybbx_plugin_telnet HYBBX_HAVE_PLUGIN_TELNET)
hybbx_apply_hardening(hybbx_plugin_telnet)

install(TARGETS hybbx_plugin_telnet
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
