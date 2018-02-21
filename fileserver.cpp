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

#include "c150nastydgmsocket.h"
#include "c150nastyfile.h"
#include "c150grading.h"
#include "c150debug.h"

#include "utils.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// constants


// fwd declarations
void usage(char *progname, int exitCode);
void run(C150DgmSocket *sock, const char *targetDir, int fileNastiness);


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

    // check target directory
    if (!isDir(argv[targetDirArg])) {
        usage(argv[0], 8);
    }

    // debugging
    initDebugLog("fileserverdebug.txt", argv[0]);
    c150debug->setIndent("    "); // if merge client/server logs, server stuff
                                  // will be indented

    // create socket
    try {
        c150debug->printf(
            C150APPLICATION,
            "Creating C150NastyDgmSocket(nastiness=%d)",
            netNastiness
        );
        C150DgmSocket *sock = new C150NastyDgmSocket(netNastiness);

        sock -> turnOffTimeouts(); // server always waits, never initiates

        c150debug->printf(C150APPLICATION, "Ready to accept messages");

        // temp
        Packet pckt;
        size_t readlen = readPacket(sock, &pckt); // pckt always null terminated by read
        cout << "readlen: " << readlen << endl
             << "fileid: " << pckt.fileid << endl
             << "seqno: " << pckt.seqno << endl
             << "data: " << pckt.data << endl;

    } catch (C150NetworkException e) {
        // write to debug log
        c150debug->printf(
            C150ALWAYSLOG,
            "Caught C150NetworkException: %s\n",
            e.formattedExplanation().c_str()
        );
        // in case logging to file, write to console too
        cerr << argv[0] << ": C150NetworkException: "
             << e.formattedExplanation() << endl;
    }

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


// run
//      - runs the main server loop
//      - loop continuously receives packets and responds to clients based on
//        the current state (checking file? transferring file? etc.)
//
//  args:
//      - sock: socket
//      - targetDir: name of target directory
//      - fileNastiness: nastiness with which to handle files
//
//  returns: n/a

void run(C150DgmSocket *sock, const char *targetDir, int fileNastiness) {

}