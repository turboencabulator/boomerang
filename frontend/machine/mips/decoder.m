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
	std::string file = Boomerang::get()->getProgPath() + "frontend/machine/mips/mips.ssl";
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
 * \param proc   The enclosing procedure.  This can be nullptr for those of us
 *               who are using this method in an interpreter.
 *
 * \returns  A DecodeResult structure containing all the information gathered
 *           during decoding.
 */

// Stub from PPC...
DecodeResult &
MIPSDecoder::decodeInstruction(ADDRESS pc, const BinaryFile *bf)
{
	static DecodeResult result;

	// Clear the result structure;
	result.reset();

	// The actual list of instantiated statements
	std::list<Statement *> *stmts = nullptr;
}
