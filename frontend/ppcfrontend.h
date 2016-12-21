#ifndef PPCFRONTEND_H
#define PPCFRONTEND_H

// Class PPCFrontEnd: derived from FrontEnd, with source machine specific
// behaviour

#include "frontend.h"       // In case included bare, e.g. ProcTest.cpp

class PPCFrontEnd : public FrontEnd {
public:
	PPCFrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~PPCFrontEnd();

	virtual platform getFrontEndId() { return PLAT_PPC; }

	virtual bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false);

	virtual std::vector<Exp *> &getDefaultParams();
	virtual std::vector<Exp *> &getDefaultReturns();

	virtual ADDRESS getMainEntryPoint(bool &gotMain);
};

#endif
