# Use NVIDIA CUDA Image for GPU Support
FROM nvidia/cuda:11.6.2-devel-ubuntu20.04

# Install necessary dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    git \
    python3.12 \
    python3-pip \
    iproute2 \
    python3.12-venv \
    protobuf-compiler \
    libprotobuf-dev \
    protobuf-c-compiler

# Set working directory for gRPC installation
WORKDIR /deps

# Clone gRPC repository
RUN git clone --recurse-submodules -b v1.69.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc.git

# Build and install gRPC
RUN mkdir -p /deps/grpc/build && cd /deps/grpc/build && \
    cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
          -DgRPC_PROTOBUF_PROVIDER=package \
          -DgRPC_ZLIB_PROVIDER=package \
          -DgRPC_SSL_PROVIDER=package \
          .. && \
    make -j$(nproc) install

# Set Python 3.12 as default
RUN update-alternatives --install /usr/bin/python python /usr/bin/python3.12 1 && \
    update-alternatives --set python /usr/bin/python3.12
    # Use a newer CUDA image with Ubuntu 22.04
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04

