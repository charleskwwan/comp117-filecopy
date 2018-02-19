// filepacket.h
//
// Defines packet struct
//
// By: Justin Jo and Charles Wan


#ifndef _FILEPACKET_H_
#define _FILEPACKET_H_


// constants 
const int HDR_LEN = 9;
const int MAX_WRITE_LEN = 495;
const int MAX_DATA_LEN = MAX_WRITE_LEN + 1; // reserve 1 for null terminator
                                            // to be set by read
const int MAX_PCKT_LEN = HDR_LEN + MAX_WRITE_LEN;

// flag masks
typedef char FLAG;
const FLAG NO_FLAGS = 0;
const FLAG SYN_FLAG = 0x01;
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

struct PacketId {
    int fileid;
    FLAG flags;
};

struct Packet {
    PacketId id;
    int seqno; // sequence number
    char data[MAX_DATA_LEN];
};


#endif
