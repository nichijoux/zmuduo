aux_source_directory(. EMAIL_DIR)

# 添加email库
add_library(zmuduo_email SHARED ${EMAIL_DIR})

# 链接base_lib库
target_link_libraries(zmuduo_email zmuduo_net)

# 将zmuduo_email 拷贝到lib目录
install(TARGETS zmuduo_email DESTINATION lib)
# 把当前目录的头文件拷贝到指定目录
file(GLOB HEADERS "./*.h")
install(FILES ${HEADERS} DESTINATION include/zmuduo/net/email)