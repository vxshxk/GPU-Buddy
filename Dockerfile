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

# Pass the token securely as an argument
ARG GITHUB_TOKEN

# Clone the private repository into a specific directory in the container
RUN git clone https://$GITHUB_TOKEN@github.com/vxshxk/GPU-Buddy.git /GPU-Buddy

WORKDIR /GPU-Buddy
RUN git checkout Server
RUN chmod +x build_script.sh && chmod +x run_server.sh && chmod +x run_client.sh
RUN ./build_script.sh