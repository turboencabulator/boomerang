/**
 * \file
 * \brief Provides the definition for the miscellaneous bits and pieces
 *        implemented in the util.so library.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef UTIL_H
#define UTIL_H

#include <string>

// Add string and integer
std::string operator +(const std::string &s, int i);

void escapeXMLChars(std::string &s);
char *escapeStr(const char *str);

int lockFileRead(const std::string &fname);
int lockFileWrite(const std::string &fname);
void unlockFile(int n);

#endif
