file(COPY attachment DESTINATION ${CMAKE_BINARY_DIR}/example/smtp_test)

add_executable(smtp_test smtp_test.cc)

target_link_libraries(smtp_test zmuduo_email)