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

#ifndef MIPSDECODER_H
#define MIPSDECODER_H

#include "decoder.h"

class MIPSDecoder : public NJMCDecoder {
public:
	MIPSDecoder(Prog *prog);

	/*
	 * Decodes the machine instruction at pc and returns an RTL instance for
	 * the instruction.
	 */
	virtual DecodeResult &decodeInstruction(ADDRESS pc, int delta);

	/*
	 * Disassembles the machine instruction at pc and returns the number of
	 * bytes disassembled. Assembler output goes to global _assembly
	 */
	virtual int decodeAssemblyInstruction(ADDRESS pc, int delta);

private:
	/*
	 * Various functions to decode the operands of an instruction into an Exp* representation.
	 */
#if 0
	Exp *dis_Eaddr(ADDRESS pc, int size = 0);
	Exp *dis_RegImm(ADDRESS pc);
	Exp *dis_Reg(unsigned r);
	Exp *dis_RAmbz(unsigned r);  // Special for rA of certain instructions
#endif
	void unused(int x);
#if 0
	RTL *createBranchRtl(ADDRESS pc, std::list<Statement *> *stmts, const char *name);
	bool isFuncPrologue(ADDRESS hostPC);
	DWord getDword(ADDRESS lc);
#endif
};

#endif
