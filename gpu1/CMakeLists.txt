cmake_minimum_required(VERSION 3.10)
project(grpc_hello)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

add_executable(server server.cpp gpu.pb.cc gpu.grpc.pb.cc)
add_executable(client client.cpp gpu.pb.cc gpu.grpc.pb.cc)

target_link_libraries(server gRPC::grpc++ protobuf::libprotobuf)
target_link_libraries(client gRPC::grpc++ protobuf::libprotobuf)
