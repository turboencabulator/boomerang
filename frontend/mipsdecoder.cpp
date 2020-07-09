#define sign_extend(N,SIZE) (((int)((N) << (sizeof(unsigned)*8-(SIZE)))) >> (sizeof(unsigned)*8-(SIZE)))
#include <assert.h>

#line 1 "machine/mips/decoder.m"
/**
 * \file
 * \brief Decoding MIPS
 *
 * \authors
 * Copyright (C) 2007, Markus Gothe <nietzsche@lysator.liu.se>
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mipsdecoder.h"

#include "boomerang.h"
#include "rtl.h"

class Statement;

MIPSDecoder::MIPSDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get().getProgPath() + "frontend/machine/mips/mips.ssl";
	RTLDict.readSSLFile(file);
}

#if 0 // Cruft?
// For now...
int
MIPSDecoder::decodeAssemblyInstruction(ADDRESS, ptrdiff_t)
{
	return 0;
}
#endif

// Stub from PPC...
void
MIPSDecoder::decodeInstruction(DecodeResult &result, ADDRESS pc, const BinaryFile *bf)
{
	// Clear the result structure;
	result.reset();
}

#line 53 "mipsdecoder.cpp"

