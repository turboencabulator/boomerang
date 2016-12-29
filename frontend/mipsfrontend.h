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

#ifndef MIPSFRONTEND_H
#define MIPSFRONTEND_H

// Class MIPSFrontEnd: derived from FrontEnd, with source machine specific
// behaviour

#include "frontend.h"       // In case included bare, e.g. ProcTest.cpp

class MIPSFrontEnd : public FrontEnd {
public:
	MIPSFrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~MIPSFrontEnd();

	virtual platform getFrontEndId() { return PLAT_MIPS; }

	virtual bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false);

	virtual std::vector<Exp *> &getDefaultParams();
	virtual std::vector<Exp *> &getDefaultReturns();

	virtual ADDRESS getMainEntryPoint(bool &gotMain);
};

#endif
