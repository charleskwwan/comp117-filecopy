// fileutils.h
//
// Includes utility functions shared between fileclient and fileserver
//
// By: Justin Jo and Charles Wan

#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_


#include <fstream>
#include <algorithm> // std::max

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
    datalen = max(datalen, MAX_WRITE_LEN);
    sock -> write((char*)pcktp, HDR_LEN + datalen);
}


#endif
