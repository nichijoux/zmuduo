add_executable(ws_server server.cc)

target_link_libraries(ws_server zmuduo_ws)

add_executable(ws_client client.cc)

target_link_libraries(ws_client zmuduo_ws)