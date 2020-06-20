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
	NJMCDecoder &getDecoder() override { return decoder; }

	platform getFrontEndId() const override { return PLAT_ST20; }
	//std::vector<Exp *> &getDefaultParams() override;
	//std::vector<Exp *> &getDefaultReturns() override;

public:
	ST20FrontEnd(BinaryFile *, Prog *);

	bool processProc(ADDRESS, UserProc *, bool = false) override;
};

#endif
