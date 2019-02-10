/**
 * \file
 * \brief Skeleton for MIPS disassembly.
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

#include "mipsfrontend.h"

#include "exp.h"
#include "proc.h"

MIPSFrontEnd::MIPSFrontEnd(BinaryFile *bf, Prog *prog) :
	FrontEnd(bf, prog),
	decoder(prog)
{
}

std::vector<Exp *> &
MIPSFrontEnd::getDefaultParams()
{
	static std::vector<Exp *> params;
	if (params.empty()) {
		for (int r = 31; r >= 0; --r) {
			params.push_back(Location::regOf(r));
		}
	}
	return params;
}

std::vector<Exp *> &
MIPSFrontEnd::getDefaultReturns()
{
	static std::vector<Exp *> returns;
	if (returns.empty()) {
		for (int r = 31; r >= 0; --r) {
			returns.push_back(Location::regOf(r));
		}
	}
	return returns;
}

bool
MIPSFrontEnd::processProc(ADDRESS addr, UserProc *proc, bool frag, bool spec)
{
	// Call the base class to do most of the work
	if (!FrontEnd::processProc(addr, proc, frag, spec))
		return false;
	// This will get done twice; no harm
	proc->setEntryBB();

	return true;
}

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new FrontEnd
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" FrontEnd *
construct(BinaryFile *bf, Prog *prog)
{
	return new MIPSFrontEnd(bf, prog);
}
extern "C" void
destruct(FrontEnd *fe)
{
	delete (MIPSFrontEnd *)fe;
}
#endif
