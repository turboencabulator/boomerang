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

	virtual Log &operator <<(const char *) = 0;
	virtual Log &operator <<(const Statement &);
	virtual Log &operator <<(const Exp &);
	virtual Log &operator <<(const Type *);
	virtual Log &operator <<(const Type &);
	virtual Log &operator <<(const RTL &);
	virtual Log &operator <<(const Range &);
	virtual Log &operator <<(const RangeMap &);
	virtual Log &operator <<(int);
	virtual Log &operator <<(char);
	virtual Log &operator <<(double);
	virtual Log &operator <<(ADDRESS);
	virtual Log &operator <<(const LocationSet &);
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
