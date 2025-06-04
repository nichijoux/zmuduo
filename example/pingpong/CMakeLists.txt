add_executable(pingpong_client client.cc)
target_link_libraries(pingpong_client zmuduo_net)

add_executable(pingpong_server server.cc)
target_link_libraries(pingpong_server zmuduo_net)

add_executable(pingpong_bench bench.cc)
target_link_libraries(pingpong_bench zmuduo_net)