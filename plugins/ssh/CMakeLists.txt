add_library(hybbx_plugin_ssh STATIC
    ssh.c
    ssh_config.c
    ssh_keys.c
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH REQUIRED IMPORTED_TARGET libssh)

find_package(Threads REQUIRED)

target_include_directories(hybbx_plugin_ssh
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(hybbx_plugin_ssh
    PRIVATE HYBBX_PLUGIN_BUILD
)

target_link_libraries(hybbx_plugin_ssh
    PRIVATE
        hybbx_core
        Threads::Threads
        PkgConfig::LIBSSH
)

target_link_libraries(hybbx PRIVATE hybbx_plugin_ssh)
target_compile_definitions(hybbx PRIVATE HYBBX_HAVE_PLUGIN_SSH)
hybbx_apply_hardening(hybbx_plugin_ssh)

install(TARGETS hybbx_plugin_ssh
    ARCHIVE DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${HYBBX_PLUGIN_INSTALL_DIR}
)
