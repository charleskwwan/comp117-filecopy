// utils.h
//
// Defines utility functions shared between fileclient and fileserver
//
// By: Justin Jo and Charles Wan

#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <string>
#include <algorithm> // max, min, sort
#include <vector>

#include "c150dgmsocket.h"
#include "c150nastyfile.h"
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
           (expect.flags & pckt.flags) == expect.flags &&
           (expect.seqno == pckt.seqno || expect.seqno == NULL_SEQNO);
}


// splitFile
//      - splits a file into multiple packets
//
//  args:
//      - parts: vector to store resulting packets. pckts WILL BE cleared and
//               may be reallocated
//      - hdr: splitFile will use control info from hdr for all packets. fileid
//             and flags will be same for all, but seqno provided will be used
//             as the initial seqno
//      - file: file data stored as byte array
//      - flen: length of file in bytes
//
//  returns:
//      - number of pckts created

int splitFile(
    vector<Packet> &parts, const Packet &hdr,
    const char *file, size_t flen
) {
    int npckts = flen / MAX_WRITE_LEN;
    int remainder = flen % MAX_WRITE_LEN; // last part may not fill packet

    // guarantee enough space for packets
    parts.reserve(npckts + (remainder != 0 ? 1 : 0));

    // create first n packets
    for (int i = 0; i < npckts; i++)
        parts.push_back(Packet(
            hdr.fileid, hdr.flags, hdr.seqno + i,
            file + i * MAX_WRITE_LEN, MAX_WRITE_LEN
        ));

    // check if remainder packet exists, and write if needed
    if (remainder != 0)
        parts.push_back(Packet(
            hdr.fileid, hdr.flags, hdr.seqno + npckts,
            file + npckts * MAX_WRITE_LEN, remainder
        ));

    return npckts + (remainder != 0 ? 1 : 0);
}


// mergePackets
//      - merges packets into a single file, stored in buf
//      - only the first buflen bytes of the file will be written to buf
//
//  args:
//      - pckts: packets to be merged
//      - initSeqno: initial sequence number
//      - buf: buffer to stored file
//      - buflen: max length of buf
//
//  returns:
//      - number of bytes successfully written to buf
//
//  note:
//      - if there are holes in pckts, there may be holes in buf, depending on
//        the size of buflen vs. actual size of file

size_t mergePackets(
    vector<Packet> &pckts, int initSeqno,
    char *buf, size_t buflen
) {
    size_t written;
    size_t offset, writelen;

    // write data to buf until buflen reached or all data successfull written
    for (vector<Packet>::iterator it = pckts.begin(); it != pckts.end(); it++) {
        offset = (it->seqno - initSeqno) * MAX_WRITE_LEN;

        if (offset >= buflen) {
            continue;
        } else {
            writelen = min(buflen - offset, (size_t)it->datalen);
            strncpy(buf + offset, it->data, writelen);
            written += writelen;
        }
    }

    return written;
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
    NASTYFILE fp(0); // use to check if file can be opened

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

    if (fp.fopen(fname.c_str(), "rb") == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "isFile: File '%s' could not be opened, errno=%s",
            fname.c_str(), strerror(errno)
        );
        return false;
    }
    fp.fclose();

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
