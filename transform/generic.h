/**
 * \file
 * \brief Provides the definition for the generic exp tranformer.
 *
 * \authors
 * Copyright (C) 2004, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef GENERIC_H
#define GENERIC_H

#include "transformer.h"

class Exp;

class GenericExpTransformer : public ExpTransformer
{
protected:
	Exp *match, *where, *become;

	bool checkCond(Exp *cond, Exp *bindings);
	Exp *applyFuncs(Exp *rhs);
public:
	GenericExpTransformer(Exp *match, Exp *where, Exp *become) : match(match), where(where), become(become) { }
	virtual Exp *applyTo(Exp *e, bool &bMod);
};

#endif
