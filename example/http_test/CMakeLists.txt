add_executable(http_server server.cc)

target_link_libraries(http_server zmuduo_http)

add_executable(http_client client.cc)

target_link_libraries(http_client zmuduo_http)