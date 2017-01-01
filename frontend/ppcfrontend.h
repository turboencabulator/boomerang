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

/**
 * \brief PPC specific FrontEnd behaviour.
 */
class PPCFrontEnd : public FrontEnd {
public:
	PPCFrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~PPCFrontEnd();

	virtual platform getFrontEndId() { return PLAT_PPC; }

	virtual std::vector<Exp *> &getDefaultParams();
	virtual std::vector<Exp *> &getDefaultReturns();

	virtual bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false);

	virtual ADDRESS getMainEntryPoint(bool &gotMain);
};

#endif
