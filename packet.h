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
const int HDR_LEN = 2 * sizeof(int) + sizeof(FLAG);
const int MAX_DATA_LEN = 512 - HDR_LEN;
const int MAX_WRITE_LEN = MAX_DATA_LEN - 1; // reserve 1 for null terminator
                                            // to be set by read
const int MAX_PCKT_LEN = HDR_LEN + MAX_WRITE_LEN;

const int NULL_FILEID = 0; // invalid fileid


// flag masks
const FLAG NO_FLAGS = 0;
const FLAG ALL_FLAGS = 0xFF;
const FLAG REQ_FLAG = 0x01;
const FLAG BUSY_FLAG = 0x02;
const FLAG FILE_FLAG = 0x04;
const FLAG CHECK_FLAG = 0x08;
const FLAG RES_FLAG = 0x10;
const FLAG FIN_FLAG = 0x20;


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
    char data[MAX_DATA_LEN];


    Packet() {}; // default constructor

    // constructor
    //      - will copy up to first MAX_WRITE_LEN bytes from _data into data

    Packet(
        int _fileid, FLAG _flags, int _seqno,
        const char *_data, int datalen
    ) {
        fileid = _fileid;
        flags = _flags;
        seqno = _seqno;

        datalen = min(datalen, MAX_WRITE_LEN);
        strncpy(data, _data, datalen);
    }
};


#endif
