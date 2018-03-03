// filehandler.h
//
// Declares a class for handling the read and write of files
//
// By: Justin Jo and Charles Wan

#ifndef _FCOPY_FILEHANDLER_H_
#define _FCOPY_FILEHANDLER_H_


#include <string>

#include "c150nastyfile.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// ==========
// 
// FILEHANDLER
//
// ==========

class FileHandler {
public:
    FileHandler(int _nastiness);
    FileHandler(string _fname, int _nastiness); // read existing file
    FileHandler(string _fname, size_t flen, int _nastiness); // allocate only
    ~FileHandler();

    string getName();
    void setName(string _fname);

    const char *getFile();
    int setFile(const char *src, size_t srclen, size_t offset);
    void setFile(const char *src, size_t srclen);

    size_t getLength(); // get length of file
    void setLength(size_t _buflen); // set length of file

    int write(); // write current buf to current fname

    char &operator[] (size_t i);    

protected:
    string fname; // filename
    char *buf; // file buffer
    size_t buflen; // length of buffer
    int nastiness; // nastiness with which to read file

    void cleanup();
    size_t nastyReadPart(NASTYFILE &fp, int offset, size_t nbytes);
    int read(); // read file with fname to buf
};

#endif
