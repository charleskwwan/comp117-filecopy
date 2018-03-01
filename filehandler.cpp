// filehandler.h
//
// Defines a class for handling the read and write of files
//
// By: Justin Jo and Charles Wan


#include <cstdlib> // use over new/delete since realloc
#include <dirent.h>
#include <algorithm> // min/max
#include <cerrno>
#include <cstring>
#include <string>
#include <sstream>
#include <map>

#include "c150nastyfile.h"
#include "c150debug.h"

#include "filehandler.h"
#include "utils.h"
#include "hash.h"

using namespace std; // for C++ std lib
using namespace C150NETWORK; // for all comp150 utils


// consts
static const int RW_TRIES = 100; // number of a tries to read write to ensure
                                 // correctness


// ==========
// 
// PROTECTED
//
// ==========

// cleans up and resets members of FileHandler

void FileHandler::cleanup() {
    if (buf != NULL) free(buf);
    buf = NULL;
    buflen = 0;
}


// nastyReadPart
//      - reads part of a file in such a way to ensure it is correct
//
//  args:
//      - offset: from beginning of file
//      - nbytes: number of bytes to read
//
//  returns:
//      - number of bytes successfully read
//
//  notes:
//      - algorithm assumes that most common hash is correct hash
//      - offset + nbytes must not exceed file size
//      - buf must already have enough space for entire file
//
//  NEEDSWORK: would have loved to pass NASTYFILE fp as arg, but has no default
//             constructor so not allowed

size_t FileHandler::nastyReadPart(NASTYFILE &fp, int offset, size_t nbytes) {
    map<Hash, int> ctr; // counts number of times each hash was seen
    size_t readlen;
    Hash hash;
    int maxCnt = 0; // for hash

    // do many reads and compute hashes for each
    for (int i = 0; i < RW_TRIES; i++) {
        fp.fseek(offset, SEEK_SET); // go to offset from beginning of file
        readlen = fp.fread(buf + offset, 1, nbytes);

        hash.set(buf + offset, nbytes);
        ctr[hash] = ctr.count(hash) ? ctr[hash] + 1 : 0;
    }

    // find most common hash
    for (map<Hash, int>::iterator it = ctr.begin(); it != ctr.end(); it++)
        if (it->second > maxCnt) {
            maxCnt = it->second;
            hash = it->first;
        }

    // read until data with most common hash found
    do {
        fp.fseek(offset, SEEK_SET); // go to offset from beginning of file
        readlen = fp.fread(buf + offset, 1, nbytes);
    } while (!(Hash(buf + offset, nbytes) == hash));

    return readlen;
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
    if (!isFile(fname, nastiness)) return -1;

    size_t fsize = (size_t)getFileSize(fname);
    int nparts = fsize / MAX_WRITE_LEN + (fsize % MAX_WRITE_LEN ? 1 : 0);
    NASTYFILE fp(nastiness);
    int retval = 0;

    buf = (char *)malloc(fsize); // allocate enough for full file
    buflen = 0;

    // try read whole file
    fp.fopen(fname.c_str(), "rb"); // isFile already verified can open
    for (int i = 0; i < nparts; i++) {
        int off = i * MAX_WRITE_LEN;
        size_t nbytes = min((size_t)MAX_WRITE_LEN, fsize - off);
        buflen += nastyReadPart(fp, off, nbytes);
    }

    // fp.fread(buf, 1, fsize); // how ever much read, set to that

    if (buflen != fsize) {
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
//      - primarily for file reading

FileHandler::FileHandler(string _fname, int _nastiness) {
    setName(_fname);
    nastiness = _nastiness;
    buf = NULL; // avoid double free error
    read(); // set buf, buflen
}


// consturctor
//      - saves fname, flen as buflen, and nastiness
//      - allocates a buf of size flen. client's responsibility to fill
//      - primarily for file writing

FileHandler::FileHandler(string _fname, size_t flen, int _nastiness) {
    setName(_fname);
    nastiness = _nastiness;
    buf = NULL; // avoid bad realloc
    setLength(flen);
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
    buf = (char *)malloc(srclen);
    strncpy(buf, src, srclen);
    buflen = srclen;
}


// returns length of file

size_t FileHandler::getLength() {
    return buflen;
}


// sets new length for buffer
//      - if buf has existing data, the first _buflen bytes are copied to the
//        new buffer

void FileHandler::setLength(size_t _buflen) {
    buflen = _buflen;
    buf = (char *)realloc(buf, buflen);
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


// [] overload, for access to buf
//      - if i is out of bounds, C150FileException is thrown

char &FileHandler::operator[](size_t i) {
    if (i < 0 || i >= buflen) {
        throw C150FileException("FileHandler: index out of bounds");
    }
    return buf[i];
}
