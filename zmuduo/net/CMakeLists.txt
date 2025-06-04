file(GLOB_RECURSE NET_DIR ./*.cc)

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
add_subdirectory(email)