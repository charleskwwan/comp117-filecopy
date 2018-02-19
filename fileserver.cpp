// fileserver.cpp
// 
// Receives and writes files by UDP from fileclients
//
// Cmd line: fileserver <networknastiness> <filenastiness> <targetdir>
//  - networknastiness <int>: range 0-4
//  - filenastiness <int>: range 0-5
//  - targetdir <string>: target directory, should be empty on start
//
//  By: Justin Jo and Charles Wan


#include <iostream>

#include "c150dgmsocket.h"
#include "c150nastyfile.h"
#include "c150grading.h"
#include "c150debug.h"

#include "fileutils.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// constants


// fwd declarations
void usage(char *progname, int exitCode);


// cmd line args
const int numberOfArgs = 3;
const int netNastyArg = 1;
const int fileNastyArg = 2;
const int targetDirArg = 3;


// ==========
// 
// MAIN
//
// ==========

int main(int argc, char *argv[]) {

    int netNastiness;
    int fileNastiness;

    GRADEME(argc, argv); // obligatory grading line

    // cmd line arg handling
    if (argc != 1 + numberOfArgs) {
        usage(argv[0], 1);
    }

    if (safeAtoi(argv[netNastyArg], &netNastiness) != 0) {
        fprintf(stderr, "error: <networknastiness> must be an integer\n");
        usage(argv[0], 4);
    }

    if (safeAtoi(argv[fileNastyArg], &fileNastiness) != 0) {
        fprintf(stderr, "error: <filenastiness> must be an integer\n");
        usage(argv[0], 4);
    }

    // debugging
    initDebugLog("fileserverdebug.txt", argv[0]);

    return 0;
}

// ==========
// 
// FUNCTION DEFS
//
// ==========

// Prints command line usage to stderr and exits
void usage(char *progname, int exitCode) {
    fprintf(
        stderr,
        "usage: %s <networknastiness> <filenastiness> <targetdir>\n",
        progname
    );
    exit(exitCode);
}
