file(GLOB_RECURSE WS_DIR ./*.cc)

# 添加http库
add_library(zmuduo_ws SHARED ${HTTP_DIR})

# 链接base_lib库
target_link_libraries(zmuduo_ws zmuduo_http)

# 将zmuduo_ws拷贝到lib目录
install(TARGETS zmuduo_ws DESTINATION lib)
# 把当前目录的头文件拷贝到指定目录
file(GLOB HEADERS "./*.h")
install(FILES ${HEADERS} DESTINATION include/zmuduo/net/http/ws)