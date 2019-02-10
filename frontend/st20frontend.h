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
	ST20FrontEnd(BinaryFile *, Prog *);

	platform getFrontEndId() const override { return PLAT_ST20; }
	NJMCDecoder &getDecoder() override { return decoder; }

	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

	bool processProc(ADDRESS, UserProc *, bool = false, bool = false) override;
};

#endif
