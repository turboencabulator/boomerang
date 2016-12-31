/**
 * \file
 * \brief The implementation of the instruction decoder for Pentium.
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

#ifndef PENTIUMDECODER_H
#define PENTIUMDECODER_H

#include "decoder.h"

class PentiumDecoder : public NJMCDecoder {
public:
	PentiumDecoder(Prog *prog);

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
	 * Various functions to decode the operands of an instruction into
	 * a SemStr representation.
	 */
	Exp *dis_Eaddr(ADDRESS pc, int size = 0);
	Exp *dis_Mem(ADDRESS ps);
	Exp *addReloc(Exp *e);

	void unused(int x);
	bool isFuncPrologue(ADDRESS hostPC);

	Byte getByte(unsigned lc);
	SWord getWord(unsigned lc);
	DWord getDword(unsigned lc);

	unsigned lastDwordLc;
};

#endif
