// packet.h
//
// Defines packet struct
//
// By: Justin Jo and Charles Wan


#ifndef _FCOPY_PACKET_H_
#define _FCOPY_PACKET_H_


#include <cstring>
#include <algorithm> // std::min

#include "c150dgmsocket.h" // for MAXDGMSIZE

using namespace std;
using namespace C150NETWORK; // for all comp150 utils


// typedefs
typedef char FLAG;


// constants 
const unsigned short HDR_LEN = 2 * sizeof(int) + sizeof(FLAG) + sizeof(short);
const unsigned short MAX_DATA_LEN = MAXDGMSIZE - HDR_LEN;
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
        const char *_data, unsigned short _datalen
    ) {
        fileid = _fileid;
        flags = _flags;
        seqno = _seqno;

        if (_data == NULL || _datalen == 0) {
            datalen = 0;
        } else {
            datalen = min(_datalen, MAX_WRITE_LEN);
            strncpy(data, _data, datalen);
        }
    }


    bool const operator==(const Packet &other) const {
        return fileid == other.fileid &&
               flags == other.flags &&
               seqno == other.seqno &&
               datalen == other.datalen &&
               strncmp(data, other.data, datalen) == 0;
    }


    // checks members in priority level
    //      - in each case, < and > are checked, as they clearly define a result
    //      - if ==, continue to next members as they might decide

    bool const operator<(const Packet &o) const {
        if (fileid < o.fileid) return true;
        if (fileid > o.fileid) return false;

        if (seqno < o.seqno) return true;
        if (seqno > o.seqno) return false;

        if (datalen < o.datalen) return true;
        if (datalen > o.datalen) return false;

        int cmpval = strncmp(data, o.data, datalen);
        if (cmpval < 0) return true;
        if (cmpval > 0) return false;

        if (flags < o.flags) return true;
        
        return false;
    }
};


#endif
