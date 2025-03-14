#!/bin/bash
cd protos
protoc -I. --cpp_out=../remoteGPU --grpc_out=../remoteGPU   --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin gpu.proto
cd ..
cd remoteGPU
mkdir -p cmake/build
cd cmake/build
cmake ../..
make