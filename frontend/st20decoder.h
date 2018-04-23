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
	//int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) override;

private:
	static uint8_t getByte(ADDRESS lc);
	//static uint16_t getWord(ADDRESS lc);
	//static uint32_t getDword(ADDRESS lc);
};

#endif
