/**
 * \file
 * \brief Contains the implementation of the class SymTab.
 *
 * \authors
 * Copyright (C) 2005, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "SymTab.h"

SymTab::SymTab()
{
}

SymTab::~SymTab()
{
}

/**
 * \brief Add a new entry.
 */
void SymTab::Add(ADDRESS a, const char *s)
{
	amap[a] = s;
	smap[s] = a;
}

/**
 * \brief Find an entry by address; NULL if none.
 */
const char *SymTab::find(ADDRESS a)
{
	std::map<ADDRESS, std::string>::iterator ff;
	ff = amap.find(a);
	if (ff == amap.end())
		return NULL;
	return ff->second.c_str();
}

/**
 * \brief Find an entry by name; NO_ADDRESS if none.
 */
ADDRESS SymTab::find(const char *s)
{
	std::map<std::string, ADDRESS>::iterator ff;
	ff = smap.find(s);
	if (ff == smap.end())
		return NO_ADDRESS;
	return ff->second;
}
