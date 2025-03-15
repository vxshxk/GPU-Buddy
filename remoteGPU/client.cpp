#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "gpu.grpc.pb.h"
#include "proxy.pb.h"
#include "proxy.grpc.pb.h"
#include "headers/CodeExtractor.h"
#include "headers/CodeRestorer.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using remoteGPU::RemoteGPU;
using remoteGPU::File;
using remoteGPU::FileID;
using remoteGPU::Output;

using proxy::ProxyService;
using proxy::Empty;
using proxy::ServerList;

const std::string PREFIX_PATH = "../";

class ProxyClient {
    public:
        ProxyClient(std::shared_ptr<Channel> channel) : stub_(ProxyService::NewStub(channel)) {}
    
        void GetAvailableServers() {
            Empty request;
            ServerList response;
            ClientContext context;
    
            Status status = stub_->GetAvailableServers(&context, request, &response);
            if (status.ok()) {
                std::cout << "Available Servers:\n";
                for (const auto& server : response.servers()) {
                    std::cout << " - " << server.ip() << ":" << server.port() << std::endl;
                }
            } else {
                std::cerr << "Failed to get available servers from proxy." << std::endl;
            }
        }
    
    private:
        std::unique_ptr<ProxyService::Stub> stub_;
    };

class RemoteGPUClient {
public:
    RemoteGPUClient(std::shared_ptr<grpc::Channel> channel) 
        : stub_(RemoteGPU::NewStub(channel)) {}

    int UploadFile(const std::vector<std::string>& code, const std::vector<std::string>& commands) {
        File request;
        for (const auto& line : code) {
            request.add_code(line);
        }
        for (const auto& command : commands) {
            request.add_commands(command);
        }

        FileID reply;
        ClientContext context;
        Status status = stub_->UploadFile(&context, request, &reply);

        if (status.ok()) {
            std::cout << "File uploaded successfully! Assigned ID: " << reply.id() << std::endl;
            return reply.id();
        } else {
            std::cerr << "Upload failed: " << status.error_message() << std::endl;
            return -1;
        }
    }

    void DownloadFile(int file_id) {
        FileID request;
        request.set_id(file_id);

        File reply;
        ClientContext context;
        Status status = stub_->DownloadFile(&context, request, &reply);

        if (status.ok()) {
            std::cout << "Downloaded file (ID: " << file_id << "):\n";
            std::vector<std::string> code;
            std::vector<std::string> commands;
            for (const auto& line : reply.code()) {
                code.push_back(line);
            }
            for (const auto& command : reply.commands()) {
                commands.push_back(command);
            }
            std::string OutputFilePath = PREFIX_PATH + "client_code" + std::to_string(file_id) + ".py";
            std::string OutputScriptPath = PREFIX_PATH + "client_script" + std::to_string(file_id) + ".sh";
            CodeRestorer::writePythonCode(OutputFilePath, OutputScriptPath, code, commands);
        } else {
            std::cerr << "Download failed: " << status.error_message() << std::endl;
        }
    }

    void Execute(int file_id) {
        FileID request;
        request.set_id(file_id);

        Output reply;
        ClientContext context;
        Status status = stub_->Execute(&context, request, &reply);

        if (status.ok()) {
            std::cout << "Executed file (ID: " << file_id << "):\n";
            std::vector<std::string> output;

            for (const auto& line : reply.out()) {
                output.push_back(line);
            }

            std::cout << "Output:\n";
            for (const auto& line: output) {
                std::cout << line << std::endl;
            }            

        } else {
            std::cerr << "Download failed: " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<RemoteGPU::Stub> stub_;
};

int main(int argc, char** argv) {
    std::string proxy_address = "localhost:50051"; // Proxy Server Address
    ProxyClient proxy(grpc::CreateChannel(proxy_address, grpc::InsecureChannelCredentials()));

    proxy.GetAvailableServers();

    // Get available servers and select one dynamically
    Empty request;
    ServerList response;
    ClientContext context;

    std::unique_ptr<ProxyService::Stub> proxy_stub = ProxyService::NewStub(grpc::CreateChannel(proxy_address, grpc::InsecureChannelCredentials()));
    Status status = proxy_stub->GetAvailableServers(&context, request, &response);

    if (!status.ok() || response.servers_size() == 0) {
        std::cerr << "No available servers found. Exiting..." << std::endl;
        return 1;
    }

    // Select the first available server (you can implement a better selection strategy)
    std::string selected_server = response.servers(0).ip() + ":" + std::to_string(response.servers(0).port());
    std::cout << "Selected server: " << selected_server << std::endl;

    // Connect to the selected RemoteGPU server
    RemoteGPUClient client(grpc::CreateChannel(selected_server, grpc::InsecureChannelCredentials()));

    std::string file_name;
    std::cout << "Enter file name: ";
    std::cin >> file_name;

    std::string InputFilePath = PREFIX_PATH + file_name;
    std::vector<std::string> code;
    std::vector<std::string> commands;
    CodeExtractor::extractPythonCode(InputFilePath, code, commands);

    int file_id = client.UploadFile(code, commands);
    std::cout << file_id << std::endl;

    client.DownloadFile(file_id);
    client.Execute(file_id);

    return 0;
}
