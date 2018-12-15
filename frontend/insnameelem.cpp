/**
 * \file
 * \brief An element of an instruction name - contains definition of class
 *        InsNameElem.
 *
 * \authors
 * Copyright (C) 2001, The University of Queensland
 * \authors
 * Copyright (C) 2002, Trent Waddington
 * \authors
 * Copyright (C) 2016, Kyle Guinn
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "insnameelem.h"

InsNameElem::InsNameElem(const std::string &name) :
	elemname(name)
{
}

InsNameElem::~InsNameElem()
{
	delete nextelem;
}

int
InsNameElem::ntokens() const
{
	return 1;
}

std::string
InsNameElem::getinstruction() const
{
	std::string s = elemname;
	if (nextelem)
		s += nextelem->getinstruction();
	return s;
}

std::string
InsNameElem::getinspattern() const
{
	std::string s = elemname;
	if (nextelem)
		s += nextelem->getinspattern();
	return s;
}

void
InsNameElem::getrefmap(std::map<std::string, const InsNameElem *> &m) const
{
	if (nextelem)
		nextelem->getrefmap(m);
	else
		m.clear();
}

int
InsNameElem::ninstructions() const
{
	int n = ntokens();
	if (nextelem)
		n *= nextelem->ninstructions();
	return n;
}

void
InsNameElem::append(InsNameElem *next)
{
	if (nextelem)
		nextelem->append(next);
	else
		nextelem = next;
}

bool
InsNameElem::increment()
{
	if (!nextelem || !nextelem->increment())
		++value;
	if (value >= ntokens()) {
		value = 0;
		return false;
	}
	return true;
}

void
InsNameElem::reset()
{
	value = 0;
	if (nextelem)
		nextelem->reset();
}

int
InsNameElem::getvalue() const
{
	return value;
}

InsOptionElem::InsOptionElem(const std::string &name) :
	InsNameElem(name)
{
}

int
InsOptionElem::ntokens() const
{
	return 2;
}

std::string
InsOptionElem::getinstruction() const
{
	std::string s;
	if (getvalue() == 0)
		s = elemname;
	if (nextelem)
		s += nextelem->getinstruction();
	return s;
}

std::string
InsOptionElem::getinspattern() const
{
	std::string s = '\'' + elemname + '\'';
	if (nextelem)
		s += nextelem->getinspattern();
	return s;
}

InsListElem::InsListElem(const std::string &name, const std::deque<std::string> *t, const std::string &idx) :
	InsNameElem(name),
	indexname(idx),
	thetable(t)
{
}

int
InsListElem::ntokens() const
{
	return thetable->size();
}

std::string
InsListElem::getinstruction() const
{
	std::string s = (*thetable)[getvalue()];
	if (nextelem)
		s += nextelem->getinstruction();
	return s;
}

std::string
InsListElem::getinspattern() const
{
	std::string s = elemname + '[' + indexname + ']';
	if (nextelem)
		s += nextelem->getinspattern();
	return s;
}

void
InsListElem::getrefmap(std::map<std::string, const InsNameElem *> &m) const
{
	if (nextelem)
		nextelem->getrefmap(m);
	else
		m.clear();
	m[indexname] = this;
	// of course, we're assuming that we've already checked (try in the parser)
	// that indexname hasn't been used more than once on this line ..
}

std::string
InsListElem::getindex() const
{
	return indexname;
}
