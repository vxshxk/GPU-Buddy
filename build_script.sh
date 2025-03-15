#!/bin/bash
cd protos
protoc -I. --cpp_out=../remoteGPU --grpc_out=../remoteGPU   --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin gpu.proto
protoc -I. --cpp_out=../remoteGPU --grpc_out=../remoteGPU \
    --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proxy.proto

cd ..


# Step 2: Prepare build directory
BUILD_DIR="remoteGPU/cmake/build"

# Remove old CMake cache if it exists
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous CMake cache..."
    rm -rf "$BUILD_DIR"
fi

# Step 3: Create fresh build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake ../..
make
