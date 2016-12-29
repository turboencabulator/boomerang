/**
 * \file
 * \brief Needed by both signature.h and frontend.h
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef SIGENUM_H
#define SIGENUM_H

enum platform {
	PLAT_PENTIUM,
	PLAT_SPARC,
	PLAT_M68K,
	PLAT_PARISC,
	PLAT_PPC,
	PLAT_MIPS,
	PLAT_ST20,
	PLAT_GENERIC
};

enum callconv {
	CONV_C,         // Standard C, no callee pop
	CONV_PASCAL,    // callee pop
	CONV_THISCALL,  // MSVC "thiscall": one parameter in register ecx
	CONV_NONE
};

#endif
