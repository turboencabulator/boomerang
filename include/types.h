/**
 * \file
 * \brief Some often-used basic type definitions.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

// Machine types
typedef uint8_t  Byte;
typedef uint16_t SWord;
typedef uint32_t DWord;
typedef uint64_t QWord;

typedef unsigned int ADDRESS;       /* 32-bit unsigned */
typedef ADDRESS dword;              /* for use in decoders */

#define STD_SIZE    32              // Standard size
#define NO_ADDRESS ((ADDRESS)-1)    // For invalid ADDRESSes

#endif
