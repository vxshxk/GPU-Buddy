# Use the latest Ubuntu image
FROM ubuntu:latest

# Install necessary packages
RUN apt-get update && apt-get install -y cmake build-essential git python3.12 python3-pip iproute2 python3.12-venv protobuf-compiler libprotobuf-dev

# Set the working directory for gRPC installation
WORKDIR /deps

# Clone the gRPC repository with submodules (specify a stable version if needed)
RUN git clone --recurse-submodules -b v1.69.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc.git

# Build and install gRPC
RUN mkdir -p /deps/grpc/build && cd /deps/grpc/build && \
    cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          .. && \
    make -j8 install

RUN update-alternatives --install /usr/bin/python python /usr/bin/python3.12 1
EXPOSE 50052
