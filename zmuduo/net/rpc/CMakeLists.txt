# 查找protobuf
find_package(Protobuf REQUIRED)
if (PROTOBUF_FOUND)
    message("protobuf found")
else ()
    message(FATAL_ERROR "Cannot find Protobuf")
endif ()
# 指定 proto 文件
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/rpc.proto)

# 设置生成文件路径在源码目录（当前目录）
set(GENERATED_SRC ${CMAKE_CURRENT_SOURCE_DIR}/rpc.pb.cc)
set(GENERATED_HDR ${CMAKE_CURRENT_SOURCE_DIR}/rpc.pb.h)

# 自定义生成命令
add_custom_command(
        OUTPUT ${GENERATED_SRC} ${GENERATED_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}
        -I ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ source from ${PROTO_FILE}"
)

# 添加源文件
aux_source_directory(. SRC_LIST)
# 将生成的文件添加到SRC_LIST中
list(APPEND SRC_LIST  ${GENERATED_SRC} ${GENERATED_HDR})
add_library(zmuduo_rpc SHARED ${SRC_LIST})
# 设置包含路径（包括 protobuf 生成路径）

target_include_directories(zmuduo_rpc PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${Protobuf_INCLUDE_DIRS}
)
# rpc依赖http模块
target_link_libraries(zmuduo_rpc zmuduo_net)