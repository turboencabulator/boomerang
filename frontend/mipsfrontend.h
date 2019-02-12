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

#include "mipsdecoder.h"

/**
 * \brief MIPS specific FrontEnd behaviour.
 */
class MIPSFrontEnd : public FrontEnd {
	MIPSDecoder decoder;
	NJMCDecoder &getDecoder() override { return decoder; }

	platform getFrontEndId() const override { return PLAT_MIPS; }
	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

public:
	MIPSFrontEnd(BinaryFile *, Prog *);

	bool processProc(ADDRESS, UserProc *, bool = false, bool = false) override;
};

#endif
