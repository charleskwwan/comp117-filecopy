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
//      - classes: for which to log
//
// returns: n/a

void initDebugLog(const char *logname, const char *progname, uint32_t classes) {
    if (logname != NULL) { // pipe logging to file
        ofstream *outstreamp = new ofstream(logname);
        DebugStream *filestreamp = new DebugStream(outstreamp);
        DebugStream::setDefaultLogger(filestreamp);
    }

    c150debug->setPrefix(progname);
    c150debug->enableTimestamp();

    c150debug->enableLogging(classes);
}


// prints a packet to a given stream

void printPacket(Packet &pckt, FILE *fp) {
    fprintf(
        fp,
        "Printing packet:\n"
        "   fileid: %d\n"
        "   flags: %x\n"
        "   seqno: %d\n"
        "   datalen: %d\n",
        pckt.fileid, pckt.flags & 0xff, pckt.seqno, pckt.datalen
    );
}


// prints sha1 hash in hex

void printHash(const unsigned char *hash, FILE *fp) {
    fprintf(fp, "Printing hash: ");
    for (int i = 0; i < 20; i++)
        fprintf(fp, "%02x", (unsigned int)hash[i]);
    fprintf(fp, "\n");
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
//      - length of data member in packet read if successful
//      - -1 if timed out

ssize_t readPacket(C150DgmSocket *sock, Packet *pcktp) {
    ssize_t readlen = sock -> read((char*)pcktp, MAX_PCKT_LEN);

    if (sock -> timedout()) {
        c150debug->printf(C150APPLICATION, "readPacket: Timeout occurred");
        return -1;
    } else {
        pcktp->data[readlen - HDR_LEN + 1] = '\0'; // ensure null terminated
        return readlen - HDR_LEN;
    }
}


// writePacket
//      - writes a packet to socket
//
//  args:
//      - sock: socket to write to
//      - pcktp: location of packet to be sent
//
//  returns: n/a
//
//  note:
//      - if datalen exceeds max allowed, writePacket will send a copy of the
//        packet with the max allowed datalen, but NOT modify the original
//        packet
//      - although pcktp could be made simply pckt (not a pointer) since copy
//        will be sent anyway, pcktp is kept to be consistent with write
//        style of interface

void writePacket(C150DgmSocket *sock, const Packet *pcktp) {
    Packet pckt = *pcktp;
    pckt.datalen = min(pckt.datalen, MAX_WRITE_LEN);
    sock -> write((char *)&pckt, HDR_LEN + pckt.datalen);
}


// checks if a packet is expected
//      - flag checks to see if flags were set, but does not preclude other
//        flags from being set

bool isExpected(const Packet &pckt, PacketExpect expect) {
    return (expect.fileid == pckt.fileid || expect.fileid == NULL_FILEID) &&
           (expect.flags & pckt.flags) == expect.flags;
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
            "isDir: Directory '%s' does not exist",
            dirname.c_str()
        );
        return false;
    }

    if (!S_ISDIR(statbuf.st_mode)) {
        c150debug->printf(
            C150APPLICATION,
            "isDir: File '%s' exists but is not a directory",
            dirname.c_str()
        );
        return false;
    }

    dir = opendir(dirname.c_str());
    if (dir == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "isDir: Directory '%s' could not be opened",
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
            "isFile: File '%s' does not exist",
            fname.c_str()
        );
        return false;
    }
    if (!S_ISREG(statbuf.st_mode)) {
        c150debug->printf(
            C150APPLICATION,
            "isFile: File '%s' exists but is not a regular file",
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
