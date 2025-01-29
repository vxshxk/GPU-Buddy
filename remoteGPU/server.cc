#include <bits/stdc++.h>
#include <grpcpp/grpcpp.h>

#include "remote_gpu.grpc.pb.h"
#include "CodeExtractor.h"
#include "CodeRestorer.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using remoteGPU::RemoteGPU;
using remoteGPU::File;
using remoteGPU::FileID;
using remoteGPU::Output;

class RemoteGPUServiceImpl final : public RemoteGPU::Service {
    public:
        RemoteGPUServiceImpl () {
            this->id = 0;
        }

        Status UploadFile (ServerContext* context, const File* request, FileID* reply) override {
            std::vector<std::string> code;
            std::vector<std::string> commands;

            int cur_id = id.fetch_add(1, std::memory_order_relaxed); 
            std::string OutputFilePath = "output" + std::to_string(cur_id) + ".py";
            std::string OutputScriptPath = "commands" + std::to_string(cur_id) + ".sh";

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
        
        Status DownloadFile (ServerContext* context, const FileID* request, File* reply) override {
            int cur_id = request->id();
            auto it = index.find(cur_id);
            if (it != index.end()) {
                std::vector<std::string> code;
                std::vector<std::string> commands;

                std::string OutputFilePath = it->second.first;
                std::string OutputScriptPath = it->second.second;

                CodeExtractor::extractPythonScriptCode(OutputFilePath, OutputScriptPath, code, commands);

                for (const auto& line: code) {
                    reply->add_code(line);
                }
                for (const auto& command: commands) {
                    reply->add_commands(command);
                }

                return Status::OK;
            }
            else {
                return Status(grpc::NOT_FOUND, "File not found.");
            }
        }

        // Yet to implement Execute rpc

    private:
        std::atomic<int> id;
        std::unordered_map<int, std::pair<std::string, std::string>> index;
};

void RunServer() {
    std::string server_address("0.0.0.0:50052");
    RemoteGPUServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on port: " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();

    return 0;
}