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

#include "types.h"
#include "statement.h"
#include "cfg.h"
#include "exp.h"
#include "register.h"
#include "rtl.h"
#include "proc.h"
#include "transformer.h"
#include "rdi.h"

#include <numeric>      // For accumulate
#include <algorithm>    // For std::max()
#include <map>          // In decideType()
#include <sstream>      // Need gcc 3.0 or better

Exp *RDIExpTransformer::applyTo(Exp *e, bool &bMod)
{
	if (e->getOper() == opAddrOf && e->getSubExp1()->getOper() == opMemOf) {
		e = e->getSubExp1()->getSubExp1()->clone();
		bMod = true;
	}
	return e;
}
