/**
 * \file
 * \brief Provides the definition for the remove double indirection exp
 *        tranformer.
 *
 * \authors
 * Copyright (C) 2004, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef RDI_H
#define RDI_H

class RDIExpTransformer : public ExpTransformer
{
public:
	RDIExpTransformer() {}
	virtual Exp *applyTo(Exp *e, bool &bMod);
};

#endif
