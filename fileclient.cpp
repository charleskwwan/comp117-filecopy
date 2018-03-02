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
#include <vector>

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
const int TIMEOUT_DURATION = 50; // 0.05s
const int MAX_TRIES = 10;
const int MAX_CHK_ATTEMPTS = 10; // max number of check attempts


// fwd declarations
void usage(char *progname, int exitCode);
int sendFile(C150DgmSocket *sock, string dir, string fname, int fnastiness);
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

    // check for valid fileMastiness
    if (fileNastiness > 5) {
        fprintf(stderr, "error: <filenastiness> must be in range [0, 5].\n");
        usage(argv[0], 4);
    }


    // check target directory
    dir = argv[srcDirArg];
    if (!isDir(dir.c_str())) {
        fprintf(stderr, "error: '%s' is not a valid directory\n", dir.c_str());
        usage(argv[0], 8);
    }

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

        sendDir(sock, dir, fileNastiness);

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
// FILES
// ==========

// sendFileRequest
//      - constructs and sends a file request for a given file
// 
//  args:
//      - sock: socket
//      - fname: name of file to send
//      - fsize: size of file to send
//
//  returns:
//      - response packet containing new fileid and initial seqno, if successful
//      - error packet, if unsuccessful (timeout or request denied)
//
//  notes:
//      - sendFileRequest is not responsible for verifying file exists and can
//        be sent

Packet sendFileRequest(C150DgmSocket *sock, string fname, size_t fsize) {
    Packet ipckt = ERROR_PCKT; // default if fail
    Packet opckt(
        NULL_FILEID, REQ_FL | FILE_FL, fsize,
        fname.c_str(), fname.length() + 1 // +1 for null term
    );
    PacketExpect expect(NULL_FILEID, REQ_FL | FILE_FL, NULL_SEQNO);
    ssize_t datalen;

    c150debug->printf(
        C150APPLICATION,
        "sendFileRequest: Sending file request for fname=%s",
        fname.c_str()
    );
    // NEEDSWORK: add grading statement?
    datalen = writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES);

    if (datalen >= 0) { // nontimeout
        c150debug->printf(
            C150APPLICATION,
            "sendFileRequest: File request for fname=%s was %s",
            fname.c_str(), ipckt.flags & NEG_FL ? "denied" : "accepted"
        );
        ipckt = ipckt.flags & NEG_FL ? ERROR_PCKT : ipckt;
    }

    return ipckt;
}


// sendFileParts
//      - sends a file in packets one at a time
//
//  args:
//      - sock: socket
//      - fname: name of file
//      - nastiness: with which to read file
//      - fileid: negotiated with server during initial file request
//      - initSeqno: iniital sequence number
//
//  return:
//      - number of packets written, if successful
//      - -1, if unsuccessful

int sendFileParts(
    C150DgmSocket *sock,
    string fname, int nastiness,
    int fileid, int initSeqno
) {
    FileHandler fhandler(fname, nastiness);
    Packet hdr(fileid, FILE_FL, initSeqno, NULL, 0);
    Packet ipckt;
    vector<Packet> parts;
    int written = 0;

    // split file into packets
    splitFile(parts, hdr, fhandler.getFile(), fhandler.getLength());

    for (vector<Packet>::iterator it = parts.begin(); it != parts.end(); it++) {
        // send every packet one at a time, abort if unsuccessful
        PacketExpect expect(fileid, FILE_FL, initSeqno + written);
        Packet opckt = *it;

        c150debug->printf(
            C150APPLICATION,
            "sendFileParts: Sending file packet seqno=%d for fname=%s, "
            "fileid=%d, with datalen=%u",
            opckt.seqno, fname.c_str(), opckt.fileid, opckt.datalen
        );

        if (writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES) < 0)
            return -1;
        written++;
    }

    return written;
}


// ==========
// CHECKING
// ==========

// sendCheckRequest
//      - constructs and sends a check request for a file
//
// args:
//      - sock: socket
//      - fileid: file id
//      - attempt: attempt number
//
// return:
//      - Hash of the file, if request successfully sent and acknowledged
//      - NULL if error during request

Hash sendCheckRequest(C150DgmSocket *sock, int fileid, int attempt) {
    Packet ipckt;
    Packet opckt = Packet(fileid, REQ_FL | CHECK_FL, attempt, NULL, 0);
    PacketExpect expect(fileid, REQ_FL | CHECK_FL, attempt);

    if (writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES) >= 0) {
        c150debug->printf(
            C150APPLICATION,
            "sendCheckRequest: Check request for fileid=%u, attempt=%d was %s",
            fileid, attempt, ipckt.flags & NEG_FL ? "denied" : "accepted"
        );
        Hash hash(ipckt.data);

        return ipckt.flags & NEG_FL ? NULL_HASH : hash;
    }

    return NULL_HASH;
}

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
    Packet ipckt;
    Packet opckt(
        fileid, CHECK_FL | (result ? POS_FL : NEG_FL), NULL_SEQNO,
        NULL, 0
    );
    PacketExpect expect(fileid, CHECK_FL | FIN_FL, NULL_SEQNO);
    ssize_t datalen;

    c150debug->printf(
        C150APPLICATION,
        "sendCheckResult: Sending result=%s",
        result ? "passed" : "failed"
    );
    datalen = writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES);

    if (datalen < 0) {
        return -1;

    } else {
        c150debug->printf(
            C150APPLICATION,
            "sendCheckResult: Server %s %s file",
            ipckt.flags & NEG_FL ? "failed to" : "successfully",
            result ? "rename" : "remove"
        );
        return ipckt.flags & NEG_FL ? -2 : 0;
    }
}


// ==========
// FINISH
// ==========

// sendFin
//      - this is to tell the server it can cleanup. however, if this packet is
//      - lossed, server will eventually timeout and cleanup anyway, so no need 
//      - to resend.
//
//  args:
//      - sock: socket
//      - fileid: id for file negotiated with server
//
//  returns:
//      - 0, if successful
//      - -1, if timeout

ssize_t sendFin(C150DgmSocket *sock, int fileid) {
    Packet ipckt;
    Packet opckt(fileid, FIN_FL, NULL_SEQNO, NULL, 0);
    PacketExpect expect(fileid, FIN_FL, NULL_SEQNO);
    ssize_t datalen;

    c150debug->printf(C150APPLICATION, "sendFin: Sending final FIN");
    datalen = writePacketWithRetries(sock, &opckt, &ipckt, expect, MAX_TRIES);

    return datalen < 0 ? -1 : 0;
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
//      - fnastiness: nastiness with which to send file
//
//  return:
//      - 0, success
//      - -1, file request unsuccessful
//      - -2, failed to send file
//      - -3, check request denied
//      - -4, check result failed due to timeout
//      - -5, check result failed due to failed rename/remove on server
//
//  notes:
//      - if directory or file is invalid, nothing happens
//      - if network fails, server is assumed down and exception is thrown

int sendFile(
    C150DgmSocket *sock, 
    string dir, string fname, int fnastiness
) {
    string fullname = makeFileName(dir, fname);
    Packet initPckt;
    bool checkRes;

    // send initial file request
    initPckt = sendFileRequest(sock, fname, getFileSize(fullname));
    if (initPckt == ERROR_PCKT) return -1;

    // send file
    if (sendFileParts(
            sock,
            fullname, fnastiness,
            initPckt.fileid, initPckt.seqno
        ) < 0) {
        return -2;
    }

    // send check request after file sent done
    sock -> turnOnTimeouts(1000);
    for (int i = 0; i < MAX_CHK_ATTEMPTS; i++) {
        // send check req to get hash from server
        Hash hash = sendCheckRequest(sock, initPckt.fileid, i);
        if (hash == NULL_HASH) return -3;

        // check file to see if correct; if successful break
        checkRes = checkFile(fullname, hash, fnastiness);
        if (checkRes) break;
    }
    sock->turnOnTimeouts(TIMEOUT_DURATION);

    switch(sendCheckResult(sock, initPckt.fileid, checkRes)) {
        case -1:
            return -4;
        case -2: 
            sendFin(sock, initPckt.fileid);
            return -5;
    }

    sendFin(sock, initPckt.fileid);
    return 0;
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
        } else if (isFile(
                makeFileName(dirname, srcFile->d_name),
                fileNastiness)
            ) {
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
