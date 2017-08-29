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
	//int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

private:
	/**
	 * \name Functions to decode instruction operands into Exp*s
	 * \{
	 */
	Exp *dis_Reg(unsigned r);  // XXX: Signedness difference with base class ???
	Exp *dis_RAmbz(unsigned r);  // Special for rA of certain instructions
	/** \} */

	//bool isFuncPrologue(ADDRESS hostPC);
	DWord getDword(ADDRESS lc);
};

#endif
