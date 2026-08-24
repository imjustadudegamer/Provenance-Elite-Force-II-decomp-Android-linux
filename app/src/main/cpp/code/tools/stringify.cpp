/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include <cstdio>
#include <ctype.h>
#include <cstring>
#include <string>

#include <iostream>
#include <fstream>


int main(int argc, const char** argv)
{
    std::ifstream ifp;
    std::ofstream ofp;
    std::string buffer;

    if (argc < 3)
        return 1;

    std::string inFile = argv[1];
    std::string outFile = argv[2];

    ifp.open(inFile, std::ios_base::in);
    if (!ifp.is_open())
        return 2;

    ofp.open(outFile, std::ios_base::out);
    if (!ofp.is_open())
        return 3;

    const size_t lastSlash = inFile.find_last_of("/\\") + 1;
    const size_t lastDot = inFile.find_last_of(".");
    std::string base = inFile.substr(lastSlash, lastDot - lastSlash);
    ofp << "const char *fallbackShader_" << base << " = " << std::endl;

    while (std::getline(ifp, buffer))
    {
        const char* end = buffer.c_str() + buffer.length();
        while (end >= buffer.c_str() && isspace(*end)) {
            end--;
        }

        std::string value = buffer.substr(0, buffer.c_str() - end);

        ofp << "\"" << value << "\\n" << "\"" << std::endl;
    }

    ofp << ";" << std::endl;

    ifp.close();
    ofp.close();

    return 0;
}
