// fileutils.h
//
// Includes utility functions shared between fileclient and fileserver
//
// By: Justin Jo and Charles Wan

#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_


// safeAtoi
//      - atoi with error checking
//
//  args:
//      - str: string representation of int
//      - intp: location to store resulting int, must be nonnull
//
//  returns:
//      - 0 for success
//      - -1 for failure

int safeAtoi(const char *str, int *ip) {
    int result = 0;
    
    if (strspn(str, "0123456789") == strlen(str)) {
        *ip = atoi(str);
    } else {
        result = -1;
    }

    return result;
}

#endif
