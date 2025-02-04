#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "gpu.grpc.pb.h"
#include "CodeExtractor.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using example::RemoteGPU;
using example::File;
using example::StringResponse;

class RemoteGPUClient {
public:
    RemoteGPUClient(std::shared_ptr<Channel> channel)
        : stub_(RemoteGPU::NewStub(channel)) {}
    void SendStrings(const std::vector<std::string>& code, const std::vector<std::string>& commands) {
        File request;
        for (const auto& str : code) {
            request.add_code(str);
        }

        for (const auto& cmd : commands) {
            request.add_commands(cmd);
        }

        StringResponse response;
        ClientContext context;

        Status status = stub_->SendStrings(&context, request, &response);

        if (status.ok()) {
            std::cout << "Server Response: " << response.message() << std::endl;
        } else {
            std::cerr << "gRPC call failed: " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<RemoteGPU::Stub> stub_;
};

int main() {

    RemoteGPUClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    std::string filePath = "/root/gpu1/input.py";
    std::vector<std::string> code;
    std::vector<std::string> commands;
    CodeExtractor::extractPythonCode(filePath, code,commands);
    client.SendStrings(code, commands);

    return 0;
}
