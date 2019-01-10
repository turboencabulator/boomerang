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
#include "proc.h"
#include "rtl.h"
#include "statement.h"
#include "type.h"

#include <sstream>

Log &
Log::operator <<(const UserProc &up)
{
	std::ostringstream st;
	st << up;
	return *this << st.str();
}

Log &
Log::operator <<(const Statement &s)
{
	std::ostringstream st;
	st << s;
	return *this << st.str();
}

Log &
Log::operator <<(const Exp &e)
{
	std::ostringstream st;
	st << e;
	return *this << st.str();
}

Log &
Log::operator <<(const Type *ty)
{
	std::ostringstream st;
	st << ty;
	return *this << st.str();
}

Log &
Log::operator <<(const Type &ty)
{
	std::ostringstream st;
	st << ty;
	return *this << st.str();
}

Log &
Log::operator <<(const Range &r)
{
	std::ostringstream st;
	st << r;
	return *this << st.str();
}

Log &
Log::operator <<(const RangeMap &r)
{
	std::ostringstream st;
	st << r;
	return *this << st.str();
}

Log &
Log::operator <<(const RTL &r)
{
	std::ostringstream st;
	st << r;
	return *this << st.str();
}

Log &
Log::operator <<(const LocationSet &l)
{
	std::ostringstream st;
	st << l;
	return *this << st.str();
}

Log &
Log::operator <<(int i)
{
	std::ostringstream st;
	st << std::dec << i;
	return *this << st.str();
}

Log &
Log::operator <<(char c)
{
	std::ostringstream st;
	st << c;
	return *this << st.str();
}

Log &
Log::operator <<(double d)
{
	std::ostringstream st;
	st << d;
	return *this << st.str();
}

Log &
Log::operator <<(ADDRESS a)
{
	std::ostringstream st;
	st << "0x" << std::hex << a;
	return *this << st.str();
}

#if 0  // Mac OS/X and 64 bit machines possibly need this, but better to just cast the size_t to unsigned
Log &
Log::operator <<(size_t s)
{
	std::ostringstream st;
	st << s;
	return *this << st.str();
}
#endif
