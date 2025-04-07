# GPU-Buddy:Remote GPU Sharing with Virtualization

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

