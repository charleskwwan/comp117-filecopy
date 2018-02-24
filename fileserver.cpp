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
#include "c150nastyfile.h"
#include "c150grading.h"
#include "c150debug.h"

#include "utils.h"
#include "hash.h"
#include "filehandler.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// constants
const int GIVEUP_TIMEOUT = 10000; // 10s, time until server gives up
const Packet ERROR_PCKT(NULL_FILEID, NEG_FL, NULL_SEQNO, NULL, 0);
const char *TMP_SUFFIX = ".TMP";


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
            "Caught %s",
            e.formattedExplanation().c_str()
        );
        // in case logging to file, write to console too
        cerr << argv[0] << " " << e.formattedExplanation() << endl;
    } catch (C150FileException e) {
        c150debug->printf(
            C150ALWAYSLOG,
            "Caught %s",
            e.formattedExplanation().c_str()
        );
        // in case logging to file, write to console too
        cerr << argv[0] << " " << e.formattedExplanation() << endl;
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


// ==========
// CHECKING
// ==========

// fillCheckRequest
//      - computes and packages a hash to fill a check request
//
//  args:
//      - fileid: associated with file to check
//      - fname: full file name to check
//      - nastiness: with which to read file
//
//  return:
//      - packet to be sent back to client
//      - if file does not exist, error packet is returned
//
//  notes:
//      - file is assumed to exist

Packet fillCheckRequest(int fileid, string fname, int nastiness) {
    FileHandler fhandler(fname, nastiness);
    Hash fhash;

    if (fhandler.getFile() == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "fillCheckRequest: File fname=%s could not be opened",
            fname.c_str()
        );
        return Packet(fileid, REQ_FL | CHECK_FL | NEG_FL, NULL_SEQNO, NULL, 0);

    } else {
        fhash.set(fhandler.getFile(), fhandler.getLength());

        c150debug->printf(
            C150APPLICATION,
            "fillCheckRequest: Hash=[%s] computed for fname=%s",
            fhash.str().c_str(), fname.c_str()
        );
        *GRADING << "File: " << fname << " computed checksum ["
                 << fhash.str() << "]" << endl;
        return Packet(
            fileid, REQ_FL | CHECK_FL | POS_FL, NULL_SEQNO,
            (const char *)fhash.get(), HASH_LEN
        );
    }
}


// checkResults
//      - checks the results of an e2e check by client
//
//  args:
//      - ipckt: received packet
//      - fileid: assoiated with fname
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
    int fileid,
    const char *fname,
    const char *tmpname
) {
    Packet opckt(fileid, CHECK_FL | FIN_FL, NULL_SEQNO, NULL, 0);

    if ((ipckt.flags & POS_FL) && rename(tmpname, fname) != 0) {
        c150debug->printf(
            C150APPLICATION,
            "checkResults: '%s' could not be renamed to '%s'",
            tmpname, fname
        );
        opckt.flags |= NEG_FL;

    } else if ((ipckt.flags & NEG_FL) && remove(tmpname) != 0) {
        c150debug->printf(
            C150APPLICATION,
            "checkResults: '%s' could not be removed",
            tmpname
        );
        opckt.flags |= NEG_FL;

    } else {
        opckt.flags |= POS_FL; // rename or remove successful
    }
    
    return opckt;
}


// ==========
// RUN
// ==========

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
    string dirname(targetDir);
    string fname, fullname, tmpname;
    // size_t filelen; 

    // network vars
    Packet ipckt, opckt; // incoming, outgoing
    ssize_t datalen;
    map<Packet, Packet> cache; // map packet received to response sent
    int fileid = NULL_FILEID; // for new id, increment

    // main loop
    while (1) {
        opckt = ERROR_PCKT; // assume error packet until otherwise changed

        // do non-state checking
        if (readPacket(sock, &ipckt) < 0) {
            // server timed out
            if (state != IDLE_ST) // mid-file transfer
                c150debug->printf(
                    C150APPLICATION, 
                    "run: Server timed out mid-transfer, client gave up"
                );

            state = IDLE_ST;
            cache.clear(); // only cache for curren transfer, now done
            continue; // no response needed

        } else if (cache.count(ipckt)) {
            // previously seen packet found, assume client retry
            c150debug->printf(
                C150APPLICATION,
                "run: Retry packet with fileid=%d, flags=%x, seqno=%d, and "
                "datalen=%d received. Resending previous response",
                ipckt.fileid, ipckt.flags & 0xff, ipckt.seqno, ipckt.datalen
            );

            opckt = cache[ipckt];
        }

        // respond by state
        //      - each state has an expectation of packets it receives
        //      - if expected packet received, opckt is changed to whats needed
        switch(state) {
            case IDLE_ST:
                if (ipckt.flags == (REQ_FL | FILE_FL)) {

                }

                // if (ipckt.flags == (REQ_FL | CHECK_FL)) {
                //     // server idle, respond yes to request
                //     fname = ipckt.data; // temp
                //     fullname = makeFileName(dirname, fname); // temp
                //     tmpname = fullname + TMP_SUFFIX; // temp
                //     fileid++; // temp

                //     c150debug->printf(
                //         C150APPLICATION,
                //         "run: Check request received for fileid=%d",
                //         fileid, fname.c_str()
                //     );
                //     *GRADING << "File: " << fname << " beginning end-to-end "
                //              << "check" << endl;

                //     opckt = fillCheckRequest(fileid, tmpname, fileNastiness);
                //     state = opckt.flags & NEG_FL ? IDLE_ST : CHECK_ST;
                // }
                break;

            case FILE_ST:
                break;

            case CHECK_ST:
                if (ipckt.flags == (CHECK_FL | POS_FL) ||
                    ipckt.flags == (CHECK_FL | NEG_FL)) {
                    // server ready for check results, pos/neg set
                    c150debug->printf(
                        C150APPLICATION,
                        "run: Check results for fileid=%d received, will %s",
                        fileid, ipckt.flags & POS_FL ? "rename" : "remove"
                    );
                    *GRADING << "File: " << fname << " end-to-end check "
                             << (ipckt.flags & POS_FL ? "succeeded" : "failed")
                             << endl;

                    state = FIN_ST;
                    opckt = checkResults( // rename/remove based on results
                        ipckt, fileid,
                        fullname.c_str(), tmpname.c_str()
                    );
                }
                break;

            case FIN_ST:
                if (ipckt.flags == FIN_FL) {
                    // final fin received
                    c150debug->printf(
                        C150APPLICATION,
                        "run: Final FIN received, cleaning up"
                    );

                    state = IDLE_ST;
                    cache.clear(); // only cache for curren transfer, now done
                    continue; // no response needed
                }
                break;

            default:
                // at the very least, tell client wrong id
                if (ipckt.fileid != fileid) opckt.fileid = ipckt.fileid;
        }

        // send opckt
        //      - by this pt, no continues so opckt should be sent
        if (opckt.flags != NEG_FL) // cache packets if nonerror
            cache.insert(pair<Packet, Packet>(ipckt, opckt));
        c150debug->printf(
            C150APPLICATION,
            "run: Sending response with fileid=%d, flags=%x, seqno=%d, "
            "datalen=%d",
            opckt.fileid, opckt.flags & 0xff, opckt.seqno, opckt.datalen
        );
        writePacket(sock, &opckt);
    }
}
