/**
 * \file
 * \brief Provides the definition for the transformer and related classes.
 *
 * \authors
 * Copyright (C) 2004, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include <list>

class Exp;

class ExpTransformer {
protected:
	static std::list<ExpTransformer *> transformers;
public:
	ExpTransformer();
	virtual ~ExpTransformer() = default;

	static void loadAll();

	virtual Exp *applyTo(Exp *e, bool &bMod) = 0;
	static Exp *applyAllTo(Exp *e, bool &bMod);
};

#endif
