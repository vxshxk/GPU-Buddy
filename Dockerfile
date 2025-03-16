
FROM nvidia/cuda:12.2.2-devel-ubuntu22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    software-properties-common \
    cmake \
    build-essential \
    git \
    iproute2 \
    protobuf-compiler \
    libprotobuf-dev \
    python3 \
    python3-pip \
    python3-venv \
    wget \
    && apt-get clean && rm -rf /var/lib/apt/lists/*


RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.0-1_all.deb && \
    dpkg -i cuda-keyring_1.0-1_all.deb && \
    apt-get update && apt-get install -y libcudnn8 libcudnn8-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /deps

RUN git clone --recurse-submodules -b v1.69.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc.git


RUN mkdir -p /deps/grpc/build && cd /deps/grpc/build && \
    cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          .. && \
    make -j8 install

RUN update-alternatives --install /usr/bin/python python /usr/bin/python3.10 1

RUN pip install --no-cache-dir --upgrade pip
RUN pip install --no-cache-dir --index-url https://download.pytorch.org/whl/cu121 torch torchvision
RUN pip install --no-cache-dir tensorflow

ARG GITHUB_TOKEN
EXPOSE 50052


RUN git clone https://$GITHUB_TOKEN@github.com/vxshxk/GPU-Buddy.git /GPU-Buddy

WORKDIR /GPU-Buddy
ARG CACHE_BREAK=0
RUN git fetch
RUN git checkout scriptfix

RUN chmod +x build_script.sh && chmod +x run_server.sh && chmod +x run_client.sh
RUN ./build_script.sh
