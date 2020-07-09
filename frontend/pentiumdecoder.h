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
#include "operator.h"

/**
 * \brief Instruction decoder for Pentium.
 */
class PentiumDecoder : public NJMCDecoder {
public:
	PentiumDecoder(Prog *prog);

	void decodeInstruction(DecodeResult &, ADDRESS, const BinaryFile *) override;
	//int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

private:
	/**
	 * \name Functions to decode instruction operands into Exp*s
	 * \{
	 */
	Exp *dis_Eaddr(ADDRESS, const BinaryFile *, int size = 0) const;
	Exp *dis_Mem(ADDRESS, const BinaryFile *) const;
	/** \} */

	//bool isFuncPrologue(ADDRESS hostPC);

	static void genBSFR(DecodeResult &, ADDRESS, Exp *, Exp *, int, int, OPER, int);
};

#endif
