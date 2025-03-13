#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
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

const std::string PREFIX_PATH = "../";

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

int main(int argc, char** argv) {
    RemoteGPUClient client(grpc::CreateChannel("localhost:50052", grpc::InsecureChannelCredentials()));

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