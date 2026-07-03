if(HYBBX_PLUGIN_TELNET)
    add_subdirectory(telnet)
endif()

if(HYBBX_PLUGIN_PACKET_RADIO)
    add_subdirectory(packet_radio)
endif()

if(HYBBX_PLUGIN_ARDOP OR HYBBX_PLUGIN_CRDOP)
    add_subdirectory(ardop)
endif()

if(HYBBX_PLUGIN_CRDOP)
    add_subdirectory(crdop)
endif()
