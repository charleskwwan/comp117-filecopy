// packet.h
//
// Defines packet struct
//
// By: Justin Jo and Charles Wan


#ifndef _FCOPY_PACKET_H_
#define _FCOPY_PACKET_H_


#include <cstring>
#include <algorithm> // std::min

using namespace std;


// typedefs
typedef char FLAG;


// constants 
const unsigned short HDR_LEN = 2 * sizeof(int) + sizeof(FLAG) + sizeof(short);
const unsigned short MAX_DATA_LEN = 512 - HDR_LEN;
const unsigned short MAX_WRITE_LEN = MAX_DATA_LEN - 1; // reserve 1 for null
                                                       // terminator
const unsigned short MAX_PCKT_LEN = HDR_LEN + MAX_WRITE_LEN;
const int NULL_FILEID = 0; // should be used to denote the lack of a fileid
const int NULL_SEQNO = 0; // should be used to denote the lack of a seqno


// flag masks
const FLAG NO_FLS = 0;
const FLAG ALL_FLS = 0xFF;
const FLAG REQ_FL = 0x01;
const FLAG FILE_FL = 0x02;
const FLAG CHECK_FL = 0x04;
const FLAG FIN_FL = 0x08;
const FLAG POS_FL = 0x10;
const FLAG NEG_FL = 0x20;


// ==========
// 
// PACKET
//
// ==========

// packed attribute required to ensure members are organized exactly as shown,
// preventing compiler from adding padding 
struct __attribute__((__packed__)) Packet {
    int fileid;
    FLAG flags;
    int seqno; // sequence number
    unsigned short datalen;
    char data[MAX_DATA_LEN];


    Packet() {}; // default constructor

    // constructor
    //      - will copy up to first MAX_WRITE_LEN bytes from _data into data

    Packet(
        int _fileid, FLAG _flags, int _seqno,
        const char *_data, unsigned short datalen
    ) {
        fileid = _fileid;
        flags = _flags;
        seqno = _seqno;

        datalen = min(datalen, MAX_WRITE_LEN);
        strncpy(data, _data, datalen);
    }
};


#endif
