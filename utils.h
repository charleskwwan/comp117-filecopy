// utils.h
//
// Declares utility functions shared between fileclient and fileserver
//
// By: Justin Jo and Charles Wan

#ifndef _FCOPY_UTILS_H_
#define _FCOPY_UTILS_H_


#include <vector>

#include "c150dgmsocket.h"
#include "packet.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// ==========
// 
// GENERAL
//
// ==========

// functions
int safeAtoi(const char *str, int *ip); // atoi with error checking
void initDebugLog(const char *logname, const char *progname, uint32_t classes);
void printPacket(Packet &pckt, FILE *fp);


// ==========
// 
// NETWORK
//
// ==========

// PacketExpect
//      - defines an expectation for the next read packet by certain identifying
//        values in a packet

struct PacketExpect {
    int fileid; // when = NULL_FILEID (filepacket.h), any fileid allowed
    FLAG flags;
};


// functions
ssize_t readPacket(C150DgmSocket *sock, Packet *pcktp);
void writePacket(C150DgmSocket *sock, const Packet *pcktp);
bool isExpected(const Packet &pckt, PacketExpect expect);
int splitFile(
    vector<Packet> &parts, const Packet &hdr,
    const char *file, size_t flen
);
size_t mergePackets(vector<Packet> &pckts, char *buf, size_t buflen);


// ==========
// 
// FILES
//
// ==========

// functions
bool isDir(string dirname);
bool isFile(string fname);
string makeFileName(string dirname, string fname); // make dirname/fname
ssize_t getFileSize(string fname);


#endif
