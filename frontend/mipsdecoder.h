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

	DecodeResult &decodeInstruction(ADDRESS, const BinaryFile *) override;
	//int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;
};

#endif
