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

public:
	PPCFrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~PPCFrontEnd();

	platform getFrontEndId() const override { return PLAT_PPC; }
	NJMCDecoder &getDecoder() override { return decoder; }

	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

	bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false) override;
};

#endif
