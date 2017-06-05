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

InsNameElem::~InsNameElem(void)
{
	//delete nextelem;
}

int
InsNameElem::ntokens(void)
{
	return 1;
}

std::string
InsNameElem::getinstruction(void)
{
	return (nextelem != NULL)
	       ? (elemname + nextelem->getinstruction())
	       : elemname;
}

std::string
InsNameElem::getinspattern(void)
{
	return (nextelem != NULL)
	       ? (elemname + nextelem->getinspattern())
	       : elemname;
}

void
InsNameElem::getrefmap(std::map<std::string, InsNameElem *> &m)
{
	if (nextelem != NULL)
		nextelem->getrefmap(m);
	else
		m.clear();
}

int
InsNameElem::ninstructions(void)
{
	return (nextelem != NULL)
	       ? (nextelem->ninstructions() * ntokens())
	       : ntokens();
}

void
InsNameElem::append(InsNameElem *next)
{
	if (nextelem == NULL)
		nextelem = next;
	else
		nextelem->append(next);
}

bool
InsNameElem::increment(void)
{
	if ((nextelem == NULL) || nextelem->increment())
		value++;
	if (value >= ntokens()) {
		value = 0;
		return true;
	}
	return false;
}

void
InsNameElem::reset(void)
{
	value = 0;
	if (nextelem != NULL) nextelem->reset();
}

int
InsNameElem::getvalue(void)
{
	return value;
}

InsOptionElem::InsOptionElem(const char *name) :
	InsNameElem(name)
{
}

int
InsOptionElem::ntokens(void)
{
	return 2;
}

std::string
InsOptionElem::getinstruction(void)
{
	std::string s = (nextelem != NULL)
	                ? ((getvalue() == 0)
	                   ? (elemname + nextelem->getinstruction())
	                   : nextelem->getinstruction())
	                : ((getvalue() == 0)
	                   ? elemname
	                   : "");
	return s;
}

std::string
InsOptionElem::getinspattern(void)
{
	return (nextelem != NULL)
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
InsListElem::ntokens(void)
{
	return thetable->records.size();
}

std::string
InsListElem::getinstruction(void)
{
	return (nextelem != NULL)
	       ? (thetable->records[getvalue()] + nextelem->getinstruction())
	       : thetable->records[getvalue()];
}

std::string
InsListElem::getinspattern(void)
{
	return (nextelem != NULL)
	       ? (elemname + '[' + indexname + ']' + nextelem->getinspattern())
	       : (elemname + '[' + indexname + ']');
}

void
InsListElem::getrefmap(std::map<std::string, InsNameElem *> &m)
{
	if (nextelem != NULL)
		nextelem->getrefmap(m);
	else
		m.clear();
	m[indexname] = this;
	// of course, we're assuming that we've already checked (try in the parser)
	// that indexname hasn't been used more than once on this line ..
}

std::string
InsListElem::getindex(void)
{
	return indexname;
}
