// filehandler.h
//
// Declares a class for handling the read and write of files
//
// By: Justin Jo and Charles Wan

#ifndef _FCOPY_FILEHANDLER_H_
#define _FCOPY_FILEHANDLER_H_

#include <string>


// constants
const unsigned short HASH_LEN = 20;


// ==========
// 
// FILEHANDLER
//
// ==========

class FileHandler {
public:
    FileHandler(int _nastiness);
    FileHandler(string _fname, int _nastiness); // read existing file
    ~FileHandler();

    string getName();
    void setName(string _fname);

    const char *getFile();
    void setFile(const char *src, size_t srclen);

    size_t getFileLength(); // get length of file
    const unsigned char *getHash();
    int write(); // write current buf to current fname

protected:
    string fname; // filename
    char *buf; // file buffer
    size_t buflen; // length of buffer
    unsigned char hash[HASH_LEN]; // hash for file in buf
    int nastiness; // nastiness with which to read file

    void cleanup();
    int read(); // read file with fname to buf
    void setHash(); // hashes buf, saves to hash
};

#endif
