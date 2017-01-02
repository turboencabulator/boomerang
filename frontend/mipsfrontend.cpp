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

MIPSFrontEnd::MIPSFrontEnd(BinaryFile *pBF, Prog *prog) :
	FrontEnd(pBF, prog),
	decoder(prog)
{
}

MIPSFrontEnd::~MIPSFrontEnd()
{
}

std::vector<Exp *> &MIPSFrontEnd::getDefaultParams()
{
	static std::vector<Exp *> params;
	if (params.size() == 0) {
		for (int r = 31; r >= 0; r--) {
			params.push_back(Location::regOf(r));
		}
	}
	return params;
}

std::vector<Exp *> &MIPSFrontEnd::getDefaultReturns()
{
	static std::vector<Exp *> returns;
	if (returns.size() == 0) {
		for (int r = 31; r >= 0; r--) {
			returns.push_back(Location::regOf(r));
		}
	}
	return returns;
}

ADDRESS MIPSFrontEnd::getMainEntryPoint(bool &gotMain)
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

bool MIPSFrontEnd::processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag /* = false */, bool spec /* = false */)
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
	return new MIPSFrontEnd(bf, prog);
}
extern "C" void destruct(FrontEnd *fe)
{
	delete (MIPSFrontEnd *)fe;
}
#endif
