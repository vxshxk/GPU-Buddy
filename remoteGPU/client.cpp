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
    REGISTER = 0, AVAILABLE = 1, DELETE = 2
};

enum class ServiceID : uint8_t {
    UPLOAD = 0, DOWNLOAD = 1, EXECUTE = 2
};

const std::string PREFIX_PATH = "../../";

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
            CodeExtractor::extractPythonCode(InputFilePath, code, commands);

            for (const auto& line : code) {
                request.add_code(line);
            }
            for (const auto& command : commands) {
                request.add_commands(command);
            }

            auto* call = new AsyncClientCall<File, FileID>;
            call->request = request;

            call->rpc = _stub->PrepareAsyncUploadFile(&call->context, request, &_queue);
            call->rpc->StartCall();

            auto* tag = new Tag;
            tag->call = (void*)call;
            tag->id = ServiceID::UPLOAD;
            call->rpc->Finish(&call->response, &call->status, (void*)tag);
        }

        void DownloadFile() {
            FileID request;
            int file_id;

            std::cout << "Enter FileID: ";
            std::cin >> file_id;

            request.set_id(file_id);

            auto* call = new AsyncClientCall<FileID, File>;
            call->request = request;

            call->rpc = _stub->PrepareAsyncDownloadFile(&call->context, request, &_queue);
            call->rpc->StartCall();

            auto* tag = new Tag;
            tag->call = (void*)call;
            tag->id = ServiceID::DOWNLOAD;
            call->rpc->Finish(&call->response, &call->status, (void*)tag);
        }

        void Execute() {
            FileID request;
            int file_id;

            std::cout << "Enter FileID: ";
            std::cin >> file_id;

            request.set_id(file_id);
         
            auto* call = new AsyncClientCall<FileID, Output>;
           
            call->request = request;
       
            call->rpc_stream = _stub->PrepareAsyncExecute(&call->context, request, &_queue);
         
            call->rpc_stream->StartCall((void*)call);
            auto* tag = new Tag;
            tag->call = (void*)call;
            tag->id = ServiceID::EXECUTE;
          
            call->rpc_stream->Read(&call->response, (void*)tag);
          
        }

        void AsyncCompleteRPC() {
            void* tag;
            bool ok;

            while (_queue.Next(&tag, &ok)) {
                auto* tag_ptr = static_cast<Tag*>(tag);

                switch (tag_ptr->id) {
                    case ServiceID::UPLOAD: {
                        auto* call = static_cast<AsyncClientCall<File, FileID>*>(tag_ptr->call);
                        if (call->status.ok()) {
                            std::cout << "File uploaded successfully! Assigned ID: " << call->response.id() << std::endl;
                        } 
                        else {
                            std::cerr << "Upload failed: " << call->status.error_message() << std::endl;
                        }
                        delete call;
                        break;
                    }
                    case ServiceID::DOWNLOAD: {
                        auto* call = static_cast<AsyncClientCall<FileID, File>*>(tag_ptr->call);
                        if (call->status.ok()) {
                            std::cout << "Downloaded file (ID: " << call->request.id() << ")\n";

                            std::vector<std::string> code;
                            std::vector<std::string> commands;
                            for (const auto& line : call->response.code()) {
                                code.push_back(line);
                            }
                            for (const auto& command : call->response.commands()) {
                                commands.push_back(command);
                            }

                            std::string OutputFilePath = PREFIX_PATH + "client_code" + std::to_string(call->request.id()) + ".py";
                            std::string OutputScriptPath = PREFIX_PATH + "client_script" + std::to_string(call->request.id()) + ".sh";
                            CodeRestorer::writePythonCode(OutputFilePath, OutputScriptPath, code, commands);
                        } else {
                            std::cerr << "Download failed: " << call->status.error_message() << std::endl;
                        }
                        delete call;
                        break;
                    }
                    case ServiceID::EXECUTE: {
                       
                        auto* call = static_cast<AsyncClientCall<FileID, Output>*>(tag_ptr->call);

                        std::cout << "Output received:\n" << call->response.out();
                        if (ok && call->status.ok()) {                            
                            call->rpc_stream->Read(&call->response, (void*)tag);  
                        } else {
                            call->rpc_stream->Finish(&call->status, (void*)tag); 
                        }

                        if (!call->status.ok()) {
                            std::cerr << "Error in Execute RPC: " << call->status.error_message() << std::endl;
                        }

                        delete call;
                        break;
                    }
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
            std::unique_ptr<ClientAsyncReader<Output>> rpc_stream;
        };

        struct Tag {
            void* call;
            ServiceID id;
        };

        std::unique_ptr<RemoteGPU::Stub> _stub;
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

void RunClient() {
    std::string selected_server;
    std::cout << "Enter server IP Address: ";
    std::cin >> selected_server;
    selected_server = selected_server + ":50052";

    std::string server_address(selected_server);
    RemoteGPUClient client(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

    std::thread thread{&RemoteGPUClient::AsyncCompleteRPC, &client};

    int choice;
    do {
        std::cout << "\nChoose an option:\n"
                  << "1. Upload File\n"
                  << "2. Download File\n"
                  << "3. Execute File\n"
                  << "4. Exit\n"
                  << "Enter choice: ";
        std::cin >> choice;

        switch (choice) {
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
    } while (choice != 4);

    thread.join();
}

int main(int argc, char** argv) {
    RunProxyClient();

    RunClient();

    return 0;
}
