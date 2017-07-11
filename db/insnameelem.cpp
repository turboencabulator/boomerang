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

InsNameElem::InsNameElem(const char *name) :
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
	return nextelem
	     ? (elemname + nextelem->getinstruction())
	     : elemname;
}

std::string
InsNameElem::getinspattern()
{
	return nextelem
	     ? (elemname + nextelem->getinspattern())
	     : elemname;
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
	return nextelem
	     ? (nextelem->ninstructions() * ntokens())
	     : ntokens();
}

void
InsNameElem::append(InsNameElem *next)
{
	if (!nextelem)
		nextelem = next;
	else
		nextelem->append(next);
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
	if (nextelem) nextelem->reset();
}

int
InsNameElem::getvalue()
{
	return value;
}

InsOptionElem::InsOptionElem(const char *name) :
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
	std::string s = nextelem
	              ? ((getvalue() == 0)
	               ? (elemname + nextelem->getinstruction())
	               : nextelem->getinstruction())
	              : ((getvalue() == 0)
	               ? elemname
	               : "");
	return s;
}

std::string
InsOptionElem::getinspattern()
{
	return nextelem
	     ? ('\'' + elemname + '\'' + nextelem->getinspattern())
	     : ('\'' + elemname + '\'');
}

InsListElem::InsListElem(const char *name, Table *t, const char *idx) :
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
	return nextelem
	     ? (thetable->records[getvalue()] + nextelem->getinstruction())
	     : thetable->records[getvalue()];
}

std::string
InsListElem::getinspattern()
{
	return nextelem
	     ? (elemname + '[' + indexname + ']' + nextelem->getinspattern())
	     : (elemname + '[' + indexname + ']');
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
