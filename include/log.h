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
#include <string>

class Exp;
class LocationSet;
class RTL;
class Range;
class RangeMap;
class Statement;
class Type;

class Log {
public:
	Log() { }
	virtual ~Log() { }

	virtual Log &operator <<(const char *str) = 0;
	virtual Log &operator <<(Statement *s);
	virtual Log &operator <<(Exp *e);
	virtual Log &operator <<(Type *ty);
	virtual Log &operator <<(RTL *r);
	virtual Log &operator <<(const Range &r);
	virtual Log &operator <<(const RangeMap &r);
	virtual Log &operator <<(int i);
	virtual Log &operator <<(char c);
	virtual Log &operator <<(double d);
	virtual Log &operator <<(ADDRESS a);
	virtual Log &operator <<(const LocationSet &l);
	Log &operator <<(const std::string &s) {
		return *this << s.c_str();
	}
};

class FileLogger : public Log {
protected:
	std::ofstream out;
public:
	FileLogger();  // Implemented in boomerang.cpp
	virtual ~FileLogger() { }

	Log &operator <<(const char *str) override {
		out << str << std::flush;
		return *this;
	}
};

#endif
