add_library(hybbx_plugin_packet_radio STATIC
    packet_radio.c
    packet_radio_config.c
    ax25.c
    kiss.c
    serial_port.c
    sixpack.c
    tnc.c
    tnc_host.c
    tnc_profiles.c
)

target_include_directories(hybbx_plugin_packet_radio
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(hybbx_plugin_packet_radio
    PRIVATE HYBBX_PLUGIN_BUILD
)

find_package(Threads REQUIRED)
target_link_libraries(hybbx_plugin_packet_radio PRIVATE hybbx_core Threads::Threads)
if(HYBBX_PLATFORM_AMIGAOS)
    target_link_libraries(hybbx_plugin_packet_radio PRIVATE amiga)
endif()
hybbx_link_plugin_instances(hybbx_plugin_packet_radio HYBBX_HAVE_PLUGIN_PACKET_RADIO)
hybbx_apply_hardening(hybbx_plugin_packet_radio)

install(TARGETS hybbx_plugin_packet_radio
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
