/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef ST20FRONTEND_H
#define ST20FRONTEND_H

// Class ST20FrontEnd: derived from FrontEnd, with source machine specific
// behaviour

#include "frontend.h"       // In case included bare, e.g. ProcTest.cpp

class ST20FrontEnd : public FrontEnd {
public:
	ST20FrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~ST20FrontEnd();

	virtual platform getFrontEndId() { return PLAT_ST20; }

	virtual bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false);

	virtual std::vector<Exp *> &getDefaultParams();
	virtual std::vector<Exp *> &getDefaultReturns();

	virtual ADDRESS getMainEntryPoint(bool &gotMain);
};

#endif
