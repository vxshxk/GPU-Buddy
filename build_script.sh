#!/bin/bash

protoc -I=protos/ --cpp_out=remoteGPU/ protos/gpu.proto
protoc -I=protos/ --grpc_out=remoteGPU/ --plugin=psrotoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin protos/gpu.proto

cd remoteGPU
mkdir -p cmake/build
cd cmake/build
cmake ../..
make