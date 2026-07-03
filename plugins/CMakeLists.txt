if(HYBBX_PLUGIN_TELNET)
    add_subdirectory(telnet)
endif()

if(HYBBX_PLUGIN_PACKET_RADIO)
    add_subdirectory(packet_radio)
endif()

if(HYBBX_PLUGIN_ARDOP)
    add_subdirectory(ardop)
endif()
