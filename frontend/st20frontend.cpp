/**
 * \file
 * \brief Contains routines to manage the decoding of ST20 instructions and
 *        the instantiation to RTLs.
 *
 * These functions replace frontend.cpp for decoding ST20 instructions.
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

#include "st20frontend.h"

#include "exp.h"
#include "proc.h"

ST20FrontEnd::ST20FrontEnd(BinaryFile *pBF, Prog *prog) :
	FrontEnd(pBF, prog),
	decoder(prog)
{
}

ST20FrontEnd::~ST20FrontEnd()
{
}

std::vector<Exp *> &ST20FrontEnd::getDefaultParams()
{
	static std::vector<Exp *> params;
	if (params.size() == 0) {
#if 0
		for (int r = 0; r <= 2; r++) {
			params.push_back(Location::regOf(r));
		}
#endif
		params.push_back(Location::memOf(Location::regOf(3)));
	}
	return params;
}

std::vector<Exp *> &ST20FrontEnd::getDefaultReturns()
{
	static std::vector<Exp *> returns;
	if (returns.size() == 0) {
		returns.push_back(Location::regOf(0));
		returns.push_back(Location::regOf(3));
		//returns.push_back(new Terminal(opPC));
	}
	return returns;
}

ADDRESS ST20FrontEnd::getMainEntryPoint(bool &gotMain)
{
	gotMain = true;
	ADDRESS start = pBF->getMainEntryPoint();
	if (start != NO_ADDRESS) return start;

	start = pBF->getEntryPoint();
	gotMain = false;
	if (start == NO_ADDRESS) return NO_ADDRESS;

	gotMain = true;
	return start;
}

bool ST20FrontEnd::processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag /* = false */, bool spec /* = false */)
{
	// Call the base class to do most of the work
	if (!FrontEnd::processProc(uAddr, pProc, os, frag, spec))
		return false;
	// This will get done twice; no harm
	pProc->setEntryBB();

	return true;
}

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new FrontEnd
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" FrontEnd *construct(BinaryFile *bf, Prog *prog)
{
	return new ST20FrontEnd(bf, prog);
}
extern "C" void destruct(FrontEnd *fe)
{
	delete fe;
}
#endif
