# Link a transport plugin into all HyBBX instance binaries (hybbxd/hybbxsd/hybbxpd).
function(hybbx_link_plugin_instances plugin_target define_name)
    foreach(_bin IN ITEMS hybbxd hybbxsd hybbxpd)
        if(TARGET ${_bin})
            target_link_libraries(${_bin} PRIVATE ${plugin_target})
            if(NOT "${define_name}" STREQUAL "")
                target_compile_definitions(${_bin} PRIVATE ${define_name})
            endif()
        endif()
    endforeach()
endfunction()

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

if(HYBBX_PLUGIN_BAYCOM)
    add_subdirectory(baycom)
endif()

if(HYBBX_PLUGIN_MAINS_PROXY)
    add_subdirectory(mains_proxy)
    target_compile_definitions(hybbx_core PRIVATE HYBBX_HAVE_PLUGIN_MAINS_PROXY)
endif()

if(HYBBX_PLUGIN_SSH AND HYBBX_HAVE_LIBSSH)
    add_subdirectory(ssh)
endif()

if(HYBBX_PLUGIN_WEBSOCKET)
    add_subdirectory(websocket)
endif()
