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

#ifndef SPARCDECODER_H
#define SPARCDECODER_H

#include "decoder.h"

#include <list>

/**
 * \brief Instruction decoder for SPARC.
 */
class SparcDecoder : public NJMCDecoder {
public:
	SparcDecoder(Prog *prog);

	virtual DecodeResult &decodeInstruction(ADDRESS pc, int delta);
	virtual int decodeAssemblyInstruction(ADDRESS pc, int delta);

	/*
	 * Indicates whether the instruction at the given address is a restore instruction.
	 */
	bool isRestore(ADDRESS hostPC);

private:
	/*
	 * Various functions to decode the operands of an instruction into
	 * a SemStr representation.
	 */
	Exp *dis_Eaddr(ADDRESS pc, int size = 0);
	Exp *dis_RegImm(ADDRESS pc);
	Exp *dis_RegLhs(unsigned r);
	Exp *dis_RegRhs(unsigned r);

	void unused(int x);
	RTL *createBranchRtl(ADDRESS pc, std::list<Statement *> *stmts, const char *name);
	bool isFuncPrologue(ADDRESS hostPC);
	DWord getDword(ADDRESS lc);
};

#endif
