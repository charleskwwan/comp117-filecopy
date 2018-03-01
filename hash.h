// hash.h
//
// Defines hash class
//
// By: Justin Jo and Charles Wan

#ifndef _FCOPY_HASH_H_
#define _FCOPY_HASH_H_


#include <cstring>
#include <string>

#include <openssl/sha.h>


// constants
const unsigned short HASH_LEN = 20;


// ==========
// 
// HASH
//
// ==========

class Hash {
public:
    Hash() { set(NULL, 0); } // set everything to \0
    Hash(const char *file, size_t filelen) { set(file, filelen); } // from file
    Hash(const char *_hash) { set(_hash); } // from existing hash
    ~Hash() {};


    // returns stored hash, only guaranteed to be the same until next set
    const unsigned char *get() {
        return hash;
    }


    // hashes file and stores resulting hash
    //      - if file == NULL, hash is set to 0
    //      - if file is invalid, undefined behavior

    void set(const char *file, size_t filelen) {
        if (file == NULL) {
            for (int i = 0; i < HASH_LEN; i++) hash[i] = '\0';
        } else {
            SHA1((const unsigned char *)file, filelen, hash);
        }
    }


    // copy and store a preexisting hash
    void set(const char *_hash) {
        if (_hash == NULL) {
            set(NULL, 0); // set to all '\0'
        } else {
            strncpy((char *)hash, _hash, HASH_LEN);
        }
    }


    // converts hash to a printable string of hex chars
    string str() {
        char s[2 * HASH_LEN + 1]; // 2 hex per hash char, +1 null term
        for (int i = 0; i < HASH_LEN; i++)
            sprintf(s+ 2 * i, "%02x", (unsigned int)hash[i]);
        s[40] = '\0'; // ensure null term
        return s;
    }


    // == overload
    bool const operator==(const Hash &o) const {
        if (hash == NULL && o.hash == NULL) {
            return true;
        } else if (hash == NULL || o.hash == NULL) {
            return false;
        } else {
            return strncmp(
                (const char *)hash, (const char *)o.hash, HASH_LEN
            ) == 0;
        }
    }


    // < overload
    bool const operator<(const Hash &o) const {
        if ((hash == NULL && o.hash == NULL) || o.hash == NULL) {
            return false;
        } else if (hash == NULL) {
            return true;
        } else {
            return strncmp(
                (const char *)hash, (const char *)o.hash, HASH_LEN
            ) < 0;
        }
    }


private:
    unsigned char hash[20];
};


// constants
const Hash NULL_HASH; // default is considered null hash

#endif
