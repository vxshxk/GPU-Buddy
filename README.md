# GPU-Buddy: Remote GPU Sharing with Virtualization

![image](https://github.com/user-attachments/assets/9b44ccfd-9679-4f0d-a81f-27d97018f98b)

GPU-Buddy is a client-server C++ application designed to enable **remote GPU sharing** among multiple clients via **gRPC**. It leverages **NVIDIA’s Multi-Process Service (MPS)** to allow efficient, concurrent execution of GPU-intensive tasks.

---
## 📌 Project Statement

To develop a client-server architecture that allows **multiple clients to share GPU resources remotely**, enabling efficient execution of parallel tasks using **GPU virtualization** and **gRPC-based communication**.

## 📚 Background

Remote GPU access is essential for compute-heavy tasks like machine learning and data analytics. Existing platforms (e.g., Google Colab) are limited in scope. This project implements **NVIDIA MPS**, which enables multiple CUDA processes to **cooperatively** share GPU resources, reducing context-switching and improving utilization.

---
## 🔧 Methodology

### Techniques & Approach

- **Client-Server Architecture** using **gRPC** for fast, scalable communication.
- **Protocol Buffers** for efficient request serialization.
- **NVIDIA MPS** for virtualization and parallel GPU task execution.

### 🖼️ System Architecture

- **Single-Client Phase**: Established core GPU task execution via gRPC.
- **Multi-Client Support**: Implemented queueing mechanism for concurrent requests.
- **Virtualization**: Integrated MPS to run client tasks in parallel with optimized resource scheduling.

### 🛠 Tools & Technologies

- **Languages**: C++
- **Frameworks**: gRPC, CUDA
- **Serialization**: Protocol Buffers
- **Build Tools**: CMake

---
## 🧰 Prerequisites
Make sure you are using Linux.

- **Docker for Linux**: [Install Docker Engine on Linux](https://docs.docker.com/engine/install/)
- **NVIDIA Container Toolkit** *(if using GPUs)*: [Install Guide](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html)

To verify installation:

```bash
docker --version
nvidia-smi
```

## 📦 Getting Started

First, clone the main repository (which includes the Dockerfile):

```bash
git clone https://github.com/vxshxk/GPU-Buddy.git
cd GPU-Buddy
```

Alternatively, if you don't wanna develop, and just be an end user: Download the Dockerfile and follow the next steps in the same directory.

## 🐳 Docker Setup

### 🔧 Build the Docker Image

Inside the project directory, run:

```bash
docker build -t gpu-virtualization .
```
Verify Docker Image:

After building the image, run the following command to confirm it was created:

```bash
docker images
```

## ▶️ Run Docker Containers
Run the following containers for each component of the system:
   
   
🧩 Proxy Server Container
```bash
docker run -it --gpus all -p 50051:50051 --name Proxy_container gpu-virtualization bash
```
Now, once innside the container, to run the Proxy server:
```bash
./run_proxy.sh
```
   
   
🖥️ Server Container
```bash
docker run -it --gpus all -p 50052:50052 --name Server_container gpu-virtualization bash
```
Now, once innside the container, to run the Server:
```bash
./run_server.sh
```
   
   
💻 Client Container (Optional)
```bash
docker run -it --gpus all -p 50053:50053 --name Client_container gpu-virtualization bash
```
Now, once innside the container, to run the Server:
```bash
./run_client.sh
```

      
✅ Verify Container Status

After running the containers, make sure they are up and running using:

```bash
docker ps
```
---

      
## 📈 Results & Analysis

### ✅ Key Outcomes

- Seamless remote GPU execution through gRPC
- NVIDIA MPS integration for parallelism
- Efficient data handling using Protocol Buffers

### 📊 Performance Metrics

- **GPU Utilization**: Tracked multi-client workload handling
- **Scalability**: Measured responsiveness as concurrent clients increased

---
## ✅ Conclusion

GPU-Buddy showcases a scalable architecture for remote GPU sharing. It successfully enhances GPU usage and opens possibilities for secure, remote, high-performance computing environments.

---

