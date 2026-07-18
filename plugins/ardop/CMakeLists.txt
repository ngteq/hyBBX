add_library(hybbx_ardop_common STATIC
    ardop_link.c
    ardop_host.c
    ardop_crc.c
    ardop_config.c
)

target_include_directories(hybbx_ardop_common
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(hybbx_ardop_common
    PRIVATE HYBBX_PLUGIN_BUILD
)

find_package(Threads REQUIRED)
target_link_libraries(hybbx_ardop_common PUBLIC hybbx_core Threads::Threads)
if(HYBBX_PLATFORM_AMIGAOS)
    target_link_libraries(hybbx_ardop_common PUBLIC amiga)
endif()
hybbx_apply_hardening(hybbx_ardop_common)

if(HYBBX_PLUGIN_ARDOP)
    add_library(hybbx_plugin_ardop STATIC
        ardop.c
    )

    target_include_directories(hybbx_plugin_ardop
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(hybbx_plugin_ardop
        PRIVATE HYBBX_PLUGIN_BUILD
    )

    target_link_libraries(hybbx_plugin_ardop PRIVATE hybbx_ardop_common)
    hybbx_link_plugin_instances(hybbx_plugin_ardop HYBBX_HAVE_PLUGIN_ARDOP)
    hybbx_apply_hardening(hybbx_plugin_ardop)

    install(TARGETS hybbx_plugin_ardop
        ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
        LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    )
endif()

install(TARGETS hybbx_ardop_common
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
