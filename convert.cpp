#include "CodeExtractor.h"
#include "CodeRestorer.h"
using namespace std;
int main() {
    string filePath="input.py"; 
    string filePath2="output.py";
    vector<string>code;
    CodeExtractor::extractPythonCode(filePath,code);
    for (auto& it:code) {
        cout << it << endl;
    }
    CodeRestorer::writePythonCode(filePath2,code);
    return 0;
}
