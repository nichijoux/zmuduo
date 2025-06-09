# 获取当前目录的源文件
aux_source_directory(. NET_DIR)
# 设置 poller 子目录下所有 .cc 文件
file(GLOB POLLER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/poller/*.cc")
# 合并两个源文件列表
set(NET_DIR ${NET_DIR} ${POLLER_DIR})

message("net dir is ${NET_DIR}")
# 添加net库
add_library(zmuduo_net SHARED ${NET_DIR})

set(LIBS zmuduo_base)

if (ENABLE_OPENSSL)
    find_package(OpenSSL REQUIRED)
    if (OpenSSL_FOUND)
        include_directories(${OPENSSL_INCLUDE_DIR})
        # 将openssl库添加到LIBS中
        list(APPEND LIBS ${OPENSSL_LIBRARIES})
        add_definitions(-DZMUDUO_ENABLE_OPENSSL)
        message(STATUS "OpenSSL enabled. Include dir: ${OPENSSL_INCLUDE_DIR}, Libraries: ${LIBS}")
    else ()
        message(FATAL_ERROR "OpenSSL is enabled but could not be found.")
    endif ()
else ()
    message("不启用openssl")
endif ()

# 链接base_lib库
target_link_libraries(zmuduo_net ${LIBS})

# 将zmuduo_net 拷贝到lib目录
install(TARGETS zmuduo_net DESTINATION lib)
# 把当前目录的头文件拷贝到指定目录
file(GLOB HEADERS "./*.h")
install(FILES ${HEADERS} DESTINATION include/zmuduo/net)

# http库
add_subdirectory(http)
# 邮件库
add_subdirectory(email)
find_package(Protobuf REQUIRED)
# rpc库
if (PROTOBUF_FOUND)
    # 找到了protobuf，编译rpc模块
    add_subdirectory(rpc)
endif ()