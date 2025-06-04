# 把当前目录的头文件拷贝到指定目录
file(GLOB HEADERS "./*.h")
install(FILES ${HEADERS} DESTINATION include/zmuduo/base/utils)