add_library(hybbx_plugin_ardop STATIC
    ardop.c
    ardop_config.c
    ardop_host.c
    ardop_crc.c
)

target_include_directories(hybbx_plugin_ardop
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(hybbx_plugin_ardop
    PRIVATE HYBBX_PLUGIN_BUILD
)

find_package(Threads REQUIRED)
target_link_libraries(hybbx_plugin_ardop PRIVATE hybbx_core Threads::Threads)
if(HYBBX_PLATFORM_AMIGAOS)
    target_link_libraries(hybbx_plugin_ardop PRIVATE amiga)
endif()
target_link_libraries(hybbx PRIVATE hybbx_plugin_ardop)
target_compile_definitions(hybbx PRIVATE HYBBX_HAVE_PLUGIN_ARDOP)
hybbx_apply_hardening(hybbx_plugin_ardop)

install(TARGETS hybbx_plugin_ardop
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
