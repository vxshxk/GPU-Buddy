cmake_minimum_required(VERSION 3.5)
project(RemoteGPU)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/remotegpu)

add_executable(server 
    remoteGPU/server.cc 
    remoteGPU/CodeExtractor.cc 
    remoteGPU/CodeRestorer.cc 
    remoteGPU/remote_gpu.grpc.pb.cc 
    remoteGPU/remote_gpu.pb.cc
)

target_link_libraries(server gRPC::grpc++ protobuf::libprotobuf)
