#ifndef CODE_EXTRACTOR_H
#define CODE_EXTRACTOR_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace CodeExtractor {
    bool extractPythonCode(std::string& FilePath, std::vector<std::string>& code) {
        std::ifstream inputFile(FilePath);
        if (!inputFile.is_open()) {
            std::cerr << "Error: Unable to open file " << FilePath << std::endl;
            return false;
        }
        std::string line;
        while (std::getline(inputFile, line)) {
            if (!line.empty() && line[0] != '!') {
                code.push_back(line);  
            }
        }
        inputFile.close();
        return true;
    }
}

#endif
