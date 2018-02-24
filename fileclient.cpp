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
#include <dirent.h>

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
const int TIMEOUT_DURATION = 1000; // 1 second
const int MAX_TRIES = 5;


// fwd declarations
void usage(char *progname, int exitCode);
int sendFile(C150DgmSocket *sock, string dir, string fname, int fileNastiness);
void sendDir(C150DgmSocket *sock, string dir, int fileNastiness);


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
        // sendDir(sock, dir, fileNastiness);

        // clean up socket
        delete sock;
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
//      - testhash: hash to check against
//      - nastiness: with which to read file
//
//  return:
//      - true, if passed
//      - false, if failed
//
//  notes:
//      - fname MUST be a file that exists. if not, checkFile will silently
//        return false.
//
//  NEEDSWORK: make checkFile better for higher nastiness levels

bool checkFile(string fname, Hash testhash, int nastiness) {
    FileHandler fhandler(fname, nastiness);
    Hash fhash(fhandler.getFile(), fhandler.getLength());

    c150debug->printf(
        C150APPLICATION,
        "checkFile: Hash=[%s] computed for fname=%s, against server hash=[%s]",
        fhash.str().c_str(), fname.c_str(), testhash.str().c_str()
    );
    *GRADING << "File: " << fname << " comparing client checksum ["
             << fhash.str() << "] against server checksum ["
             << testhash.str() << "]" << endl;

    return fhandler.getFile() != NULL && fhash == testhash;
}


// sendCheckResult
//      - sends the result of the end2end to the server
//
//  args:
//      - sock: socket
//      - fileid: id for file negotiated with server
//      - result: whether the check passed or not
//
//  result:
//      - 0, server successfully renamed/removed file
//      - -1 if timed out
//      - -2 if server failed to rename/remove

int sendCheckResult(C150DgmSocket *sock, int fileid, bool result) {
    FLAG resFlag = result ? POS_FL : NEG_FL;
    Packet ipckt;
    Packet opckt(fileid, CHECK_FL | resFlag, NULL_SEQNO, NULL, 0);
    PacketExpect expect = { fileid, CHECK_FL | FIN_FL };
    ssize_t datalen;

    c150debug->printf(
        C150APPLICATION,
        "sendCheckResult: Sending result=%s",
        result ? "passed" : "failed"
    );
    datalen = writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES);

    if (datalen < 0) {
        return -1;
    } else if (ipckt.flags & NEG_FL) {
        c150debug->printf(
            C150APPLICATION,
            "sendCheckResult: Server failed to %s file",
            result ? "rename" : "remove"
        );
        return -2;
    } else {
        c150debug->printf(
            C150APPLICATION,
            "sendCheckResult: Server successfully %s file",
            result ? "renamed" : "removed"
        );
        return 0;
    }
}


// ==========
// FINISH
// ==========

// sendFin
//      - this is to tell the server it can cleanup. however, if this packet is
//      - lossed, server will eventually timeout and cleanup anyway, so no need 
//      - to resend.

void sendFin(C150DgmSocket *sock, int fileid) {
    Packet opckt(fileid, FIN_FL, NULL_SEQNO, NULL, 0);

    c150debug->printf(C150APPLICATION, "sendFin: Sending final FIN");
    writePacket(sock, &opckt);
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
//      - 0, success
//      - -1, timeout
//      - -2, file check denied
//      - -3, file read error
//      - -4, server cleanup error (could not rename/remove)
//
//  notes:
//      - if directory or file is invalid, nothing happens
//      - if network fails, server is assumed down and exception is thrown
//
//  NEEDSWORK: implement filecopy, currently only does end to end checking
//  NEEDSWORK: once filecopy implemented, move check req to own function
//  NEEDSWORK: make end-to-end check better, currently just one attempt

int sendFile(
    C150DgmSocket *sock, 
    string dir, string fname, int fileNastiness
) {
    int retval = 0; // sendFile return value
    int sendVal; // for results of sending packets at each stage

    // file vars
    string fullname = makeFileName(dir, fname);
    FileHandler fhandler(fullname, fileNastiness);

    // network vars
    Packet ipckt, opckt;
    PacketExpect expect;
    int fileid;
    int checkAttempt = 1; // does nothing for now

    // check if file was successfully loaded
    if (fhandler.getFile() == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "sendFile: File %s could not be loaded. Skipping...",
            fullname.c_str()
        );
        return -3;
    }

    // send check request
    // when filecopy implemented, put in own function, but right now needs more
    // info than eventually neccessary.
    *GRADING << "File: " << fname
             << " requesting end-to-end check, attempt " << checkAttempt
             << endl;

    opckt = Packet(
        NULL_FILEID, REQ_FL | CHECK_FL, NULL_SEQNO,
        fname.c_str(), fname.length()+1 // +1 for null term
    );
    expect.fileid = NULL_FILEID;
    expect.flags = REQ_FL | CHECK_FL;
    if (writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES) < 0) {
        return -1;
    } else if (ipckt.flags & NEG_FL) {
        c150debug->printf(
            C150APPLICATION,
            "sendFile: Check request for fname=%s denied",
            fullname.c_str()
        );
        return -2;
    } else if (ipckt.flags & POS_FL) {
        fileid = ipckt.fileid;
    }

    // send check result
    bool checkResult = checkFile(fullname, Hash(ipckt.data), fileNastiness);
    *GRADING << "File: " << fname << " end-to-end check "
             << (checkResult ? "succeeded" : "failed") << ", attempt "
             << checkAttempt << endl;
    sendVal = sendCheckResult(sock, fileid, checkResult);

    if (sendVal == -1) {
        return -1;
    } else if (sendVal == -2) { // server failed to rename/remove
        retval = -4;
    }

    // send final fin
    sendFin(sock, fileid);

    return retval;
}


// sendDir
//      - sends an entire directory to server
//      - subdirectories are skipped
//
//  args:
//      - sock: socket
//      - dir: name of directory
//      - fileNastiness: with which to send files
//
//  returns: n/a
//
//  notes:
//      - if file send fails, just move on to next file
//
//  NEEDSWORK: add retry mechanism for failed files

void sendDir(C150DgmSocket *sock, string dirname, int fileNastiness) {
    // check to make sure directory can be opened
    if (!isDir(dirname)) {
        c150debug->printf(
            C150APPLICATION,
            "sendDir: Directory '%s' could not be opened",
            dirname.c_str()
        );
        return;
    }

    DIR *dir = opendir(dirname.c_str()); // will succeed since checked
    struct dirent *srcFile; // directory entry for source file

    // loop thru all files, and send all valid nondir files
    while ((srcFile = readdir(dir)) != NULL) {
        // skip . and ..
        if (strcmp(srcFile->d_name, ".") == 0 ||
            strcmp(srcFile->d_name, "..") == 0) {
            continue;
        } else if (isFile(makeFileName(dirname, srcFile->d_name))) {
            c150debug->printf(
                C150APPLICATION,
                "sendDir: Sending file '%s'",
                srcFile->d_name
            );
            sendFile(sock, dirname, srcFile->d_name, fileNastiness);
        } else {
            c150debug->printf(
                C150APPLICATION,
                "sendDir: Skipping subdirectory '%s'",
                srcFile->d_name
            );
        }
    }

    closedir(dir);
}
