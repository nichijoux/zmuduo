file(COPY cacert.pem DESTINATION ${CMAKE_BINARY_DIR}/example/openssl_test)
file(COPY privkey.pem DESTINATION ${CMAKE_BINARY_DIR}/example/openssl_test)

add_executable(ssl_server ssl_server.cc)
add_executable(ssl_client ssl_client.cc)

target_link_libraries(ssl_client zmuduo_net ${OPENSSL_LIBRARIES})
target_link_libraries(ssl_server zmuduo_net ${OPENSSL_LIBRARIES})