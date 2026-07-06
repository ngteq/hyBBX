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

if(HYBBX_PLUGIN_SSH AND HYBBX_HAVE_LIBSSH)
    add_subdirectory(ssh)
endif()

if(HYBBX_PLUGIN_WEBSOCKET)
    add_subdirectory(websocket)
endif()
