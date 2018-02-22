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
#include <cstring>

#include "c150nastydgmsocket.h"
#include "c150grading.h"
#include "c150debug.h"

#include "utils.h"
#include "filehandler.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// CopyResult enum
//      - for result of a file send

enum CopyResult {
    SUCCESS,
    TIMEOUT,
    FILE_CHECK_DENIED, // server rejected file check request
    FILE_READ_ERROR, // file could not be loaded
    SERVER_CLEANUP_ERROR // server could rename/remove
};


// constants
const int TIMEOUT_DURATION = 1000; // 1 second
const int MAX_TRIES = 5;


// fwd declarations
void usage(char *progname, int exitCode);
CopyResult sendFile(
    C150DgmSocket *sock, 
    string dir, string fname, int fileNastiness
);


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
    // uint32_t debugClasses = C150APPLICATION | C150NETWORKTRAFFIC |
    //                         C150NETWORKDELIVERY | C150FILEDEBUG
    uint32_t debugClasses = C150APPLICATION;
    // initDebugLog("fileclientdebug.txt", argv[0], debugClasses);
    initDebugLog(NULL, argv[0], debugClasses);

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

// ==========
// GENERAL
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


// ==========
// CHECKING
// ==========

// checkFile
//      - checks a hash against a file, read from disk
//
//  args:
//      - fname: full file name to check
//      - hash: hash to check against
//      - nastiness: with which to read file
//
//  return:
//      - true, if passed
//      - false, if failed
//
//  notes:
//      - fname MUST be a file that exists. if not, checkFile will silently
//        return false.
//      - hash must not be null, and be a sha1 hash of length 20
//
//  NEEDSWORK: make checkFile better for higher nastiness levels

bool checkFile(string fname, const char *hash, int nastiness) {
    FileHandler fhandler(fname, nastiness);
    return fhandler.getFile() != NULL &&
           strncmp((const char *)fhandler.getHash(), hash, HASH_LEN) == 0;
}


// sendCheckResult
//      - sends the result of the end2end to the server
//
//  args:
//      - sock: socket
//      - ipcktp: location to put incoming packet from server
//      - fileid: id for file negotiated with server
//      - result: whether the check passed or not
//
//  result:
//      - length of data read if successful
//      - -1 if timed out

ssize_t sendCheckResult(
    C150DgmSocket *sock, Packet *ipcktp,
    int fileid, bool result
) {
    FLAG resFlag = result ? POS_FL : NEG_FL;
    Packet opckt(fileid, CHECK_FL | resFlag, NULL_SEQNO, NULL, 0);
    PacketExpect expect = { fileid, CHECK_FL | FIN_FL };
    return writePacketWithRetries(sock, &opckt, ipcktp, expect, MAX_TRIES);
}


// ==========
// SEND
// ==========

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
//      - copy result, see CopyResult enum
//
//  notes:
//      - if directory or file is invalid, nothing happens
//      - if network fails, server is assumed down and exception is thrown
//
//  NEEDSWORK: add logs for grading
//  NEEDSWORK: implement filecopy, currently only does end to end checking
//  NEEDSWORK: once filecopy implemented, move check req to own function

CopyResult sendFile(
    C150DgmSocket *sock, 
    string dir, string fname, int fileNastiness
) {
    CopyResult retval = SUCCESS;

    // file vars
    string fullname = makeFileName(dir, fname);
    FileHandler fhandler(fullname, fileNastiness);

    // network vars
    Packet ipckt, opckt;
    PacketExpect expect;
    int fileid;

    // check if file was successfully loaded
    if (fhandler.getFile() == NULL) return FILE_READ_ERROR;

    // send check request
    opckt = Packet(
        NULL_FILEID, REQ_FL | CHECK_FL, NULL_SEQNO,
        fname.c_str(), fname.length()+1 // +1 for null term
    );
    expect.fileid = NULL_FILEID;
    expect.flags = REQ_FL | CHECK_FL;
    if (writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES) < 0) {
        return TIMEOUT;
    } else if (ipckt.flags & NEG_FL) {
        c150debug->printf(
            C150APPLICATION,
            "sendFile: Check request for fname=%s denied",
            fullname.c_str()
        );
        return FILE_CHECK_DENIED;
    } else if (ipckt.flags & POS_FL) {
        fileid = ipckt.fileid;
    }

    // send check result
    bool checkResult = checkFile(fullname, ipckt.data, fileNastiness);
    if (sendCheckResult(sock, &ipckt, fileid, checkResult) < 0) {
        return TIMEOUT;
    } else if (ipckt.flags & NEG_FL) { // server failed to rename/remove
        retval = SERVER_CLEANUP_ERROR;
    }

    // send final fin
    // this is to tell the server it can cleanup. however, if this packet is
    // lossed, server will eventually timeout and cleanup anyway, so no need 
    // to resend.
    opckt = Packet(fileid, FIN_FL, NULL_SEQNO, NULL, 0);
    writePacket(sock, &opckt);

    return retval;
}
