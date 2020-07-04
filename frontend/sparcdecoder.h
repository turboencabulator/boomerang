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

/**
 * \brief Instruction decoder for SPARC.
 */
class SparcDecoder : public NJMCDecoder {
public:
	SparcDecoder(Prog *prog);

	DecodeResult &decodeInstruction(ADDRESS, const BinaryFile *) override;
	//int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

	/*
	 * Indicates whether the instruction at the given address is a restore instruction.
	 */
	static bool isRestore(ADDRESS, const BinaryFile *);

private:
	/**
	 * \name Functions to decode instruction operands into Exp*s
	 * \{
	 */
	static Exp *dis_Eaddr(ADDRESS, const BinaryFile *);
	static Exp *dis_RegImm(ADDRESS, const BinaryFile *);
	static Exp *dis_RegLhs(unsigned r);
	static Exp *dis_RegRhs(unsigned r);
	/** \} */

	static Statement *createBranch(ADDRESS, ADDRESS, const BinaryFile *);
	//bool isFuncPrologue(ADDRESS hostPC);
};

#endif
