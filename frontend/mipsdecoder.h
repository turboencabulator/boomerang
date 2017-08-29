/**
 * \file
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

/**
 * \brief Instruction decoder for MIPS.
 */
class MIPSDecoder : public NJMCDecoder {
public:
	MIPSDecoder(Prog *prog);

	DecodeResult &decodeInstruction(ADDRESS pc, ptrdiff_t delta) override;
	int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

private:
#if 0
	RTL *createBranchRtl(ADDRESS pc, std::list<Statement *> *stmts, const char *name);
	bool isFuncPrologue(ADDRESS hostPC);
	DWord getDword(ADDRESS lc);
#endif
};

#endif
