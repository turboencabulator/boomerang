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

#include "table.h"

InsNameElem::InsNameElem(const std::string &name) :
	elemname(name)
{
}

InsNameElem::~InsNameElem()
{
	//delete nextelem;
}

int
InsNameElem::ntokens()
{
	return 1;
}

std::string
InsNameElem::getinstruction()
{
	std::string s = elemname;
	if (nextelem)
		s += nextelem->getinstruction();
	return s;
}

std::string
InsNameElem::getinspattern()
{
	std::string s = elemname;
	if (nextelem)
		s += nextelem->getinspattern();
	return s;
}

void
InsNameElem::getrefmap(std::map<std::string, InsNameElem *> &m)
{
	if (nextelem)
		nextelem->getrefmap(m);
	else
		m.clear();
}

int
InsNameElem::ninstructions()
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
	if (!nextelem || nextelem->increment())
		++value;
	if (value >= ntokens()) {
		value = 0;
		return true;
	}
	return false;
}

void
InsNameElem::reset()
{
	value = 0;
	if (nextelem)
		nextelem->reset();
}

int
InsNameElem::getvalue()
{
	return value;
}

InsOptionElem::InsOptionElem(const std::string &name) :
	InsNameElem(name)
{
}

int
InsOptionElem::ntokens()
{
	return 2;
}

std::string
InsOptionElem::getinstruction()
{
	std::string s;
	if (getvalue() == 0)
		s = elemname;
	if (nextelem)
		s += nextelem->getinstruction();
	return s;
}

std::string
InsOptionElem::getinspattern()
{
	std::string s = '\'' + elemname + '\'';
	if (nextelem)
		s += nextelem->getinspattern();
	return s;
}

InsListElem::InsListElem(const std::string &name, Table *t, const std::string &idx) :
	InsNameElem(name),
	indexname(idx),
	thetable(t)
{
}

int
InsListElem::ntokens()
{
	return thetable->records.size();
}

std::string
InsListElem::getinstruction()
{
	std::string s = thetable->records[getvalue()];
	if (nextelem)
		s += nextelem->getinstruction();
	return s;
}

std::string
InsListElem::getinspattern()
{
	std::string s = elemname + '[' + indexname + ']';
	if (nextelem)
		s += nextelem->getinspattern();
	return s;
}

void
InsListElem::getrefmap(std::map<std::string, InsNameElem *> &m)
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
InsListElem::getindex()
{
	return indexname;
}
