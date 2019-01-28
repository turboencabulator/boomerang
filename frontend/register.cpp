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

/**
 * \brief Equality operator.
 *
 * \param[in] r2  Reference to another Register object.
 * \returns       true if the same.
 */
bool
Register::operator ==(const Register &r2) const
{
	// compare on name
	return name == r2.name;
}

/**
 * \brief Comparison operator (to establish an ordering).
 *
 * \param[in] r2  Reference to another Register object.
 * \returns       true if this name is less than the given Register's name.
 */
bool
Register::operator <(const Register &r2) const
{
	// compare on name
	return name < r2.name;
}

/**
 * \brief Get the type for this register.
 *
 * \returns  The type as a pointer to a Type object.
 */
Type *
Register::g_type() const
{
	if (flt)
		return new FloatType(size);
	return new IntegerType(size);
}
