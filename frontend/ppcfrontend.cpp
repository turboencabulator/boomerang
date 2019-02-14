/**
 * \file
 * \brief Contains routines to manage the decoding of PPC instructions and the
 *        instantiation to RTLs.
 *
 * These functions replace frontend.cpp for decoding PPC instructions.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2000-2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ppcfrontend.h"

#include "exp.h"
#include "proc.h"

PPCFrontEnd::PPCFrontEnd(BinaryFile *bf, Prog *prog) :
	FrontEnd(bf, prog),
	decoder(prog)
{
}

#if 0 // Cruft?
std::vector<Exp *> &
PPCFrontEnd::getDefaultParams()
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
PPCFrontEnd::getDefaultReturns()
{
	static std::vector<Exp *> returns;
	if (returns.empty()) {
		for (int r = 31; r >= 0; --r) {
			returns.push_back(Location::regOf(r));
		}
	}
	return returns;
}
#endif

bool
PPCFrontEnd::processProc(ADDRESS addr, UserProc *proc, bool frag, bool spec)
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
	return new PPCFrontEnd(bf, prog);
}
extern "C" void
destruct(FrontEnd *fe)
{
	delete (PPCFrontEnd *)fe;
}
#endif
