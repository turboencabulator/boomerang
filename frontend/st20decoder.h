/**
 * \file
 *
 * \authors
 * Copyright (C) 2005, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef ST20DECODER_H
#define ST20DECODER_H

#include "decoder.h"

#include <list>

/**
 * \brief Instruction decoder for ST20.
 */
class ST20Decoder : public NJMCDecoder {
public:
	ST20Decoder(Prog *prog);

	DecodeResult &decodeInstruction(ADDRESS pc, ptrdiff_t delta) override;
	int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

private:
	/*
	 * Various functions to decode the operands of an instruction into
	 * a SemStr representation.
	 */
#if 0
	Exp *dis_Eaddr(ADDRESS pc, int size = 0);
	Exp *dis_RegImm(ADDRESS pc);
	Exp *dis_Reg(unsigned r);
	Exp *dis_RAmbz(unsigned r);  // Special for rA of certain instructions
#endif

	RTL *createBranchRtl(ADDRESS pc, std::list<Statement *> *stmts, const char *name);
	bool isFuncPrologue(ADDRESS hostPC);
	DWord getDword(ADDRESS lc);
	SWord getWord(ADDRESS lc);
	Byte getByte(ADDRESS lc);
};

#endif
