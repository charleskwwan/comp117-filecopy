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
#include "c150grading.h"
#include "c150debug.h"

#include "utils.h"
#include "filehandler.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// constants
const int TIMEOUT_DURATION = 1000; // 1 second
const int MAX_TRIES = 5;


// fwd declarations
void usage(char *progname, int exitCode);
int sendFile(C150DgmSocket *sock, string dir, string fname, int fileNastiness);


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
    string dir;
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
    dir = argv[srcDirArg];
    if (!isDir(dir.c_str())) usage(argv[0], 8);

    // debugging
    // initDebugLog("fileclientdebug.txt", argv[0]);
    initDebugLog(NULL, argv[0]);

    try {
        // create socket
        c150debug->printf(
            C150APPLICATION,
            "Creating C150NastyDgmSocket(nastiness=%d)",
            netNastiness
        );
        C150DgmSocket *sock = new C150NastyDgmSocket(netNastiness);

        sock -> setServerName(argv[serverArg]);
        sock -> turnOnTimeouts(TIMEOUT_DURATION);

        c150debug->printf(C150APPLICATION, "Ready to send messages");

        // temp
        sendFile(sock, dir, "data1", fileNastiness);

        // clean up socket
        delete sock;
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
// DEFS
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


// readExpectedPacket
//      - reads packets until an expected one arrives or timeout occurs
//      - any unexpected packets are DROPPED
// 
//  args:
//      - sock: socket
//      - pcktp: location to store read packet
//      - expect: attributes expected in packet
//
//  returns:
//      - length of packet data read if successful
//      - -1 if timed out

ssize_t readExpectedPacket(
    C150DgmSocket *sock, Packet *pcktp,
    PacketExpect expect
) {
    Packet tmp;
    ssize_t datalen;

    do {
        datalen = readPacket(sock, &tmp);
    } while (datalen >= 0 && !isExpected(tmp, expect));

    if (datalen != -1) *pcktp = tmp; // return packet to caller
    return datalen;
}


// writePacketWithRetries
//      - writes a packet and waits for a response
//      - will retry after a timeout a certain number of times
//
//  args:
//      - sock: socket
//      - opcktp: outgoing packet
//      - ipcktp: incoming packet
//      - expect: expectation for incoming packet
//      - tries: max number of tries allowed, must be >=1, defaults to 1
// 
//  returns:
//      - length of data read if successful
//      - -1 if timed out

ssize_t writePacketWithRetries(
    C150DgmSocket *sock,
    Packet *opcktp,
    Packet *ipcktp, PacketExpect expect,
    int tries
) {
    ssize_t datalen;
    do {
        writePacket(sock, opcktp);
        datalen = readExpectedPacket(sock, ipcktp, expect);
        tries--;
    } while (tries > 0 && datalen < 0);

    return datalen;
}


// sendFile
//      - sends a file via a socket
//
//  args:
//      - sock: socket
//      - dir: name of file directory
//      - fname: name of file
//      - fileNastiness: nastiness with which to send file
//
//  return:
//      - 0 if successful
//      - -1 if file could not be loaded
//
//  notes:
//      - if directory or file is invalid, nothing happens
//      - if network fails, server is assumed down and exception is thrown
//
//  NEEDSWORK: add logs for grading
//  NEEDSWORK: implement filecopy, currently only does end to end checking

int sendFile(
    C150DgmSocket *sock,
    string dir,
    string fname,
    int fileNastiness
) {
    // file vars
    string fullname = makeFileName(dir, fname);
    FileHandler fhandler(fullname, fileNastiness);

    // network vars
    Packet ipckt, opckt;
    PacketExpect expect;
    // int fileid;

    // check if file was successfully loaded
    if (fhandler.getFile() == NULL) return -1;

    // send check request
    opckt = Packet(
        NULL_FILEID, REQ_FL | CHECK_FL, NULL_SEQNO,
        fname.c_str(), fname.length()+1 // +1 for null term
    );
    expect.fileid = NULL_FILEID;
    expect.flags = REQ_FL | CHECK_FL;
    if (writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES) < 0) {
        c150debug->printf(
            C150APPLICATION,
            "sendFile: Check request for fileid=%d timed out"
        );
    }

    printPacket(ipckt, stdout);
    printHash((const unsigned char *)ipckt.data, stdout);

    return 0;
}
