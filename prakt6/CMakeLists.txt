cmake_minimum_required(VERSION 3.16)
project(grpc_async_homework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/proto/greeter.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(PROTO_SRC "${GENERATED_DIR}/greeter.pb.cc")
set(PROTO_HDR "${GENERATED_DIR}/greeter.pb.h")
set(GRPC_SRC "${GENERATED_DIR}/greeter.grpc.pb.cc")
set(GRPC_HDR "${GENERATED_DIR}/greeter.grpc.pb.h")

add_custom_command(
    OUTPUT "${PROTO_SRC}" "${PROTO_HDR}" "${GRPC_SRC}" "${GRPC_HDR}"
    COMMAND $<TARGET_FILE:protobuf::protoc>
    ARGS
        --proto_path="${CMAKE_CURRENT_SOURCE_DIR}/proto"
        --cpp_out="${GENERATED_DIR}"
        --grpc_out="${GENERATED_DIR}"
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}" protobuf::protoc gRPC::grpc_cpp_plugin
    VERBATIM
)

add_library(grpc_contract "${PROTO_SRC}" "${GRPC_SRC}")
target_include_directories(grpc_contract PUBLIC "${GENERATED_DIR}")
target_link_libraries(grpc_contract PUBLIC protobuf::libprotobuf gRPC::grpc++)

add_executable(grpc_server src/server.cpp)
target_link_libraries(grpc_server PRIVATE grpc_contract)

add_executable(unary_client src/unary_client.cpp)
target_link_libraries(unary_client PRIVATE grpc_contract)

add_executable(streaming_client src/streaming_client.cpp)
target_link_libraries(streaming_client PRIVATE grpc_contract)

add_executable(async_client src/async_client.cpp)
target_link_libraries(async_client PRIVATE grpc_contract)
