#include <iostream>
#include <grpcpp/grpcpp.h>
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

const std::string PREFIX_PATH = "../../";

class RemoteGPUServiceImpl final : public RemoteGPU::Service {
    public:
        RemoteGPUServiceImpl () {
            this->id = 0;
        }

        Status UploadFile (ServerContext* context, const File* request, FileID* reply) override {
            std::vector<std::string> code;
            std::vector<std::string> commands;

            int cur_id = id.fetch_add(1, std::memory_order_relaxed); 
            std::string OutputFilePath = PREFIX_PATH + "server_code" + std::to_string(cur_id) + ".py";
            std::string OutputScriptPath = PREFIX_PATH + "server_script" + std::to_string(cur_id) + ".sh";

            for (const auto& line : request->code()) {
                code.push_back(line);
            }
            for (const auto& command : request->commands()) {
                std::string NewCommand = CodeExtractor::extractPackageName(command);
                commands.push_back(NewCommand);
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

        Status Execute (ServerContext* context, const FileID* request, Output* reply) override {
            int cur_id = request->id();
            auto it = index.find(cur_id);
            if (it != index.end()) {
                std::string FilePath = it->second.first;
                std::string ScriptPath = it->second.second;
                std::string OutputPath = PREFIX_PATH + "output" + std::to_string(cur_id) + ".txt";

                std::string SetEnvironment = "python -m venv env && . env/bin/activate";
                std::string RunScript = "chmod +x " + ScriptPath + " && ./" + ScriptPath ;
                std::string RunCode = "python " + FilePath + " > " + OutputPath ;
                std::string CloseEnvironment = "deactivate ";
                
                std::string TerminalExecute = SetEnvironment + " && " + RunScript + " && " + RunCode + " && " + CloseEnvironment;
                
                system(TerminalExecute.c_str());
                
                std::fstream file(OutputPath);
                if (!file.is_open()) {
                    return Status(grpc::UNAVAILABLE, "No output");
                }

                std::vector<std::string> output;
                std::string line;

                while (std::getline(file, line)) {
                    output.push_back(line); 
                }

                file.close();

                for (const auto& line : output) {
                    reply->add_out(line);
                }

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