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

/**
 * A dummy function to suppress "unused local variable" messages.
 *
 * \param x  Integer variable to be "used".
 */
void
MIPSDecoder::unused(int x)
{
}

MIPSDecoder::MIPSDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get()->getProgPath() + "frontend/machine/mips/mips.ssl";
	RTLDict.readSSLFile(file.c_str());
}

// For now...
int
MIPSDecoder::decodeAssemblyInstruction(ADDRESS, ptrdiff_t)
{
	return 0;
}

/**
 * Attempt to decode the high level instruction at a given address and return
 * the corresponding HL type (e.g. CallStatement, GotoStatement etc).  If no
 * high level instruction exists at the given address, then simply return the
 * RTL for the low level instruction at this address.  There is an option to
 * also include the low level statements for a HL instruction.
 *
 * \param pc     The native address of the pc.
 * \param delta  The difference between the above address and the host address
 *               of the pc (i.e. the address that the pc is at in the loaded
 *               object file).
 * \param proc   The enclosing procedure.  This can be NULL for those of us
 *               who are using this method in an interpreter.
 *
 * \returns  A DecodeResult structure containing all the information gathered
 *           during decoding.
 */

// Stub from PPC...
DecodeResult &
MIPSDecoder::decodeInstruction(ADDRESS pc, ptrdiff_t delta)
{
	static DecodeResult result;
	ADDRESS hostPC = pc + delta;

	// Clear the result structure;
	result.reset();

	// The actual list of instantiated statements
	std::list<Statement *> *stmts = NULL;

	ADDRESS nextPC = NO_ADDRESS;
}


