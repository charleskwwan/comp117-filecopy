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
const int GIVEUP_TIMEOUT = 10000; // 10s, time until server gives up
const Packet ERROR_PCKT(NULL_FILEID, NEG_FL, NULL_SEQNO, NULL, 0);


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
    // uint32_t debugClasses = C150APPLICATION | C150NETWORKTRAFFIC |
    //                         C150NETWORKDELIVERY | C150FILEDEBUG
    uint32_t debugClasses = C150APPLICATION;
    // initDebugLog("fileclientdebug.txt", argv[0], debugClasses);
    initDebugLog(NULL, argv[0], debugClasses);
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
        sock -> turnOnTimeouts(GIVEUP_TIMEOUT); // on timeout, give up
        c150debug->printf(C150APPLICATION, "Ready to accept messages");

        run(sock, argv[targetDirArg], fileNastiness);

    } catch (C150NetworkException e) {
        // write to debug log
        c150debug->printf(
            C150ALWAYSLOG,
            "Caught C150NetworkException: %s",
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


// checkResults
//      - checks the results of an e2e check by client
//
//  args:
//      - ipckt: received packet
//      - fname: intended file name
//      - tmpname: temporary file name
//
//  returns:
//      - packet to be sent back to client
//
//  notes:
//      - ipckt's flags are assumed to be: CHECK_FL | (POS_FL xor NEG_FL)

Packet checkResults(
    const Packet &ipckt,
    const char *fname,
    const char *tmpname
) {
    Packet opckt(NULL_FILEID, CHECK_FL | FIN_FL, NULL_SEQNO, NULL, 0);

    if ((ipckt.flags & POS_FL) && rename(tmpname, fname) != 0) {
        c150debug->printf(
            C150APPLICATION,
            "run: '%s' could not be renamed to '%s'",
            tmpname, fname
        );
        opckt.flags |= NEG_FL;

    } else if ((ipckt.flags & NEG_FL) && remove(tmpname) != 0) {
        c150debug->printf(
            C150APPLICATION,
            "run: '%s' could not be removed",
            tmpname
        );
        opckt.flags |= NEG_FL;

    } else {
        opckt.flags |= POS_FL; // rename or remove successful
    }
    
    return opckt;
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
    string fname, tmpname;
    // size_t filelen; 

    // network vars
    Packet ipckt, opckt; // incoming, outgoing
    ssize_t datalen;
    map<Packet, Packet> cache; // map packet received to response sent
    int fileid = NULL_FILEID; // for new id, increment

    // main loop
    while (1) {
        datalen = readPacket(sock, &ipckt);

        if (datalen < 0 || (state == FIN_ST && ipckt.flags == FIN_FL)) {
            // server timed out, in which case client gave up so reset OR
            // client received final message, can complete request now
            if (state != FIN_ST && state != IDLE_ST) { // timeout mid-transfer
                c150debug->printf(
                    C150APPLICATION,
                    "run: Server timed out mid-transfer, client gave up"
                );
            } else if (state == FIN_ST && ipckt.flags == FIN_FL) {
                c150debug->printf(
                    C150APPLICATION,
                    "run: Final FIN received, cleaning up"
                );
            }

            state = IDLE_ST;
            fname.clear();
            tmpname.clear();
            cache.clear(); // only cache for current transfer, now done

            continue; // no response needed

        } else if (cache.count(ipckt)) {
            // previously sent packet received again, and still in current
            // session. assume due to retry and send reply the same way
            opckt = cache[ipckt];

        } else if (state == IDLE_ST && ipckt.flags == (REQ_FL | CHECK_FL)) {
            // server idle, respond yes to request
            c150debug->printf(
                C150APPLICATION,
                "run: Check request received for fileid=%d, fname=%s",
                fileid, ipckt.data
            );

            state = CHECK_ST;
            fname = makeFileName(dirname, ipckt.data);
            tmpname = fname + ".TMP";
            // file assumed to exist
            fhandler = FileHandler(tmpname, fileNastiness);

            opckt = Packet(
                ++fileid, ipckt.flags | POS_FL, NULL_SEQNO,
                (const char *)fhandler.getHash(), HASH_LEN
            );

        } else if (state != IDLE_ST && ipckt.fileid != fileid) {
            // server in transfer, but wrong fileid 
            opckt = ERROR_PCKT;
            opckt.fileid = ipckt.fileid; // tell client wrong id

        } else if (state == CHECK_ST &&
                   (ipckt.flags == (CHECK_FL | POS_FL) ||
                    ipckt.flags == (CHECK_FL | NEG_FL))) {
            // server ready for check results, pos/neg set
            c150debug->printf(
                C150APPLICATION,
                "run: Check results for fileid=%d received, will rename/"
                "remove",
                fileid
            );

            state = FIN_ST;
            opckt = checkResults(ipckt, fname.c_str(), tmpname.c_str());

        } else {
            // default, return error to client
            opckt = ERROR_PCKT;
        }

        if (opckt.flags != NEG_FL) // cache packets if nonerror
            cache.insert(pair<Packet, Packet>(ipckt, opckt));
        writePacket(sock, &opckt);
    }
}