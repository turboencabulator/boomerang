/**
 * \file
 * \brief Implementation of the RDIExpTransformer and related classes.
 *
 * \authors
 * Copyright (C) 2004, Mike Van Emmerik and Trent Waddington
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exp.h"
#include "transformer.h"
#include "rdi.h"

Exp *RDIExpTransformer::applyTo(Exp *e, bool &bMod)
{
	if (e->getOper() == opAddrOf && e->getSubExp1()->getOper() == opMemOf) {
		e = e->getSubExp1()->getSubExp1()->clone();
		bMod = true;
	}
	return e;
}
