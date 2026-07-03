add_library(hybbx_plugin_crdop STATIC
    crdop.c
    crdop_config.c
)

target_include_directories(hybbx_plugin_crdop
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/plugins/ardop
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(hybbx_plugin_crdop
    PRIVATE HYBBX_PLUGIN_BUILD
)

target_link_libraries(hybbx_plugin_crdop PRIVATE hybbx_ardop_common)
target_link_libraries(hybbx PRIVATE hybbx_plugin_crdop)
target_compile_definitions(hybbx PRIVATE HYBBX_HAVE_PLUGIN_CRDOP)
hybbx_apply_hardening(hybbx_plugin_crdop)

install(TARGETS hybbx_plugin_crdop
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
