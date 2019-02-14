/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef PPCFRONTEND_H
#define PPCFRONTEND_H

#include "frontend.h"

#include "ppcdecoder.h"

/**
 * \brief PPC specific FrontEnd behaviour.
 */
class PPCFrontEnd : public FrontEnd {
	PPCDecoder decoder;
	NJMCDecoder &getDecoder() override { return decoder; }

	platform getFrontEndId() const override { return PLAT_PPC; }
	//std::vector<Exp *> &getDefaultParams() override;
	//std::vector<Exp *> &getDefaultReturns() override;

public:
	PPCFrontEnd(BinaryFile *, Prog *);

	bool processProc(ADDRESS, UserProc *, bool = false, bool = false) override;
};

#endif
