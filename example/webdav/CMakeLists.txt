# 查找 ZLIB
find_package(ZLIB)

# 基本源文件（总是编译）
set(SOURCES
        main.cc
        tinyxml2.cc
        webdav_server.cc
)

# 如果找到 ZLIB，添加 compress_filter.cc
if (ZLIB_FOUND)
    message(STATUS "ZLIB found: ${ZLIB_INCLUDE_DIRS}")
    list(APPEND SOURCES compress_filter.cc)
    add_definitions(-DZMUDUO_FIND_ZLIB)
else ()
    message(WARNING "ZLIB not found, compress_filter will be excluded.")
endif ()

# 构建可执行文件
add_executable(zmuduo_webdav ${SOURCES})

# 链接依赖
target_link_libraries(zmuduo_webdav zmuduo_http)

if (ZLIB_FOUND)
    target_link_libraries(zmuduo_webdav ZLIB::ZLIB)
endif ()
