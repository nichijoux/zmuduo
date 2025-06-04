add_subdirectory(daytime)
add_subdirectory(echo)
add_subdirectory(pingpong)
add_subdirectory(timing_wheel)
add_subdirectory(serial_test)
add_subdirectory(smtp_test)
add_subdirectory(pop3_test)
add_subdirectory(tcp_client_test)
add_subdirectory(http_test)
add_subdirectory(websocket)
add_subdirectory(udp_server_test)
add_subdirectory(udp_client_test)
# 设置自定义CMake模块路径
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})
# 是否启用openssl
if (ENABLE_OPENSSL)
    add_subdirectory(openssl_test)
endif ()

# 查找Gperftools
find_package(Gperftools)

# 如果找到Gperftools，则添加包含目录
if (GPERFTOOLS_FOUND)
    add_subdirectory(inspect)
endif ()

add_subdirectory(cdns)
add_subdirectory(webdav)