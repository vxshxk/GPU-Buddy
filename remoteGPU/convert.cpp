#include "CodeExtractor.h"
#include "CodeRestorer.h"
using namespace std;
int main() {
    string InputFilePath = "input.py"; 
    string OutputFilePath = "output.py";
    string OutputScriptPath = "commands.sh";
    vector<string> code;
    vector<string> commands;
    CodeExtractor::extractPythonCode(InputFilePath, code, commands);
    CodeRestorer::writePythonCode(OutputFilePath, OutputScriptPath, code, commands);
    return 0;
}
