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

#include "frontend.h"

/**
 * \brief MIPS specific FrontEnd behaviour.
 */
class MIPSFrontEnd : public FrontEnd {
public:
	MIPSFrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~MIPSFrontEnd();

	virtual platform getFrontEndId() { return PLAT_MIPS; }

	virtual std::vector<Exp *> &getDefaultParams();
	virtual std::vector<Exp *> &getDefaultReturns();

	virtual bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false);

	virtual ADDRESS getMainEntryPoint(bool &gotMain);
};

#endif
