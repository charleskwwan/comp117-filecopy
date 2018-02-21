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
#include <cstdio>
#include <string>
#include <map> // O(logn), but ideally unordered_map for O(1) if c++11 allowed

#include "c150nastydgmsocket.h"
#include "c150grading.h"
#include "c150debug.h"

#include "utils.h"
#include "filehandler.h"

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
//
//  limitations:
//      - only serves one file transfer at a time currently
//
//  notes:
//      - initially considered switch statement, but need to check current state
//        against packet flags, so if-else required
//      - function is LONG, but difficult to shorten
//
//  NEEDSWORK: need to implement filecopy parts, for now short circuit to check.
//             only initial file request should have filename. check request in
//             future should not, and use fileid instead.

void run(C150DgmSocket *sock, const char *targetDir, int fileNastiness) {
    State state = IDLE_ST; // waiting

    // file vars
    FileHandler fhandler(fileNastiness);
    string dirname(targetDir);
    string fullFname;
    string tmpFname;
    // size_t filelen; 

    // network vars
    Packet pckt;
    map<int, Packet> cache; // keys = seqno, NULL_SEQNO for last nonerror pckt
    int fileid = NULL_FILEID; // for new id, increment

    // main loop
    while (1) {
        if (readPacket(sock, &pckt) < 0) {
            // time out should be impossible, but incl anyway
            c150debug->printf(C150APPLICATION, "run: Server read timed out\n");
            continue; 

        } else if (state == IDLE_ST && pckt.flags == (REQ_FL | CHECK_FL)) {
            // server idle, respond yes to request
            state = CHECK_ST;

            fullFname = makeFileName(dirname, string(pckt.data));
            tmpFname = fullFname + ".TMP";
            fhandler.setName(tmpFname); // file assumed to exist, will
                                        // automatically read

            pckt.flags = pckt.flags | POS_FL;
            pckt.fileid = ++fileid; // new fileid
            pckt.seqno = NULL_SEQNO;
            pckt.datalen = HASH_LEN;
            strncpy(pckt.data, (const char *)fhandler.getHash(), HASH_LEN);

        } else if (state != IDLE_ST && pckt.fileid != fileid) {
            // server in transfer, but wrong fileid 
            pckt.flags = NEG_FL;
            pckt.datalen = 0;

        } else if (state == CHECK_ST &&
                   (pckt.flags & (CHECK_FL | POS_FL | NEG_FL))) {
            // server ready for check results, pos/neg set
            state = FIN_ST;

            pckt.flags = CHECK_FL | FIN_FL;
            pckt.seqno = NULL_SEQNO;
            pckt.datalen = 0;

            if ((pckt.flags & POS_FL) &&
                rename(tmpFname.c_str(), fullFname.c_str()) != 0) {
                c150debug->printf(
                    C150APPLICATION,
                    "run: '%s' could not be renamed to '%s'\n",
                    tmpFname.c_str(), fullFname.c_str()
                );
                pckt.flags |= NEG_FL;

            } else if ((pckt.flags & NEG_FL) && remove(tmpFname.c_str()) != 0) {
                c150debug->printf(
                    C150APPLICATION,
                    "run: '%s' could not be removed\n",
                    tmpFname.c_str()
                );
                pckt.flags |= NEG_FL;

            } else {
                pckt.flags |= POS_FL; // rename or remove successful
            }

        } else if (state == FIN_ST && pckt.flags == FIN_FL) {
            // client received final message, can complete request now
            state = IDLE_ST;

            fullFname.clear();
            tmpFname.clear();

            continue; // no response needed

        } else {
            // default, return error to client
            pckt.flags = NEG_FL;
            pckt.datalen = 0;
        }

        if (pckt.flags != NEG_FL) // cache last packet if nonerror
            cache.insert(pair<int, Packet>(pckt.seqno, pckt));
        writePacket(sock, &pckt);
    }
}