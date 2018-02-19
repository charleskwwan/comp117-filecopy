// fileutils.h
//
// Includes utility functions shared between fileclient and fileserver
//
// By: Justin Jo and Charles Wan

#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_


#include <sys/stat.h> 
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm> // std::max, std::min
#include <openssl/sha.h>

#include "c150dgmsocket.h"
#include "c150debug.h"
#include "filepacket.h"

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


// readExpectedPacket
//      - reads packets from socket until an 'expected' packet type is received,
//        or a timeout occurs
//      - ALL packets that are not 'expected' are ignored and dropped
//
//  args:
//      - sock: socket to read from
//      - pcktp: location to store packet
//      - expected: expected packet id
//
//  returns:
//      - length of data read if successful
//      - -1 if timed out

ssize_t readExpectedPacket(
    C150DgmSocket *sock,Packet *pcktp,
    PacketId expected
) {
    Packet tmp;
    ssize_t readlen;

    // read until timeout OR expected flags found
    do {
        readlen = readPacket(sock, &tmp);
    } while (
        readlen != -1 && 
        !(tmp.id.fileid == expected.fileid && tmp.id.flags & expected.flags)
    );

    if (readlen != -1) {
        *pcktp = tmp;
    }

    return readlen;
}

// ==========
// 
// FILES
//
// ==========

// checks if given dirname specifies a valid directory
bool isDir(const char *dirname) {
    struct stat statbuf;

    if (lstat(dirname, &statbuf) != 0) {
        fprintf(
            stderr,
            "error (isDir): Directory '%s' does not exist\n",
            dirname
        );
        return false;
    }
    if (!S_ISDIR(statbuf.st_mode)) {
        fprintf(
            stderr,
            "error (isDir): File '%s' exists but is not a directory\n",
            dirname
        );
        return false;
    }

    return true;
}


// checks if given fname specifies a valid file
bool isFile(const char *fname) {
    struct stat statbuf;

    if (lstat(fname, &statbuf) != 0) {
        fprintf(
            stderr,
            "error (isFile): File '%s' does not exist\n",
            fname
        );
        return false;
    }
    if (!S_ISREG(statbuf.st_mode)) {
        fprintf(
            stderr,
            "error (isFile): File '%s' exists but is not a regular file\n",
            fname
        );
        return false;
    }

    return true;
}


// hashFile
//      - computes sha1 hash for a given file
//
//  args:
//      - fname: name of file
//
//  returns:
//      - hash as string if successful
//      - '\0' if file is invalid
//
//  NEEDSWORK (maybe): do we need to go through NASTYFILE?

string hashFile(const char *fname) {
    if (!isFile(fname)) return '\0'; // verify file

    ifstream *t = new ifstream(fname);
    stringstream *buffer = new stringstream;
    unsigned char obuf[20];

    *buffer << t->rdbuf();
    SHA1(
        (const unsigned char *)buffer->str().c_str(),
        (buffer->str()).length(),
        obuf
    );

    delete t;
    delete buffer;
    return string((char *)obuf);
}


#endif
