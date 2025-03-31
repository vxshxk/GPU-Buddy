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
using proxy::ProxyResponse;
using proxy::Empty;
using proxy::ServerList;

using namespace grpc;
using namespace proxy;

enum class MessageID : uint8_t {
    REGISTER = 0, AVAILABLE = 1, DELETE = 2
};

std::map<std::string, ServerInfo> server_registry;

class ProxyServer {
    public:
        ~ProxyServer() {
            _server->Shutdown();
            _queue->Shutdown();
        }

        void Run() {
            std::string server_address("0.0.0.0:50051");
            ServerBuilder builder;
            builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
            builder.RegisterService(&_service);
            _queue = builder.AddCompletionQueue();
            _server = builder.BuildAndStart();

            std::cout << "Server listening on " << server_address << std::endl;
            HandleRPCs();
        }
    
    private:
        struct Tag {
            void* call;
            MessageID id;
        };

        class CallData {
            public:
                CallData(ProxyService::AsyncService* service, ServerCompletionQueue* queue)
                        : _service{service}, _queue{queue}, _status{CallStatus::CREATE} {}

                virtual void Proceed() = 0;

            protected:
                ProxyService::AsyncService* _service;
                ServerCompletionQueue* _queue;
                ServerContext _context;
                enum class CallStatus {
                    CREATE, PROCESS, FINISH
                };
                CallStatus _status;
        };

        class RegisterServerCallData : public CallData {
            public:
                RegisterServerCallData(ProxyService::AsyncService* service, ServerCompletionQueue* queue)
                    : CallData{service, queue}, _responder{&_context} {
                _tag.id = MessageID::REGISTER;
                _tag.call = this;
                Proceed();
            }

            void Proceed() override {
                switch (_status) {
                    case CallStatus::CREATE: {
                        _status = CallStatus::PROCESS;
                        _service->RequestRegisterServer(&_context, &_request, &_responder, _queue, _queue, (void*)&_tag);
                        break;
                    }
                    case CallStatus::PROCESS: {
                        new RegisterServerCallData{_service, _queue};
                        std::string key = _request.ip() + ":" + std::to_string(_request.port());
                        server_registry[key] = _request;
                        _response.set_message("Server registered successfully with GPU: " + _request.gpu_name());
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
                ServerInfo _request;
                ProxyResponse _response;
                ServerAsyncResponseWriter<ProxyResponse> _responder;
                Tag _tag;
        };

        class GetAvailableServersCallData : public CallData {
            public:
                GetAvailableServersCallData(ProxyService::AsyncService* service, ServerCompletionQueue* queue)
                    : CallData{service, queue}, _responder{&_context} {
                _tag.id = MessageID::AVAILABLE;
                _tag.call = this;
                Proceed();
            }

            void Proceed() override {
                switch (_status) {
                    case CallStatus::CREATE: {
                        _status = CallStatus::PROCESS;
                        _service->RequestGetAvailableServers(&_context, &_request, &_responder, _queue, _queue, (void*)&_tag);
                        break;
                    }
                    case CallStatus::PROCESS: {
                        new GetAvailableServersCallData{_service, _queue};
                        for (const auto& entry : server_registry) {
                            *(_response.add_servers()) = entry.second;
                        }
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
                Empty _request;
                ServerList _response;
                ServerAsyncResponseWriter<ServerList> _responder;
                Tag _tag;
        };

        class DeleteServerCallData : public CallData {
            public:
                DeleteServerCallData(ProxyService::AsyncService* service, ServerCompletionQueue* queue)
                    : CallData{service, queue}, _responder{&_context} {
                _tag.id = MessageID::DELETE;
                _tag.call = this;
                Proceed();
            }

            void Proceed() override {
                switch (_status) {
                    case CallStatus::CREATE: {
                        _status = CallStatus::PROCESS;
                        _service->RequestDeleteServer(&_context, &_request, &_responder, _queue, _queue, (void*)&_tag);
                        break;
                    }
                    case CallStatus::PROCESS: {
                        new DeleteServerCallData{_service, _queue};
                        std::string key = _request.ip() + ":" + std::to_string(_request.port());
                        server_registry.erase(key);
                        _response.set_message("Server deleted successfully!");
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
                ServerInfo _request;
                ProxyResponse _response;
                ServerAsyncResponseWriter<ProxyResponse> _responder;
                Tag _tag;
        };

        void HandleRPCs() {
            new RegisterServerCallData{&_service, _queue.get()};
            new GetAvailableServersCallData{&_service, _queue.get()};
            new DeleteServerCallData{&_service, _queue.get()};
            void* tag;
            bool ok;
            while (true) {
                if (_queue->Next(&tag, &ok) && ok) {
                    auto* tag_ptr = static_cast<Tag*>(tag);
                    switch (tag_ptr->id) {
                        case MessageID::REGISTER: {
                            static_cast<RegisterServerCallData*>(tag_ptr->call)->Proceed();
                            break;
                        }
                        case MessageID::AVAILABLE: {
                            static_cast<GetAvailableServersCallData*>(tag_ptr->call)->Proceed();
                            break;
                        }
                        case MessageID::DELETE: {
                            static_cast<DeleteServerCallData*>(tag_ptr->call)->Proceed();
                            break;
                        }
                    }
                } else {
                    std::cout << "Server shutting down" << std::endl;
                    break;
                }
            }
        }

        ProxyService::AsyncService _service;
        std::unique_ptr<ServerCompletionQueue> _queue;
        std::unique_ptr<Server> _server;
};


int main() {
    ProxyServer server;
    server.Run();

    return 0;
}
