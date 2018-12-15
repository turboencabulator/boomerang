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

	DecodeResult &decodeInstruction(ADDRESS pc, ptrdiff_t delta) override;
	//int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

private:
	/**
	 * \name Functions to decode instruction operands into Exp*s
	 * \{
	 */
	Exp *dis_Eaddr(ADDRESS pc, ptrdiff_t delta, int size = 0);
	Exp *dis_Mem(ADDRESS pc, ptrdiff_t delta);
	Exp *addReloc(Exp *e);
	/** \} */

	//bool isFuncPrologue(ADDRESS hostPC);

	static uint8_t getByte(ADDRESS lc, ptrdiff_t delta);
	static uint16_t getWord(ADDRESS lc, ptrdiff_t delta);
	uint32_t getDword(ADDRESS lc, ptrdiff_t delta);

	unsigned lastDwordLc;
};

#endif
