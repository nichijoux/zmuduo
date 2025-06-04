cmake_minimum_required(VERSION 3.5.1)
project(zmuduo)

# 设置c++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入头文件
include_directories(${PROJECT_SOURCE_DIR})

# cmake可选项
option(ENABLE_OPENSSL "是否启用OpenSSL" ON)
option(ZMUDUO_BUILD_EXAMPLES "是否构建example" ON)

# 添加base和net子目录
add_subdirectory(zmuduo/base)
add_subdirectory(zmuduo/net)

if (ENABLE_OPENSSL)
    message(STATUS "启用openssl")
    find_package(OpenSSL REQUIRED)
    if (OpenSSL_FOUND)
        include_directories(${OPENSSL_INCLUDE_DIR})
        # 将openssl库添加到LIBS中
        add_definitions(-DZMUDUO_ENABLE_OPENSSL)
    else ()
        message(FATAL_ERROR "启用了OpenSSL但系统中找不到OpenSSL库.")
    endif ()
endif ()
# 是否编译example
if (ZMUDUO_BUILD_EXAMPLES)
    add_subdirectory(example)
endif ()