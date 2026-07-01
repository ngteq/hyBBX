add_executable(hybbx_test_util test_util.c)
target_link_libraries(hybbx_test_util PRIVATE hybbx_core)

add_test(NAME util COMMAND hybbx_test_util)
