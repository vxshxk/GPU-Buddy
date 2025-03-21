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
#include <csignal>
#include <atomic>

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
using proxy::ProxyResponse;

using namespace grpc;
using namespace proxy;

enum class MessageID : uint8_t { REGISTER = 0, AVAILABLE = 1, DELETE = 2 };
enum class ServiceID : uint8_t { UPLOAD = 0, DOWNLOAD = 1, EXECUTE = 2 };

const std::string PREFIX_PATH = "../../";

std::atomic<bool> shutdown_flag{false};

std::string GetLocalIPAddress() {
    const char* env_ip = std::getenv("SERVER_IP");
    if (env_ip) return std::string(env_ip);
    return "192.168.1.101"; 
}

bool checkGRPCCompatibility() {
    int status = std::system("nvidia-smi > /dev/null 2>&1");
    if (status != 0) return false;
    
    status = std::system("nvcc --version > /dev/null 2>&1");
    if (status != 0) return false;
    
    status = std::system("which grpc_cpp_plugin > /dev/null 2>&1");
    return status == 0;
}

class ProxyServerClient {
public:
    ProxyServerClient(std::shared_ptr<Channel> channel) : _stub{ProxyService::NewStub(channel)} {}

    void Register() {
        ServerInfo request;
        request.set_ip(GetLocalIPAddress());
        request.set_port(50052);

        auto* call = new AsyncClientCall<ServerInfo, ProxyResponse>;
        call->request = request;

        call->rpc = _stub->PrepareAsyncRegisterServer(&call->context, request, &_queue);
        call->rpc->StartCall();

        auto* tag = new Tag;
        tag->call = (void*)call;
        tag->id = MessageID::REGISTER;
        call->rpc->Finish(&call->response, &call->status, (void*)tag);
    }

    void DeleteServer() {
        ServerInfo request;
        request.set_ip(GetLocalIPAddress());
        request.set_port(50052);

        auto* call = new AsyncClientCall<ServerInfo, ProxyResponse>;
        call->request = request;

        call->rpc = _stub->PrepareAsyncDeleteServer(&call->context, request, &_queue);
        call->rpc->StartCall();

        auto* tag = new Tag;
        tag->call = (void*)call;
        tag->id = MessageID::DELETE;
        call->rpc->Finish(&call->response, &call->status, (void*)tag);
    }


    void AsyncCompleteRPC() {
        void* tag;
        bool ok = false;

        if (_queue.Next(&tag, &ok)) {
            auto* tag_ptr = static_cast<Tag*>(tag);
            if (!ok || !tag_ptr) abort();

            std::string err;
            switch (tag_ptr->id) {
                case MessageID::REGISTER: {
                    auto* call = static_cast<AsyncClientCall<ServerInfo, ProxyResponse>*>(tag_ptr->call);
                    if (call) {
                        if (!call->status.ok()) {
                            err = std::to_string(call->status.error_code()) + ": " + call->status.error_message();
                        }
                    } else {
                        err = "A client call was deleted";
                    }
                    delete call;
                    break;
                }
                case MessageID::DELETE: {
                    auto* call = static_cast<AsyncClientCall<ServerInfo, ProxyResponse>*>(tag_ptr->call);
                    if (call) {
                        if (call->status.ok()) {
                            std::cout << "Proxy server successfully notified about shutdown." << std::endl;
                        } else {
                            std::cerr << "Failed to notify proxy server: " << call->status.error_message() << std::endl;
                        }
                    } else {
                        err = "A client call was deleted";
                    }
                    delete call;
                    break;
                }
            }
            delete tag_ptr;

            if (!err.empty()) throw std::runtime_error(err);
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

class RemoteGPUServer {
public:
    static std::atomic<int> id;
    static std::unordered_map<int, std::pair<std::string, std::string>> index;

    ~RemoteGPUServer() {
        _server->Shutdown();
        _queue->Shutdown();
        this->id = 0;
        index.clear();
    }

    void Run(std::function<void()> shutdown_callback) {
        std::string server_address = GetLocalIPAddress() + ":50052";

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&_service);
        _queue = builder.AddCompletionQueue();
        _server = builder.BuildAndStart();

        std::cout << "Server listening on " << server_address << std::endl;
        std::thread rpc_thread([this]() { HandleRPCs(); });

        while (!shutdown_flag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        shutdown_callback();
        _server->Shutdown();
        _queue->Shutdown();
        rpc_thread.join();
    }

private:
    struct Tag {
        void* call;
        ServiceID id;
    };

    class CallData {
        public:
            CallData(RemoteGPU::AsyncService* service, ServerCompletionQueue* queue)
                    : _service{service}, _queue{queue}, _status{CallStatus::CREATE} {}

            virtual void Proceed() = 0;
        protected:
            RemoteGPU::AsyncService* _service;
            ServerCompletionQueue* _queue;
            ServerContext _context;
            enum class CallStatus {
                CREATE, PROCESS, FINISH, STREAMING
            };
            CallStatus _status;
    };

    class UploadFileCallData : public CallData {
        public:
            UploadFileCallData(RemoteGPU::AsyncService* service, ServerCompletionQueue* queue)
                : CallData{service, queue}, _responder{&_context} {
            _tag.id = ServiceID::UPLOAD;
            _tag.call = this;
            Proceed();
        }

        void Proceed() override {
            switch (_status) {
                case CallStatus::CREATE: {
                    _status = CallStatus::PROCESS;
                    _service->RequestUploadFile(&_context, &_request, &_responder, _queue, _queue, (void*)&_tag);
                    break;
                }
                case CallStatus::PROCESS: {
                    new UploadFileCallData{_service, _queue};
                    std::vector<std::string> code;
                    std::vector<std::string> commands;

                    int cur_id = id.fetch_add(1, std::memory_order_relaxed);
                    std::string OutputFilePath = PREFIX_PATH + "server_code" + std::to_string(cur_id) + ".py";
                    std::string OutputScriptPath = PREFIX_PATH + "server_script" + std::to_string(cur_id) + ".sh";

                    for (const auto& line : _request.code()) {
                        code.push_back(line);
                    }
                    for (const auto& command : _request.commands()) {
                        commands.push_back(command);
                    }

                    CodeRestorer::writePythonCode(OutputFilePath, OutputScriptPath, code, commands);

                    index[cur_id] = std::make_pair(OutputFilePath, OutputScriptPath);
                    _response.set_id(cur_id);
                    _status = CallStatus::FINISH;
                    _responder.Finish(_response, Status::OK, (void*)&_tag);
                    break;
                }
                default: {
                    delete this;
                }
            }
        }

        private:
            File _request;
            FileID _response;
            ServerAsyncResponseWriter<FileID> _responder;
            Tag _tag;
    };

    class DownloadFileCallData : public CallData {
        public:
            DownloadFileCallData(RemoteGPU::AsyncService* service, ServerCompletionQueue* queue)
                : CallData{service, queue}, _responder{&_context} {
            _tag.id = ServiceID::DOWNLOAD;
            _tag.call = this;
            Proceed();
        }

        void Proceed() override {
            switch (_status) {
                case CallStatus::CREATE: {
                    _status = CallStatus::PROCESS;
                    _service->RequestDownloadFile(&_context, &_request, &_responder, _queue, _queue, (void*)&_tag);
                    break;
                }
                case CallStatus::PROCESS: {
                    new DownloadFileCallData{_service, _queue};
                    int cur_id = _request.id();
                    auto it = index.find(cur_id);
                    if (it != index.end()) {
                        std::vector<std::string> code;
                        std::vector<std::string> commands;

                        std::string OutputFilePath = it->second.first;
                        std::string OutputScriptPath = it->second.second;

                        CodeExtractor::extractPythonScriptCode(OutputFilePath, OutputScriptPath, code, commands);

                        for (const auto& line : code) {
                            _response.add_code(line);
                        }
                        for (const auto& command : commands) {
                            _response.add_commands(command);
                        }

                        _status = CallStatus::FINISH;
                        _responder.Finish(_response, Status::OK, (void*)&_tag);
                    } else {
                        _status = CallStatus::FINISH;
                        _responder.Finish(_response, Status(grpc::NOT_FOUND, "File not found."), (void*)&_tag); 
                    }
                    break;
                }
                default: {
                    delete this;
                }
            }
        }

        private:
            FileID _request;
            File _response;
            ServerAsyncResponseWriter<File> _responder;
            Tag _tag;
    };

    class ExecuteCallData : public CallData {
        public:
            ExecuteCallData(RemoteGPU::AsyncService* service, ServerCompletionQueue* queue)
                : CallData{service, queue}, _responder{&_context}, _pipe{nullptr} {
                _tag.id = ServiceID::EXECUTE;
                _tag.call = this;
                Proceed();
            }

            void Proceed() override {
                switch (_status) {
                    case CallStatus::CREATE: {
                        _status = CallStatus::PROCESS;
                        _service->RequestExecute(&_context, &_request, &_responder, _queue, _queue, (void*)&_tag);
                        break;
                    }
                    case CallStatus::PROCESS: {
                        new ExecuteCallData{_service, _queue};

                        int cur_id = _request.id();
                        auto it = index.find(cur_id);

                        if (it != index.end()) {
                            std::string FilePath = it->second.first;
                            std::string ScriptPath = it->second.second;
                            std::string RunScript = "chmod +x " + ScriptPath + " && bash " + ScriptPath;
                            std::string RunCode = "python -u " + FilePath + " 2>&1";
                            _terminalExecute = RunScript + " && " + RunCode;

                            _pipe = popen(_terminalExecute.c_str(), "r");

                            if (!_pipe) {
                                _status = CallStatus::FINISH;
                                _responder.Finish(Status(grpc::INTERNAL, "Failed to execute."), (void*)&_tag);
                                return;
                            }

                            _status = CallStatus::STREAMING;
                            Proceed();
                        } else {
                            _status = CallStatus::FINISH;
                            _responder.Finish(Status(grpc::NOT_FOUND, "File not found."), (void*)&_tag);
                        }
                        break;
                    }
                    case CallStatus::STREAMING: {
                        char buffer[1024];
                        if (fgets(buffer, sizeof(buffer), _pipe) != nullptr) {
                            Output response;
                            response.set_out(std::string(buffer, strlen(buffer)));
                            _responder.Write(response, (void*)&_tag);
                        } else {
                            pclose(_pipe);
                            _status = CallStatus::FINISH;
                            _responder.Finish(Status::OK, (void*)&_tag);
                        }
                        break;
                    }
                    default: {
                        delete this;
                    }
                }
            }

        private:
            FileID _request;
            Output _response;
            ServerAsyncWriter<Output> _responder;
            Tag _tag;
            FILE* _pipe;
            std::string _terminalExecute;
        };

    void HandleRPCs() {
        new UploadFileCallData{&_service, _queue.get()};
        new DownloadFileCallData{&_service, _queue.get()};
        new ExecuteCallData{&_service, _queue.get()};
        void* tag;
        bool ok;
        while (true) {
            if (_queue->Next(&tag, &ok) && ok) {
                auto* tag_ptr = static_cast<Tag*>(tag);
                switch (tag_ptr->id) {
                    case ServiceID::UPLOAD: {
                        static_cast<UploadFileCallData*>(tag_ptr->call)->Proceed();
                        break;
                    }
                    case ServiceID::DOWNLOAD: {
                        static_cast<DownloadFileCallData*>(tag_ptr->call)->Proceed();
                        break;
                    }
                    case ServiceID::EXECUTE: {
                        static_cast<ExecuteCallData*>(tag_ptr->call)->Proceed();
                        break;
                    }
                }
            } else {
                std::cerr << "Server shutting down" << std::endl;
                break;
            }
        }
    }

    RemoteGPU::AsyncService _service;
    std::unique_ptr<ServerCompletionQueue> _queue;
    std::unique_ptr<Server> _server;
};

std::atomic<int> RemoteGPUServer::id{0};
std::unordered_map<int, std::pair<std::string, std::string>> RemoteGPUServer::index;

void SignalHandler(int signal) {
    if (signal == SIGINT) shutdown_flag = true;
}

void RunProxyServerClient() {
    std::string proxy_ip;
    std::cout << "Enter proxy server IP Address: ";
    std::cin >> proxy_ip;
    std::string proxy_address = proxy_ip + ":50051";    

    ProxyServerClient client{grpc::CreateChannel(proxy_address, grpc::InsecureChannelCredentials())};
    std::thread thread{&ProxyServerClient::AsyncCompleteRPC, &client};

    client.Register();

    auto shutdown_callback = [&client]() { client.DeleteServer(); };

    RemoteGPUServer server;
    server.Run(shutdown_callback);

    thread.join();
}

int main(int argc, char** argv) {
    if (!checkGRPCCompatibility()) {
        exit(EXIT_FAILURE);
    }

    std::signal(SIGINT, SignalHandler);

    RunProxyServerClient();

    return 0;
}
