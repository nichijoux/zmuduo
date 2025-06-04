# 获取当前目录及子目录下的所有 .cpp 和 .h 文件
file(GLOB_RECURSE SOURCES "*.cc")
file(GLOB_RECURSE HEADERS "*.h")

add_library(zmuduo_base SHARED ${HEADERS} ${SOURCES})

add_subdirectory(utils)

# 将zmuduo_base 拷贝到lib目录
install(TARGETS zmuduo_base DESTINATION lib)
# 把当前目录的头文件拷贝到指定目录
file(GLOB HEADERS "./*.h")
install(FILES ${HEADERS} DESTINATION include/zmuduo/base)