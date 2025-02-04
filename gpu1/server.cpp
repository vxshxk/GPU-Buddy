#include <iostream>
#include <grpcpp/grpcpp.h>
#include "gpu.grpc.pb.h"
#include "CodeRestorer.h"
using grpc::Server;
using grpc::ServerContext;
using grpc::Status;
using example::RemoteGPU;
using example::File;
using example::StringResponse;
std::string filePath = "/root/gpu1/output.py";
std::string scriptPath="/root/gpu1/output_script.py";
class RemoteGPUServiceImpl final : public RemoteGPU::Service {
public:
    Status SendStrings(ServerContext* context, const File* request, StringResponse* response) override {
        std::vector<std::string>code;
        std::vector<std::string>commands;
        std::cout << "Received code: " << std::endl;
        for (const auto& line : request->code()) {
            std::cout << line << std::endl;
            code.push_back(line);
        }
        
        std::cout << "Received commands: " << std::endl;
        for (const auto& command : request->commands()) {
            std::cout << command << std::endl;
            commands.push_back(command);
        }
        CodeRestorer::writePythonCode(filePath,scriptPath,code,commands);
        response->set_message("Data received successfully!");

        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    RemoteGPUServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
}

int main() {
    RunServer();
    return 0;
}
