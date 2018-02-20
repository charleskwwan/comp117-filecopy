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


// ==========
// 
// FILES
//
// ==========

// checks if given dirname specifies a valid directory
bool isDir(string dirname) {
    struct stat statbuf;

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

string hashFile(string fname) {
    if (!isFile(fname)) return '\0'; // verify file

    ifstream *t = new ifstream(fname.c_str());
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


// readFile
//      - reads a file from a given directory
//
//  args:
//      - dirname: name of directory
//      - fname: file name
//      - nastiness: with which to read the file
//      - bufp: location to put resulting file buffer
//
//  returns:
//      - 0 if non errors occurred
//      - nonzero error code if something happened
//          - -1 specifies file was bad
//
//  note:
//      - readFile will allocate a buffer with the file in it
//      - allocated buffer must be DELETED by caller
//      - a returned error code DOES NOT mean that file read failed. caller
//        needs to check buffer at bufp.
//          - if *bufp = null, readFile was unsuccessful
//          - if *bufp != null, readFile successfully read file
//
//  NEEDSWORK: bad design to have allocation by callee and deallocation by
//             caller. however, wanted return value to be able to show error
//             somehow. consdiered string return type, but string cannot
//             represent a nonvalue like NULL.

int readFile(string dirname, string fname, int nastiness, char **bufp) {
    *bufp = NULL; // assume unsuccessful fread
    ssize_t fsize = getFileSize(fname);

    // verify that file exists
    if (fsize < 0) return -1;

    string fullFname = makeFileName(dirname, fname);
    char *buf = new char[fsize]; // buffer for full file
    NASTYFILE fp(nastiness);
    int retval = 0;

    // open file in rb to avoid line end munging
    if (fp.fopen(fullFname.c_str(), "rb") == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "readFile: Error opening file %s, errno=%s",
            fullFname.c_str(), strerror(errno)
        );
        return errno; // return straight away, since no more work needed
    }

    // read whole file
    if (fp.fread(buf, 1, fsize) != (size_t)fsize) {
        c150debug->printf(
            C150APPLICATION,
            "readFile: Error reading file %s, errno=%s",
            fullFname.c_str(), strerror(errno)
        );
        retval = errno; // still should close fp
    } else {
        *bufp = buf; // fread successful, return to buffer to client
    }

    // close file - unlikely to fail but check anyway
    if (fp.fclose() != 0) {
        c150debug->printf(
            C150APPLICATION,
            "readFile: Error closing file %s, errno=%s",
            fullFname.c_str(), strerror(errno)
        );
        retval = errno;
    }

    return retval;
}


#endif
