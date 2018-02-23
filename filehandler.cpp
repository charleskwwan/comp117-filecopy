// filehandler.h
//
// Defines a class for handling the read and write of files
//
// By: Justin Jo and Charles Wan


#include <dirent.h>

#include <cerrno>
#include <string>
#include <sstream>
#include <openssl/sha.h>

#include "c150nastyfile.h"
#include "c150debug.h"

#include "filehandler.h"
#include "utils.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// ==========
// 
// PROTECTED
//
// ==========

// cleans up and resets members of FileHandler

void FileHandler::cleanup() {
    if (buf != NULL) delete [] buf;
    buf = NULL;
    buflen = 0;
}


// read
//      - reads file to buf
//      - sets buflen to amt of file read
//      - if unsuccessful, sets buf to NULL
//
//  args: n/a
//
//  returns:
//      - length of data read
//      - error code if unsuccessful
//          - -1, if file is invalid

int FileHandler::read() {
    // reset buf and buflen
    cleanup();

    // check file is valid
    if (!isFile(fname)) return -1;

    ssize_t fsize = getFileSize(fname);
    NASTYFILE fp(nastiness);
    int retval = 0;

    buf = new char[fsize]; // allocate enough for full file

    // open file in rb to avoid line end munging
    if (fp.fopen(fname.c_str(), "rb") == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "readFile: Error opening file %s, errno=%s",
            fname.c_str(), strerror(errno)
        );
        return errno; // return straight away, since no more work needed
    }

    // try read whole file
    buflen = fp.fread(buf, 1, fsize); // how ever much read, set to that
    if (buflen != (size_t)fsize) {
        c150debug->printf(
            C150APPLICATION,
            "readFile: Error reading file %s, errno=%s",
            fname.c_str(), strerror(errno)
        );
        retval = errno; // still should close fp
    }

    // close file - unlikely to fail but check anyway
    if (fp.fclose() != 0) {
        c150debug->printf(
            C150APPLICATION,
            "readFile: Error closing file %s, errno=%s",
            fname.c_str(), strerror(errno)
        );
        retval = errno;
    }

    return retval;
}


// ==========
// 
// PUBLIC
//
// ==========

// constructor

FileHandler::FileHandler(int _nastiness) {
    nastiness = _nastiness;
    buf = NULL;
    buflen = 0;
}

// constructor
//      - saves fname and nastiness
//      - automatically attempts to read file specified by fname to buf

FileHandler::FileHandler(string _fname, int _nastiness) {
    setName(_fname);
    nastiness = _nastiness;
    buf = NULL; // avoid double free error
    read(); // set buf, buflen
}

// destructor

FileHandler::~FileHandler() {
    cleanup();
}


// returns a copy of fname

string FileHandler::getName() {
    stringstream ss;
    ss << fname;
    return ss.str();
}


// sets fname to _fname

void FileHandler::setName(string _fname) {
    stringstream ss;
    ss << _fname;
    fname = ss.str();
}


// returns entire file
//      - file cannot be altered outside the FileHandler, only read

const char *FileHandler::getFile() {
    return buf;
}


// sets buf and buflen
//      - _buf and _buflen are expected to match
//      - if say NULL, 6 is provided, FileHandler will have undefined behavior
//        going forward

void FileHandler::setFile(const char *src, size_t srclen) {
    cleanup();
    buf = new char[srclen];
    strncpy(buf, src, srclen);
    buflen = srclen;
}


// returns length of file

size_t FileHandler::getLength() {
    return buflen;
}


// write
//      - writes a buf to a file named fname
//      - WARNING: will silently overwrite existing files with intended name
//      - if no file data is buffered, no file is created
//
//  args: n/a
//
//  returns:
//      - length of data written
//      - error code if unsuccessful
//          - -1 if no file data found

int FileHandler::write() {
    if (buf == NULL) return -1; // check if file data exists

    NASTYFILE fp(nastiness);
    int retval = 0;

    // open file in wb to avoid line end munging
    if (fp.fopen(fname.c_str(), "wb") == NULL) {
        c150debug->printf(
            C150APPLICATION,
            "FileHandler::write: Error opening file %s, errno=%s",
            fname.c_str(), strerror(errno)
        );
        return errno; // return straight away, since no more work needed
    }

    // try to write all file data
    if (fp.fwrite(buf, 1, buflen) != buflen) {
        c150debug->printf(
            C150APPLICATION,
            "FileHandler::write: Error writing file %s, errno=%s",
            fname.c_str(), strerror(errno)
        );
        retval = errno; // still should close fp
    }

    // close file - unlikely to fail but check anyway
    if (fp.fclose() != 0) {
        c150debug->printf(
            C150APPLICATION,
            "FileHandler::write: Error closing file %s, errno=%s",
            fname.c_str(), strerror(errno)
        );
        retval = errno;
    }

    return retval;
}
