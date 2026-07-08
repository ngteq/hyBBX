add_library(hybbx_plugin_baycom STATIC
    baycom.c
    baycom_config.c
    baycom_modem.c
    baycom_kernel.c
    baycom_kiss.c
    ../packet_radio/ax25.c
    ../packet_radio/kiss.c
    ../packet_radio/serial_port.c
)

target_include_directories(hybbx_plugin_baycom
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/plugins/packet_radio
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(hybbx_plugin_baycom
    PRIVATE HYBBX_PLUGIN_BUILD
)

find_package(Threads REQUIRED)
target_link_libraries(hybbx_plugin_baycom PRIVATE hybbx_core Threads::Threads)
if(HYBBX_PLATFORM_AMIGAOS)
    target_link_libraries(hybbx_plugin_baycom PRIVATE amiga)
endif()
target_link_libraries(hybbx PRIVATE hybbx_plugin_baycom)
target_compile_definitions(hybbx PRIVATE HYBBX_HAVE_PLUGIN_BAYCOM)
hybbx_apply_hardening(hybbx_plugin_baycom)

install(TARGETS hybbx_plugin_baycom
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
