// fileclient.cpp
//
// Reads files from a directory and sends to a fileserver via UDP
//
// Cmd line: fileclient <server> <networknastiness> <filenastiness> <srcdir>
//  - server <string>: server address
//  - networknastiness <int>: range 0-4
//  - filenastiness <int>: range 0-5
//  - srcdir <string>: source directory
// 
// Limitations:
//  - Subdirectories are ignored
//
// By: Justin Jo and Charles Wan


#include <iostream>

#include "c150nastydgmsocket.h"
#include "c150nastyfile.h"
#include "c150grading.h"
#include "c150debug.h"

#include "fileutils.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// constants
const int TIMEOUT_DURATION = 1000; // 1 second
const int MAX_TRIES = 5;


// fwd declarations
void usage(char *progname, int exitCode);
void sendFile(C150DgmSocket *sock, string dir, string fname, int fileNastiness);


// cmd line args
const int numberOfArgs = 4;
const int serverArg = 1;
const int netNastyArg = 2;
const int fileNastyArg = 3;
const int srcDirArg = 4;


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

    // check target directory
    if (!isDir(argv[srcDirArg])) {
        usage(argv[0], 8);
    }

    // debugging
    initDebugLog("fileclientdebug.txt", argv[0]);

    // create socket
    // try {
    //     c150debug->printf(
    //         C150APPLICATION,
    //         "Creating C150NastyDgmSocket(nastiness=%d)",
    //         netNastiness
    //     );
    //     C150DgmSocket *sock = new C150NastyDgmSocket(netNastiness);

    //     sock -> setServerName(argv[serverArg]);
    //     sock -> turnOnTimeouts(TIMEOUT_DURATION);

    //     c150debug->printf(C150APPLICATION, "Ready to send messages");
    // } catch (C150NetworkException e) {
    //     // write to debug log
    //     c150debug->printf(
    //         C150ALWAYSLOG,
    //         "Caught C150NetworkException: %s\n",
    //         e.formattedExplanation().c_str()
    //     );
    //     // in case logging to file, write to console too
    //     cerr << argv[0] << ": C150NetworkException: "
    //          << e.formattedExplanation() << endl;
    // }

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
        "usage: %s <server> <networknastiness> <filenastiness> <srcdir>\n",
        progname
    );
    exit(exitCode);
}



