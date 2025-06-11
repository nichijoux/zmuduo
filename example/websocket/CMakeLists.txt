file(COPY cacert.pem DESTINATION ${CMAKE_BINARY_DIR}/example/websocket)
file(COPY privkey.pem DESTINATION ${CMAKE_BINARY_DIR}/example/websocket)

add_executable(ws_server server.cc)

target_link_libraries(ws_server zmuduo_ws)

add_executable(ws_client client.cc)

target_link_libraries(ws_client zmuduo_ws)