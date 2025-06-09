# 查找protobuf
find_package(Protobuf REQUIRED)
if (PROTOBUF_FOUND)
    message("找到protobuf")
endif ()

# Proto 文件路径
set(PROTO_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/echo_service.proto
)

# 生成 .pb.h 和 .pb.cc
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

add_executable(rpc_echo_server echo_server.cc echo_service.pb.cc)

add_executable(rpc_echo_client echo_client.cc echo_service.pb.cc)

add_executable(rpc_registry_server echo_registry_server.cc)

target_include_directories(rpc_echo_server PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(rpc_echo_client PRIVATE ${CMAKE_CURRENT_BINARY_DIR})


target_link_libraries(rpc_echo_server zmuduo_rpc ${Protobuf_LIBRARIES})
target_link_libraries(rpc_echo_client zmuduo_rpc ${Protobuf_LIBRARIES})
target_link_libraries(rpc_registry_server zmuduo_rpc ${Protobuf_LIBRARIES})