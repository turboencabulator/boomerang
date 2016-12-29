/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef LOG_H
#define LOG_H

#include "types.h"

#include <fstream>

class Statement;
class Exp;
class LocationSet;
class RTL;
class Range;
class RangeMap;
class Type;

class Log {
public:
	Log() { }
	virtual ~Log() { }

	virtual Log &operator<<(const char *str) = 0;
	virtual Log &operator<<(Statement *s);
	virtual Log &operator<<(Exp *e);
	virtual Log &operator<<(Type *ty);
	virtual Log &operator<<(RTL *r);
	virtual Log &operator<<(Range *r);
	virtual Log &operator<<(Range &r);
	virtual Log &operator<<(RangeMap &r);
	virtual Log &operator<<(int i);
	virtual Log &operator<<(char c);
	virtual Log &operator<<(double d);
	virtual Log &operator<<(ADDRESS a);
	virtual Log &operator<<(LocationSet *l);
	Log &operator<<(std::string &s) {
		return operator<<(s.c_str());
	}
	virtual void tail();
};

class FileLogger : public Log {
protected:
	std::ofstream out;
public:
	FileLogger();  // Implemented in boomerang.cpp
	virtual ~FileLogger() { }

	void tail();
	virtual Log &operator<<(const char *str) {
		out << str << std::flush;
		return *this;
	}
};

#endif
