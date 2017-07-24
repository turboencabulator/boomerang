/**
 * \file
 *
 * \authors
 * Copyright (C) 1996-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef PPCDECODER_H
#define PPCDECODER_H

#include "decoder.h"

#include <list>

/**
 * \brief Instruction decoder for PPC.
 */
class PPCDecoder : public NJMCDecoder {
public:
	PPCDecoder(Prog *prog);

	DecodeResult &decodeInstruction(ADDRESS pc, ptrdiff_t delta) override;
	int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

private:
	/*
	 * Various functions to decode the operands of an instruction into an Exp* representation.
	 */
	Exp *dis_Eaddr(ADDRESS pc, int size = 0);
	Exp *dis_RegImm(ADDRESS pc);
	Exp *dis_Reg(unsigned r);
	Exp *dis_RAmbz(unsigned r);  // Special for rA of certain instructions

	RTL *createBranchRtl(ADDRESS pc, std::list<Statement *> *stmts, const char *name);
	bool isFuncPrologue(ADDRESS hostPC);
	DWord getDword(ADDRESS lc);
};

#endif
