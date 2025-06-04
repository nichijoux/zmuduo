# 查找 ZLIB
find_package(ZLIB)

# 基本源文件（总是编译）
set(SOURCES
        main.cc
        inspector.cc
        performance_inspector.cc
        system_inspector.cc
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
add_executable(inspect ${SOURCES})

# 链接依赖
target_link_libraries(inspect zmuduo_http tcmalloc_and_profiler)

if (ZLIB_FOUND)
    target_link_libraries(inspect ZLIB::ZLIB)
endif ()