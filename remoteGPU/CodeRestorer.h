#ifndef CODE_RESTORER_H
#define CODE_RESTORER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace CodeRestorer {
    bool writePythonCode(std::string& FilePath, std::string& ScriptPath, std::vector<std::string>& code, std::vector<std::string>& commands) {
        std::ofstream outputFile(FilePath, std::ios::out);
        std::ofstream scriptFile(ScriptPath, std::ios::out);
        if (!outputFile.is_open()) {
            std::cerr << "Error: Unable to create file " << FilePath << std::endl;
            return false;
        }
        for (auto& line : code) {
            outputFile << line << '\n';
        }
        outputFile.close();

        if (!scriptFile.is_open()) {
            std::cerr << "Error: Unable to create file " << ScriptPath << std::endl;
            return false;
        }
        for (auto& command : commands) {
            scriptFile << command << '\n';
        }
        scriptFile.close();

        return true;
    }
}

#endif
