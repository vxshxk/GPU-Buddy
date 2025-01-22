#ifndef CODE_RESTORER_H
#define CODE_RESTORER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace CodeRestorer {
    bool writePythonCode(std::string& FilePath,std::vector<std::string>& code) {
        std::ofstream outputFile(FilePath, std::ios::out);
        if (!outputFile.is_open()) {
            std::cerr << "Error: Unable to create file " << FilePath << std::endl;
            return false;
        }
        for (auto& line : code) {
            outputFile << line << '\n';
        }
        outputFile.close();
        return true;
    }
}

#endif
