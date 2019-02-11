/**
 * \file
 * \brief Important initialisation that has to happen at the start of main().
 *
 * Also contains main(), so it can be the only file different between
 * boomerang and bigtest.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002-2006, Mike Van Emmerik and Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "boomerang.h"

#ifdef GARBAGE_COLLECTOR
//#define GC_DEBUG 1  // Uncomment to debug the garbage collector
#include <gc/gc.h>

// Prototypes for various initialisation functions for garbage collection safety
void init_dfa();
void init_basicblock();
#endif

#ifndef NO_CMDLINE_MAIN
int
main(int argc, const char *argv[])
{
#ifdef GARBAGE_COLLECTOR
	// Call the various initialisation functions for safe garbage collection
	init_dfa();
	init_basicblock();
#endif

	return Boomerang::get().commandLine(argc, argv);
}
#endif

#ifdef GARBAGE_COLLECTOR
#ifndef NO_NEW_OR_DELETE_OPERATORS
/* This makes sure that the garbage collector sees all allocations, even those
    that we can't be bothered collecting, especially standard STL objects */
void *
operator new(size_t n)
{
#ifdef DONT_COLLECT_STL
	return GC_malloc_uncollectable(n);  // Don't collect, but mark
#else
	return GC_malloc(n);                // Collect everything
#endif
}

void
operator delete(void *p)
{
#ifdef DONT_COLLECT_STL
	GC_free(p); // Important to call this if you call GC_malloc_uncollectable
	// #else do nothing!
#endif
}
#endif
#endif
