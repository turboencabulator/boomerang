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

/**
 * \brief Add a new entry.
 */
void
SymTab::Add(ADDRESS a, const std::string &s)
{
	amap[a] = s;
	smap[s] = a;
}

/**
 * \brief Find an entry by address; nullptr if none.
 */
const char *
SymTab::find(ADDRESS a) const
{
	auto ff = amap.find(a);
	if (ff == amap.end())
		return nullptr;
	return ff->second.c_str();
}

/**
 * \brief Find an entry by name; NO_ADDRESS if none.
 */
ADDRESS
SymTab::find(const std::string &s) const
{
	auto ff = smap.find(s);
	if (ff == smap.end())
		return NO_ADDRESS;
	return ff->second;
}
