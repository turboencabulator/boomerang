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

#ifndef PENTIUMDECODER_H
#define PENTIUMDECODER_H

#include "decoder.h"

/**
 * \brief Instruction decoder for Pentium.
 */
class PentiumDecoder : public NJMCDecoder {
public:
	PentiumDecoder(Prog *prog);

	virtual DecodeResult &decodeInstruction(ADDRESS pc, int delta);
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
