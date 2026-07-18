add_library(hybbx_plugin_mains_proxy STATIC
    mains_proxy.c
)

target_include_directories(hybbx_plugin_mains_proxy
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(hybbx_plugin_mains_proxy
    PRIVATE HYBBX_PLUGIN_BUILD
)

target_link_libraries(hybbx_plugin_mains_proxy PRIVATE hybbx_core)
hybbx_link_plugin_instances(hybbx_plugin_mains_proxy HYBBX_HAVE_PLUGIN_MAINS_PROXY)

hybbx_apply_hardening(hybbx_plugin_mains_proxy)

install(TARGETS hybbx_plugin_mains_proxy
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
