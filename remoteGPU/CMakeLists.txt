# Minimum CMake required
cmake_minimum_required(VERSION 3.15)

# Project
project(remoteGPU)

# Protobuf
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

# gRPC
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

# Find Abseil
find_package(absl REQUIRED)

# Set explicit paths for protoc and grpc_cpp_plugin
set(PROTOC_EXECUTABLE /usr/bin/protoc)
set(GRPC_CPP_PLUGIN_EXECUTABLE /usr/bin/grpc_cpp_plugin)

# Proto file path
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../protos/remote_gpu.proto")

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/remote_gpu.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/remote_gpu.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/remote_gpu.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/remote_gpu.grpc.pb.h")

# Generate source files from the .proto file
add_custom_command(
    OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
    COMMAND ${PROTOC_EXECUTABLE}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${CMAKE_CURRENT_SOURCE_DIR}/../protos"
         --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
)

# Include the generated header files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Define the targets (client and server)
foreach(_target client server)
    add_executable(${_target} "${_target}.cc"
        ${hw_proto_srcs}
        ${hw_grpc_srcs}
    )
    target_link_libraries(${_target}
        ${gRPC_LIBRARIES}
        ${Protobuf_LIBRARIES}
        absl::base
        absl::log
        absl::strings
        absl::log_internal_check_op
        absl::log_internal_conditions
        absl::cord
        gRPC::grpc      # Link the core gRPC library
        gRPC::grpc++    # Link the C++ gRPC library
    )
endforeach()