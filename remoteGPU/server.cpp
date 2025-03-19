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

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using remoteGPU::RemoteGPU;
using remoteGPU::File;
using remoteGPU::FileID;
using remoteGPU::Output;

using grpc::Channel;
using grpc::ClientContext;
using proxy::ProxyService;
using proxy::ServerInfo;
using proxy::RegisterResponse;

using namespace grpc;
using namespace proxy;

enum class MessageID : uint8_t {
    REGISTER = 0, AVAILABLE = 1
};

const std::string PREFIX_PATH = "../../";

std::string GetLocalIPAddress() {
    const char* env_ip = std::getenv("SERVER_IP");
    if (env_ip) return std::string(env_ip);
    return "192.168.1.101"; 
}

bool checkGRPCCompatibility() {
    int status = std::system("nvidia-smi > /dev/null 2>&1");
    if (status != 0) {
        std::cerr << "Error: NVIDIA GPU not detected or drivers are not installed." << std::endl;
        return false;
    }
    
    status = std::system("nvcc --version > /dev/null 2>&1");
    if (status != 0) {
        std::cerr << "Error: CUDA compiler (nvcc) not found. Ensure CUDA is installed properly." << std::endl;
        return false;
    }
    
    status = std::system("which grpc_cpp_plugin > /dev/null 2>&1");
    if (status != 0) {
        std::cerr << "Error: gRPC C++ plugin not found. Ensure gRPC is installed." << std::endl;
        return false;
    }
    
    return true;
}

class RemoteGPUServiceImpl final : public RemoteGPU::Service {
public:
    RemoteGPUServiceImpl() {
        this->id = 0;
    }

    Status UploadFile(ServerContext* context, const File* request, FileID* reply) override {
        std::vector<std::string> code;
        std::vector<std::string> commands;

        int cur_id = id.fetch_add(1, std::memory_order_relaxed);
        std::string OutputFilePath = PREFIX_PATH + "server_code" + std::to_string(cur_id) + ".py";
        std::string OutputScriptPath = PREFIX_PATH + "server_script" + std::to_string(cur_id) + ".sh";

        for (const auto& line : request->code()) {
            code.push_back(line);
        }
        for (const auto& command : request->commands()) {
            commands.push_back(command);
        }

        CodeRestorer::writePythonCode(OutputFilePath, OutputScriptPath, code, commands);

        index[cur_id] = std::make_pair(OutputFilePath, OutputScriptPath);
        reply->set_id(cur_id);

        return Status::OK;
    }

    Status DownloadFile(ServerContext* context, const FileID* request, File* reply) override {
        int cur_id = request->id();
        auto it = index.find(cur_id);
        if (it != index.end()) {
            std::vector<std::string> code;
            std::vector<std::string> commands;

            std::string OutputFilePath = it->second.first;
            std::string OutputScriptPath = it->second.second;

            CodeExtractor::extractPythonScriptCode(OutputFilePath, OutputScriptPath, code, commands);

            for (const auto& line : code) {
                reply->add_code(line);
            }
            for (const auto& command : commands) {
                reply->add_commands(command);
            }

            return Status::OK;
        } else {
            return Status(grpc::NOT_FOUND, "File not found.");
        }
    }

    Status Execute(ServerContext* context, const FileID* request, grpc::ServerWriter<Output>* writer) override {
        int cur_id = request->id();
        auto it = index.find(cur_id);
        if (it != index.end()) {
            std::string FilePath = it->second.first;
            std::string ScriptPath = it->second.second;
            std::string RunScript = "chmod +x " + ScriptPath + " && bash " + ScriptPath;       
            std::string RunCode = "python -u " + FilePath + " 2>&1";
            std::string TerminalExecute =  RunScript + " && " + RunCode ;
            FILE* pipe = popen(TerminalExecute.c_str(), "r");
            if (!pipe) {
                return Status(grpc::INTERNAL, "Failed to execute script.");
            }
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                Output response;
                response.set_out(std::string(buffer, strlen(buffer)));  
                writer->Write(response);
                }
            pclose(pipe);

                return Status::OK;
        }
        else {
            return Status(grpc::NOT_FOUND, "File not found.");
        }
    }

private:
    std::atomic<int> id;
    std::unordered_map<int, std::pair<std::string, std::string>> index;
};

void RunServer() {
    std::string server_ip = GetLocalIPAddress(); 
    std::string server_address = server_ip + ":50052";
    RemoteGPUServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on: " << server_address << std::endl;

    server->Wait();
}

class ProxyServerClient {
    public:
        ProxyServerClient(std::shared_ptr<Channel> channel) : _stub{ProxyService::NewStub(channel)} {}

        void Register() {
            ServerInfo request;

            std::string server_ip = GetLocalIPAddress(); 
            int server_port = 50052;
            bool available = true;

            request.set_ip(server_ip);
            request.set_port(server_port);
            request.set_available(available);

            auto* call = new AsyncClientCall<ServerInfo, RegisterResponse>;
            call->request = request;

            call->rpc = _stub->PrepareAsyncRegisterServer(&call->context, request, &_queue);
            call->rpc->StartCall();

            auto* tag = new Tag;
            tag->call = (void*)call;
            tag->id = MessageID::REGISTER;
            call->rpc->Finish(&call->response, &call->status, (void*)tag);
        }

        void AsyncCompleteRPC() {
            void* tag;
            bool ok = false;

            if (_queue.Next(&tag, &ok)) {
                auto* tag_ptr = static_cast<Tag*>(tag);
                if (!ok || !tag_ptr) {
                    std::cerr << "Something went wrong" << std::endl;
                    abort();
                }

                std::string err;
                switch (tag_ptr->id) {
                    case MessageID::REGISTER: {
                        auto* call = static_cast<AsyncClientCall<ServerInfo, RegisterResponse>*>(tag_ptr->call);
                        if (call) {
                            if (call->status.ok()) {
                                std::cout << "Proxy Response: " << call->response.message() << std::endl;
                            } else {
                                err = std::to_string(call->status.error_code()) + ": " + call->status.error_message();
                            }
                        } else {
                            err = "A client call was deleted";
                        }
                        delete call;
                        break;
                    }
                }
                delete tag_ptr;

                if (!err.empty()) {
                    throw std::runtime_error(err);
                }
            }
        }

    private:
        template <class RequestType, class ResponseType>
        struct AsyncClientCall {
            RequestType request;
            ResponseType response;
            ClientContext context;
            Status status;
            std::unique_ptr<ClientAsyncResponseReader<ResponseType>> rpc;
        };

        struct Tag {
            void* call;
            MessageID id;
        };

        std::unique_ptr<ProxyService::Stub> _stub;
        CompletionQueue _queue;
};

void RunProxyServerClient() {
    std::string proxy_ip;
    std::cout << "Enter proxy server IP Address: ";
    std::cin >> proxy_ip;
    std::string proxy_address = proxy_ip + ":50051";    

    std::string server_address{proxy_address};
    ProxyServerClient client{grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials())};

    std::thread thread{&ProxyServerClient::AsyncCompleteRPC, &client};

    client.Register();

    thread.join();
}

int main(int argc, char** argv) {
    if (!checkGRPCCompatibility()) {
        std::cerr << "Error: gRPC is not compatible with this environment. Please check NVIDIA GPU drivers and settings." << std::endl;
        exit(EXIT_FAILURE);
    }

    RunProxyServerClient();

    RunServer();
    return 0;
}
