#include <iostream>
#include <map>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "proxy.grpc.pb.h"
#include "proxy.pb.h"


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using proxy::ProxyService;
using proxy::ServerInfo;
using proxy::RegisterResponse;
using proxy::Empty;
using proxy::ServerList;

std::map<std::string, ServerInfo> server_registry;
std::mutex registry_mutex;

class ProxyServiceImpl final : public ProxyService::Service {
public:
    Status RegisterServer(ServerContext* context, const ServerInfo* request, RegisterResponse* response) override {
        std::lock_guard<std::mutex> lock(registry_mutex);
        std::string key = request->ip() + ":" + std::to_string(request->port());
        server_registry[key] = *request;
        response->set_message("Server registered successfully.");
        return Status::OK;
    }

    Status GetAvailableServers(ServerContext* context, const Empty* request, ServerList* response) override {
        std::lock_guard<std::mutex> lock(registry_mutex);
        for (const auto& entry : server_registry) {
            if (entry.second.available()) {
                *response->add_servers() = entry.second;
            }
        }
        return Status::OK;
    }
};

void RunProxyServer() {
    std::string server_address("0.0.0.0:50051");
    ProxyServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Proxy Server listening on " << server_address << std::endl;
    server->Wait();
}

int main() {
    RunProxyServer();
    return 0;
}
