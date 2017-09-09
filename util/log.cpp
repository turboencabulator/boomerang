/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "log.h"

#include "exp.h"
#include "managed.h"
#include "rtl.h"
#include "statement.h"

#include <sstream>

Log &
Log::operator <<(Statement *s)
{
	std::ostringstream st;
	s->print(st);
	return *this << st.str().c_str();
}

Log &
Log::operator <<(Exp *e)
{
	std::ostringstream st;
	e->print(st);
	return *this << st.str().c_str();
}

Log &
Log::operator <<(Type *ty)
{
	std::ostringstream st;
	st << ty;
	return *this << st.str().c_str();
}

Log &
Log::operator <<(const Range &r)
{
	std::ostringstream st;
	st << r;
	return *this << st.str().c_str();
}

Log &
Log::operator <<(const RangeMap &r)
{
	std::ostringstream st;
	st << r;
	return *this << st.str().c_str();
}

Log &
Log::operator <<(RTL *r)
{
	std::ostringstream st;
	r->print(st);
	return *this << st.str().c_str();
}

Log &
Log::operator <<(const LocationSet &l)
{
	std::ostringstream st;
	st << l;
	return *this << st.str().c_str();
}

Log &
Log::operator <<(int i)
{
	std::ostringstream st;
	st << std::dec << i;
	return *this << st.str().c_str();
}

Log &
Log::operator <<(char c)
{
	std::ostringstream st;
	st << c;
	return *this << st.str().c_str();
}

Log &
Log::operator <<(double d)
{
	std::ostringstream st;
	st << d;
	return *this << st.str().c_str();
}

Log &
Log::operator <<(ADDRESS a)
{
	std::ostringstream st;
	st << "0x" << std::hex << a;
	return *this << st.str().c_str();
}

#if 0  // Mac OS/X and 64 bit machines possibly need this, but better to just cast the size_t to unsigned
Log &
Log::operator <<(size_t s)
{
	std::ostringstream st;
	st << st;
	return *this << st.str().c_str();
}
#endif
