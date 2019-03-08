/**
 * \file
 * \brief Element comparison functions for expressions and statements.
 *
 * \authors
 * Copyright (C) 2003, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef EXPHELP_H
#define EXPHELP_H

#include <functional>

class Assign;
class Assignment;
class Exp;

/**
 * A class for comparing Exp*s (comparing the actual expressions).
 * Type sensitive.
 */
class lessExpStar : public std::less<Exp *> {
public:
	bool operator ()(const Exp *x, const Exp *y) const;
};

/**
 * A class for comparing Exp*s (comparing the actual expressions).
 * Type insensitive.
 */
class lessTI : public std::less<Exp *> {
public:
	bool operator ()(const Exp *x, const Exp *y) const;
};

/**
 * Compare Assignments by their left hand sides (only).
 * Implemented in statement.cpp
 */
class lessAssignment : public std::less<Assignment *> {
public:
	bool operator ()(const Assignment *x, const Assignment *y) const;
};

/**
 * Repeat the above for Assigns; sometimes the #include ordering is such that
 * the compiler doesn't know that an Assign is a subclass of Assignment.
 */
class lessAssign : public std::less<Assign *> {
public:
	bool operator ()(const Assign *x, const Assign *y) const;
};

#endif
