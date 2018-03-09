/**
 * \file
 * \brief Register class descriptions.  Holds detailed information about a
 *        single register.
 *
 * \authors
 * Copyright (C) 1999-2000, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "register.h"

#include "type.h"

/*==============================================================================
 * FUNCTION:      Register::Register
 * OVERVIEW:      Constructor.
 *============================================================================*/
Register::Register()
{
}

/*==============================================================================
 * FUNCTION:      Register::Register
 * OVERVIEW:      Copy constructor.
 * PARAMETERS:    Reference to another Register object to construct from
 *============================================================================*/
Register::Register(const Register &r) :
	name(r.name),
	size(r.size),
	address(r.address),
	mappedIndex(r.mappedIndex),
	mappedOffset(r.mappedOffset),
	flt(r.flt)
{
}

/*==============================================================================
 * FUNCTION:      Register::operator =
 * OVERVIEW:      Copy operator
 * PARAMETERS:    Reference to another Register object (to be copied)
 * RETURNS:       This object
 *============================================================================*/
Register
Register::operator =(const Register &r2)
{
	name         = r2.name;
	size         = r2.size;
	address      = r2.address;
	mappedIndex  = r2.mappedIndex;
	mappedOffset = r2.mappedOffset;
	flt          = r2.flt;

	return (*this);
}

/*==============================================================================
 * FUNCTION:      Register::operator ==
 * OVERVIEW:      Equality operator
 * PARAMETERS:    Reference to another Register object
 * RETURNS:       True if the same
 *============================================================================*/
bool
Register::operator ==(const Register &r2) const
{
	// compare on name
	return name == r2.name;
}

/*==============================================================================
 * FUNCTION:      Register::operator <
 * OVERVIEW:      Comparison operator (to establish an ordering)
 * PARAMETERS:    Reference to another Register object
 * RETURNS:       true if this name is less than the given Register's name
 *============================================================================*/
bool
Register::operator <(const Register &r2) const
{
	// compare on name
	return name < r2.name;
}

/*==============================================================================
 * FUNCTION:      Register::g_type
 * OVERVIEW:      Get the type for this register
 * RETURNS:       The type as a pointer to a Type object
 *============================================================================*/
Type *
Register::g_type() const
{
	if (flt)
		return new FloatType(size);
	return new IntegerType(size);
}
