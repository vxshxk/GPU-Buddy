#include <bits/stdc++.h>
#include <grpcpp/grpcpp.h>
#include <cstdlib>
#include <fstream>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#include "proxy.pb.h"
#include "proxy.grpc.pb.h"
#include "gpu.grpc.pb.h"
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

using namespace grpc;
using namespace proxy;

const std::string PREFIX_PATH = "../../";

class ProxyClient {
    public:
        ProxyClient(std::shared_ptr<Channel> channel) : _stub{ProxyService::NewStub(channel)} {}

        void GetAvailableServers() {
            available_servers.clear();
            Empty request;
            ClientContext context;
            ServerList response;

            Status status = _stub->GetAvailableServers(&context, request, &response);

            if (status.ok()) {
                std::cout << "Available Servers:\n";
                for (const auto& server : response.servers()) {
                    std::string server_address = server.ip() + ":" + std::to_string(server.port());
                    available_servers.push_back(server_address);
                    gpu_names.push_back(server.gpu_name());
                }
            } else {
                std::cerr << "Error: " << status.error_message() << std::endl;
            }
        }
        const std::vector<std::string>& getAvailableServers() const {
            return available_servers;
        }
        const std::vector<std::string>& getAvailableGPUs() const {
            return gpu_names;
        }
    private:
        std::unique_ptr<ProxyService::Stub> _stub;
        std::vector<std::string> available_servers;
        std::vector<std::string> gpu_names;
};

class RemoteGPUClient {
    public:
        RemoteGPUClient(std::shared_ptr<Channel> channel) : _stub{RemoteGPU::NewStub(channel)} {}

        void UploadFile() {
            File request;
            std::string file_name;
            std::cout << "Enter file name: ";
            std::cin >> file_name;
            
            std::string InputFilePath = PREFIX_PATH + file_name;
            std::vector<std::string> code;
            std::vector<std::string> commands;
            bool fileExists = CodeExtractor::extractPythonCode(InputFilePath, code, commands);
            if (!fileExists) {
                std::cerr << "File does not exist. Please enter a valid file name" << std::endl;
                return;
            }
            for (const auto& line : code) request.add_code(line);
            for (const auto& command : commands) request.add_commands(command);

            ClientContext context;
            FileID response;
            Status status = _stub->UploadFile(&context, request, &response);
            
            if (status.ok()) {
                std::cout << "File uploaded successfully! Assigned ID: " << response.id() << std::endl;
            } else {
                std::cerr << "Upload failed: " << status.error_message() << std::endl;
            }
        }

        void DownloadFile() {
            FileID request;
            int file_id;
            std::cout << "Enter FileID: ";
            std::cin >> file_id;
            request.set_id(file_id);

            ClientContext context;
            File response;
            Status status = _stub->DownloadFile(&context, request, &response);

            if (status.ok()) {
                std::cout << "Downloaded file (ID: " << request.id() << ")\n";
                std::vector<std::string> code(response.code().begin(), response.code().end());
                std::vector<std::string> commands(response.commands().begin(), response.commands().end());
                
                std::string OutputFilePath = PREFIX_PATH + "client_code" + std::to_string(request.id()) + ".py";
                std::string OutputScriptPath = PREFIX_PATH + "client_script" + std::to_string(request.id()) + ".sh";
                CodeRestorer::writePythonCode(OutputFilePath, OutputScriptPath, code, commands);
            } else {
                std::cerr << "Download failed: " << status.error_message() << std::endl;
            }
        }

        void Execute() {
            FileID request;
            int file_id;
            std::cout << "Enter FileID: ";
            std::cin >> file_id;
            request.set_id(file_id);

            ClientContext context;
            std::unique_ptr<ClientReader<Output>> reader(_stub->Execute(&context, request));

            Output output;
            std::cout << "Output received:\n";
            while (reader->Read(&output)) {
                std::string output_line(output.out().begin(), output.out().end()); 
                std::cout << output_line << std::flush;
            }

            Status status = reader->Finish();
            if (!status.ok()) {
                std::cerr << "Error in Execute RPC: " << status.error_message() << std::endl;
            }
        }

    private:
        std::unique_ptr<RemoteGPU::Stub> _stub;
};

void Welcome() {
    std::cout << " __        __  _____   _        ____    ___    __  __   _____ " << std::endl;
    std::cout << " \\ \\      / / | ____| | |      / ___|  / _ \\  |  \\/  | | ____|" << std::endl;
    std::cout << "  \\ \\ /\\ / /  |  _|   | |     | |     | | | | | |\\/| | |  _|  " << std::endl;
    std::cout << "   \\ V  V /   | |___  | |___  | |___  | |_| | | |  | | | |___ " << std::endl;
    std::cout << "    \\_/\\_/    |_____| |_____|  \\____|  \\___/  |_|  |_| |_____|" << std::endl << std::endl;

    std::cout << "   _______________                         ||*\\_/*|________" << std::endl;
    std::cout << "  |  ___________  |     .-.     .-.        ||_/-\\_|______  |" << std::endl;
    std::cout << "  | |           | |    .****. .****.       | |           | |" << std::endl;
    std::cout << "  | |   0   0   | |    .*****.*****.       | |   0   0   | |" << std::endl;
    std::cout << "  | |     -     | |     .*********.        | |     -     | |" << std::endl;
    std::cout << "  | |   |___|   | |      .*******.         | |   |___|   | |" << std::endl;
    std::cout << "  | |___________| |       .*****.          | |___________| |" << std::endl;
    std::cout << "  |_____|   |_____|        .***.           |_______________|" << std::endl;
    std::cout << "    _|__|   |_|_.............*..............._|________|_" << std::endl;
    std::cout << "   / ********** \\                          / ********** \\" << std::endl;
    std::cout << " /  ************  \\                      /  ************  \\" << std::endl;
    std::cout << "--------------------                     -------------------" << std::endl << std::endl;

    std::cout << "   ____   ____    _   _      ____    _   _   ____    ____   __   __" << std::endl;
    std::cout << "  / ___| |  _ \\  | | | |    | __ )  | | | | |  _ \\  |  _ \\  \\ \\ / /" << std::endl;
    std::cout << " | |  _  | |_) | | | | |    |  _ \\  | | | | | | | | | | | |  \\ V / " << std::endl;
    std::cout << " | |_| | |  __/  | |_| |    | |_)|  | |_| | | |_| | | |_| |   | |  " << std::endl;
    std::cout << "  \\____| |_|      \\___/     |____/   \\___/  |____/  |____/    |_|  " << std::endl << std::endl;
}

void RunClient(ProxyClient& proxyClient) {
    const std::vector<std::string>& available_servers = proxyClient.getAvailableServers();
    const std::vector<std::string>& gpu_names = proxyClient.getAvailableGPUs();
    if (available_servers.empty()) {
        std::cerr << "No available servers found. Exiting...\n";
        return;
    }

    std::cout << "Available Servers:\n";
    for (size_t i = 0; i < available_servers.size(); ++i) {
        std::cout << i + 1 << ". " << available_servers[i]  << " | GPU: " << gpu_names[i] << std::endl;
    }

    int choice;
    std::string selected_server;
    while (true) {
        std::cout << "Select a server by number: ";
        std::cin >> choice;

        if (choice >= 1 && choice <= available_servers.size()) {
            selected_server = available_servers[choice - 1];
            break;
        } else {
            std::cout << "Invalid choice. Please try again.\n";
        }
    }

    RemoteGPUClient client(grpc::CreateChannel(selected_server, grpc::InsecureChannelCredentials()));

    int option;
    do {
        std::cout << "\nChoose an option:\n"
                  << "1. Upload File\n"
                  << "2. Download File\n"
                  << "3. Execute File\n"
                  << "4. Exit\n"
                  << "Enter choice: ";
        std::cin >> option;
        switch (option) {
            case 1:
                client.UploadFile();
                break;
            case 2:
                client.DownloadFile();
                break;
            case 3:
                client.Execute();
                break;
            case 4:
                std::cout << "Exiting...\n";
                break;
            default:
                std::cout << "Invalid choice. Please try again.\n";
                break;
        }
    } while (option != 4);
}
void RunProxyClient() {
    std::string proxy_ip;
    ProxyClient* proxyClient = nullptr;
    while (true) {
        std::cout << "Enter proxy server IP Address: ";
        std::cin >> proxy_ip;
        std::string proxy_address = proxy_ip + ":50051";
        auto channel = grpc::CreateChannel(proxy_address, grpc::InsecureChannelCredentials());
        std::cout << "Connecting to proxy server at " << proxy_address << "...\n";
        if (!channel->WaitForConnected(gpr_time_add(
                gpr_now(GPR_CLOCK_REALTIME),
                gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
            std::cerr << "Failed to connect to proxy server. Please try again.\n";
            continue;
        }

        std::cout << "Connected successfully!\n";
        proxyClient = new ProxyClient (channel);
        proxyClient->GetAvailableServers();
        break; 
    }
    if (proxyClient) {
        RunClient(*proxyClient);
    }
}




int main(int argc, char** argv) {
    Welcome();
    RunProxyClient();
    return 0;
}