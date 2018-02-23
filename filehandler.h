// filehandler.h
//
// Declares a class for handling the read and write of files
//
// By: Justin Jo and Charles Wan

#ifndef _FCOPY_FILEHANDLER_H_
#define _FCOPY_FILEHANDLER_H_

#include <string>


// ==========
// 
// FILEHANDLER
//
// ==========

// NEEDSWORK: overload [] operator so allow in place writing and individual
//            bytes access

class FileHandler {
public:
    FileHandler(int _nastiness);
    FileHandler(string _fname, int _nastiness); // read existing file
    ~FileHandler();

    string getName();
    void setName(string _fname);

    const char *getFile();
    void setFile(const char *src, size_t srclen);

    size_t getLength(); // get length of file
    int write(); // write current buf to current fname

protected:
    string fname; // filename
    char *buf; // file buffer
    size_t buflen; // length of buffer
    int nastiness; // nastiness with which to read file

    void cleanup();
    int read(); // read file with fname to buf
};

#endif
