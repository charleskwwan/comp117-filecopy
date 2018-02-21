// utils.h
//
// Defines utility functions shared between fileclient and fileserver
//
// By: Justin Jo and Charles Wan

#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <algorithm> // std::max, std::min

#include "c150dgmsocket.h"
#include "c150debug.h"

#include "utils.h"
#include "packet.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// ==========
// 
// GENERAL
//
// ==========

// safeAtoi
//      - atoi with error checking
//
//  args:
//      - str: string representation of int
//      - ip: location to store resulting int, must be nonnull
//
//  returns:
//      - 0 for success
//      - -1 for failure

int safeAtoi(const char *str, int *ip) {
    int result = 0;
    
    if (strspn(str, "0123456789") == strlen(str)) {
        *ip = atoi(str);
    } else {
        result = -1;
    }

    return result;
}


// initDebugLog
//      - enables logging to either console or file
//
// args:
//      - logname: name of log file; if NULL, defaults to console
//      - progname: name of program
//
// returns: n/a

void initDebugLog(const char *logname, const char *progname) {
    if (logname != NULL) { // pipe logging to file
        ofstream *outstreamp = new ofstream(logname);
        DebugStream *filestreamp = new DebugStream(outstreamp);
        DebugStream::setDefaultLogger(filestreamp);
    }

    c150debug->setPrefix(progname);
    c150debug->enableTimestamp();

    c150debug->enableLogging(
        C150APPLICATION |
        C150NETWORKTRAFFIC |
        C150NETWORKDELIVERY |
        C150FILEDEBUG
    );
}


// ==========
// 
// NETWORK
//
// ==========


// readPacket
//      - reads a packet from a socket
//
//  args:
//      - sock: socket to read from
//      - pcktp: location to store packet
//
//  returns:
//      - length of data read if successful
//      - -1 if timed out

ssize_t readPacket(C150DgmSocket *sock, Packet *pcktp) {
    ssize_t readlen = sock -> read((char*)pcktp, MAX_PCKT_LEN);

    if (sock -> timedout()) {
        // automatically logged to c150debug
        return -1;
    } else {
        pcktp->data[readlen - HDR_LEN + 1] = '\0'; // ensure null terminated
        return readlen;
    }
}


// writePacket
//      - writes a packet to socket
//
//  args:
//      - sock: socket to write to
//      - pcktp: location of packet to be sent
//      - datalen: length of data section in packet (incl. )
//
//  returns: n/a
//
//  note:
//      - if datalen exceeds max allowed, writePacket will SILENTLY send the max
//        allowed

void writePacket(C150DgmSocket *sock, const Packet *pcktp, int datalen) {
    datalen = min(datalen, MAX_WRITE_LEN);
    sock -> write((char*)pcktp, HDR_LEN + datalen);
}


// checks if a packet is expected
//      - use Packet* instead of Packet since packets are pretty big

bool isExpected(Packet *pcktp, PacketExpect expect) {
    return (expect.fileid == pcktp->fileid || expect.fileid == NULL_FILEID) &&
           (expect.flags & pcktp->flags) == expect.flags;
}


// ==========
// 
// FILES
//
// ==========

// checks if given dirname specifies a valid directory
bool isDir(string dirname) {
    struct stat statbuf;
    DIR *dir;

    if (lstat(dirname.c_str(), &statbuf) != 0) {
        c150debug->printf(
            C150APPLICATION,
            "isDir: Directory '%s' does not exist\n",
            dirname.c_str()
        );
        return false;
    }

    if (!S_ISDIR(statbuf.st_mode)) {
        c150debug->printf(
            C150APPLICATION,
            "isDir: File '%s' exists but is not a directory\n",
            dirname.c_str()
        );
        return false;
    }

    dir = opendir(dirname.c_str());
    if (dir == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "isDir: Directory '%s' could not be opened\n",
            dirname.c_str()
        );
        return false;
    }
    closedir(dir);

    return true;
}


// checks if given fname specifies a valid file
bool isFile(string fname) {
    struct stat statbuf;

    if (lstat(fname.c_str(), &statbuf) != 0) {
        c150debug->printf(
            C150APPLICATION,
            "isFile: File '%s' does not exist\n",
            fname.c_str()
        );
        return false;
    }
    if (!S_ISREG(statbuf.st_mode)) {
        c150debug->printf(
            C150APPLICATION,
            "isFile: File '%s' exists but is not a regular file\n",
            fname.c_str()
        );
        return false;
    }

    return true;
}


// combines a directory and file name, making sure there's a / in between
string makeFileName(string dirname, string fname) {
    stringstream ss;

    ss << dirname;
    // ensure dirname ends with /
    if (dirname.substr(dirname.length() - 1, 1) != "/") ss << '/';
    ss << fname;

    return ss.str();
}


// getFileSize
//  returns:
//      - size of file
//      - -1, if file was invalid

ssize_t getFileSize(string fname) {
    struct stat statbuf;
    return lstat(fname.c_str(), &statbuf) != 0 ? -1 : statbuf.st_size;
}
