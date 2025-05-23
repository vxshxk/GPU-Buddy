#ifndef CODE_EXTRACTOR_H
#define CODE_EXTRACTOR_H
//Code Extractor
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace CodeExtractor {
    bool extractPythonCode(std::string& FilePath, std::vector<std::string>& code, std::vector<std::string>& commands) {
        std::ifstream inputFile(FilePath);
        if (!inputFile.is_open()) {
            std::cerr << "Error: Unable to open file " << FilePath << std::endl;
            return false;
        }
        std::string line;
        size_t i;
        while (std::getline(inputFile, line)) {
            if (line[0] == ' '){
                for (i=0; i < line.length(); ++i){
                    if (line[i] != ' '){
                        break;
                    }
                }
                if (line[i] == '!'){
                    commands.push_back(line.substr(i+1));
                    continue;
                } 
            }
            if (!line.empty() && line[0] != '!') {
                code.push_back(line);  
            }
            else if (!line.empty() && line[0] == '!') {
                commands.push_back(line.substr(1));  
            }
        }
        inputFile.close();
        return true;
    }

    bool extractPythonScriptCode(std::string& FilePath, std::string& ScriptPath, std::vector<std::string>& code, std::vector<std::string>& commands) {
        std::ifstream inputFile(FilePath);
        if (!inputFile.is_open()) {
            std::cerr << "Error: Unable to open file " << FilePath << std::endl;
            return false;
        }
        std::string line;
        size_t i;
        while (std::getline(inputFile, line)) {
            if (line[0] == ' ') {
                for (i=0; i < line.length(); ++i){
                    if (line[i] != ' '){
                        break;
                    }
                }
                if (line[i] == '!') continue;
            }
            if (!line.empty() && line[0] != '!') {
                code.push_back(line);  
            }
            else if (!line.empty() && line[0] == '!') {
                commands.push_back(line.substr(1));  
            }
        }
        inputFile.close();

        std::ifstream inputScriptFile(ScriptPath);
        if (!inputScriptFile.is_open()) {
            std::cerr << "Error: Unable to open file " << ScriptPath << std::endl;
            return false;
        }
        std::string command;
        i;
        while (std::getline(inputScriptFile, command)) {
            if (command[0] == ' '){
                for (i=0;i<command.length();i++){
                    if (command[i] != ' '){
                        break;
                    }
                }
            }
            if (!command.empty()) {
                commands.push_back(command);  
            }
        }
        inputScriptFile.close();

        return true;
    }
    std::string extractPackageName(const std::string &command) {
        std::istringstream iss(command);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        if (tokens.size() >= 3 && tokens[0] == "pip" && tokens[1] == "install") {
            return "pip install " +  tokens[2];
        }
        return "";
    }
    
}

#endif