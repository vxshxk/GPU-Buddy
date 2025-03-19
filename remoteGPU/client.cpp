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

enum class MessageID : uint8_t {
    REGISTER = 0, AVAILABLE = 1
};

const std::string PREFIX_PATH = "../../";

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

        ClientContext context;
        std::unique_ptr<grpc::ClientReader<Output>> reader(stub_->Execute(&context, request));

        Output response;
        std::cout << "Execution Output (ID: " << file_id << "):\n";
        while (reader->Read(&response)) {
            std::string output_line(response.out().begin(), response.out().end()); 
            std::cout << output_line << std::flush;
        }

        Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "Execution failed: " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<RemoteGPU::Stub> stub_;
};

class ProxyClient {
    public:
        ProxyClient(std::shared_ptr<Channel> channel) : _stub{ProxyService::NewStub(channel)} {}

        void GetAvailableServers() {
            Empty request;

            auto* call = new AsyncClientCall<Empty, ServerList>;
            call->request = request;

            call->rpc = _stub->PrepareAsyncGetAvailableServers(&call->context, request, &_queue);
            call->rpc->StartCall();

            auto* tag = new Tag;
            tag->call = (void*)call;
            tag->id = MessageID::AVAILABLE;
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
                    case MessageID::AVAILABLE: {
                        auto* call = static_cast<AsyncClientCall<Empty, ServerList>*>(tag_ptr->call);
                        if (call) {
                            if (call->status.ok()) {
                                std::cout << "Available Servers:\n";
                                for (const auto& server : call->response.servers()) {
                                    std::cout << " - " << server.ip() << ":" << server.port() << std::endl;
                                }
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

void RunProxyClient() {
    std::string proxy_ip;
    std::cout << "Enter proxy server IP Address: ";
    std::cin >> proxy_ip;
    std::string proxy_address = proxy_ip + ":50051";    

    std::string server_address{proxy_address};
    ProxyClient client{grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials())};

    std::thread thread{&ProxyClient::AsyncCompleteRPC, &client};

    client.GetAvailableServers();

    thread.join();
}

int main(int argc, char** argv) {
    RunProxyClient();

    std::string selected_server;
    std::cout << "Enter server IP Address: ";
    std::cin >> selected_server;
    selected_server = selected_server + ":50052";

    std::string server_address(selected_server);
    RemoteGPUClient client(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

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
