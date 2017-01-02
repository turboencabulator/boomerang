/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef ST20FRONTEND_H
#define ST20FRONTEND_H

#include "frontend.h"

#include "st20decoder.h"

/**
 * \brief ST20 specific FrontEnd behaviour.
 */
class ST20FrontEnd : public FrontEnd {
	ST20Decoder decoder;

public:
	ST20FrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~ST20FrontEnd();

	virtual platform getFrontEndId() { return PLAT_ST20; }
	virtual NJMCDecoder &getDecoder() { return decoder; }

	virtual std::vector<Exp *> &getDefaultParams();
	virtual std::vector<Exp *> &getDefaultReturns();

	virtual bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false);

	virtual ADDRESS getMainEntryPoint(bool &gotMain);
};

#endif
