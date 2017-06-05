/**
 * \file
 * \brief Contains the definition of the class SymTab.
 *
 * \authors
 * Copyright (C) 2005, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef SYMTAB_H
#define SYMTAB_H

#include "types.h"

#include <map>
#include <string>

/**
 * A simple class to implement a symbol table that can be looked up by address
 * or by name.
 *
 * \note Can't readily use operator [] overloaded for address and string
 * parameters.  The main problem is that when you do symtab[0x100] = "main",
 * the string map doesn't see the string.  If you have one of the maps be a
 * pointer to the other string and use a special comparison operator, then if
 * the strings are ever changed, then the map's internal rb-tree becomes
 * invalid.
 */
class SymTab {
	/// The map indexed by address.
	std::map<ADDRESS, std::string> amap;
	/// The map indexed by string. Note that the strings are stored twice.
	std::map<std::string, ADDRESS> smap;

public:
	            SymTab();
	           ~SymTab();

	void        Add(ADDRESS a, const char *s);
	const char *find(ADDRESS a);
	ADDRESS     find(const char *s);
#if 0
	char       *FindAfter(ADDRESS &dwAddr);     // Find entry with >= given value
	char       *FindNext(ADDRESS &dwAddr);      // Find next entry (after a Find())
	int         FindIndex(ADDRESS dwAddr);      // Find index for entry
	ADDRESS     FindSym(char *pName);           // Linear search for addr from name
#endif
	std::map<ADDRESS, std::string> &getAll() { return amap; }
};

#endif
