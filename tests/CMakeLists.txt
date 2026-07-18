add_executable(hybbx_test_util test_util.c)
target_link_libraries(hybbx_test_util PRIVATE hybbx_core)

add_executable(hybbx_test_config test_config.c)
target_link_libraries(hybbx_test_config PRIVATE hybbx_core)

function(hybbx_add_instance_test name role_src)
    add_executable(${name} test_instance.c ${role_src})
    target_link_libraries(${name} PRIVATE hybbx_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

hybbx_add_instance_test(hybbx_test_instance_main
    ${CMAKE_SOURCE_DIR}/src/instance_role_main.c)
hybbx_add_instance_test(hybbx_test_instance_secondary
    ${CMAKE_SOURCE_DIR}/src/instance_role_secondary.c)
hybbx_add_instance_test(hybbx_test_instance_proxy
    ${CMAKE_SOURCE_DIR}/src/instance_role_proxy.c)

add_test(NAME util COMMAND hybbx_test_util)
add_test(NAME config COMMAND hybbx_test_config)
