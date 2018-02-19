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
const char SYN_FLAG = 0x01;
const char BUSY_FLAG = 0x02;
const char FILE_FLAG = 0x04;
const char CHECK_FLAG = 0x08;
const char RES_FLAG = 0x10;
const char FIN_FLAG = 0x20;


// ==========
// 
// PACKET
//
// ==========

struct Packet {
    int fileid;
    int seqno; // sequence number
    char flags;
    char data[MAX_DATA_LEN];
};


#endif
