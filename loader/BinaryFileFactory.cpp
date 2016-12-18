/**
 * \file
 * \brief Contains the implementation of the factory function
 *        BinaryFile::open(), and also BinaryFile::close().
 *
 * \authors
 * Copyright (C) 2014-2016, Kyle Guinn
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BinaryFile.h"

#include <dlfcn.h>

#include <cstdio>
#include <cstring>
#include <cerrno>

#define TESTMAGIC2(buf, off, a, b) \
   ( buf[off]   == a \
  && buf[off+1] == b )

#define TESTMAGIC4(buf, off, a, b, c, d) \
   ( buf[off]   == a \
  && buf[off+1] == b \
  && buf[off+2] == c \
  && buf[off+3] == d )

#define LIBPREFIX       ""
#define LIBSUFFIX       ".so"

#define ELFBINFILE      LIBPREFIX    "ElfBinaryFile" LIBSUFFIX
#define WIN32BINFILE    LIBPREFIX  "Win32BinaryFile" LIBSUFFIX
#define DOS4GWBINFILE   LIBPREFIX "DOS4GWBinaryFile" LIBSUFFIX
#define EXEBINFILE      LIBPREFIX    "ExeBinaryFile" LIBSUFFIX
#define PALMBINFILE     LIBPREFIX   "PalmBinaryFile" LIBSUFFIX
#define MACHOBINFILE    LIBPREFIX  "MachOBinaryFile" LIBSUFFIX
#define HPSOMBINFILE    LIBPREFIX  "HpSomBinaryFile" LIBSUFFIX
#define INTELCOFFFILE   LIBPREFIX    "IntelCoffFile" LIBSUFFIX

// Detect the file type and return the library name
static const char *detect_libname(FILE *f)
{
	unsigned char buf[64];

	fread(buf, sizeof buf, 1, f);
	if (TESTMAGIC4(buf, 0, '\177', 'E', 'L', 'F')) {
		/* ELF Binary */
		return ELFBINFILE;
	} else if (TESTMAGIC2(buf, 0, 'M', 'Z')) {
		/* DOS-based file */
		int peoff = LMMH(buf[0x3c]);
		if (peoff != 0 && fseek(f, peoff, SEEK_SET) == 0) {
			fread(buf, 4, 1, f);
			if (TESTMAGIC4(buf, 0, 'P', 'E', 0, 0)) {
				/* Win32 Binary */
				return WIN32BINFILE;
			} else if (TESTMAGIC2(buf, 0, 'N', 'E')) {
				/* Win16 / Old OS/2 Binary */
			} else if (TESTMAGIC2(buf, 0, 'L', 'E')) {
				/* Win32 VxD (Linear Executable) or DOS4GW app */
				return DOS4GWBINFILE;
			} else if (TESTMAGIC2(buf, 0, 'L', 'X')) {
				/* New OS/2 Binary */
			}
		}
		/* Assume MS-DOS Real-mode binary. */
		return EXEBINFILE;
	} else if (TESTMAGIC4(buf, 0x3c, 'a', 'p', 'p', 'l')
	        || TESTMAGIC4(buf, 0x3c, 'p', 'a', 'n', 'l')) {
		/* PRC Palm-pilot binary */
		return PALMBINFILE;
	} else if (TESTMAGIC4(buf, 0, 0xfe, 0xed, 0xfa, 0xce)
	        || TESTMAGIC4(buf, 0, 0xce, 0xfa, 0xed, 0xfe)) {
		/* Mach-O Mac OS-X binary */
		return MACHOBINFILE;
	} else if (buf[0] == 0x02
	        && buf[2] == 0x01
	        && (buf[1] == 0x10 || buf[1] == 0x0b)
	        && (buf[3] == 0x07 || buf[3] == 0x08 || buf[4] == 0x0b)) {
		/* HP Som binary (last as it's not really particularly good magic) */
		return HPSOMBINFILE;
	} else if (TESTMAGIC2(buf, 0, 0x4c, 0x01)) {
		return INTELCOFFFILE;
	}
	return NULL;
}

static const char *get_libname(const char *name)
{
	FILE *f = fopen(name, "rb");
	if (f == NULL) {
		fprintf(stderr, "%s: fopen: %s\n", name, strerror(errno));
		return NULL;
	}

	const char *libname = detect_libname(f);
	if (libname == NULL) {
		fprintf(stderr, "%s: unrecognised binary file\n", name);
	}

	fclose(f);
	return libname;
}

/**
 * This function determines the type of a binary and loads the appropriate
 * loader class dynamically.
 */
BinaryFile *BinaryFile::open(const char *name)
{
	const char *libname = get_libname(name);
	if (libname == NULL) {
		return NULL;
	}

	// Load the specific loader library
	void *handle = dlopen(libname, RTLD_LAZY);
	if (handle == NULL) {
		fprintf(stderr, "cannot load library: %s\n", dlerror());
		return NULL;
	}

	// Reset errors
	const char *error = dlerror();

	// Use the handle to find symbols
	const char *symbol = "construct";
	constructFcn construct = (constructFcn)dlsym(handle, symbol);
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "cannot load symbol '%s': %s\n", symbol, error);
		dlclose(handle);
		return NULL;
	}
	symbol = "destruct";
	destructFcn destruct = (destructFcn)dlsym(handle, symbol);
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "cannot load symbol '%s': %s\n", symbol, error);
		dlclose(handle);
		return NULL;
	}

	// Call the construct function
	BinaryFile *bf = construct();

	// Stash pointers in the constructed object, for use by BinaryFile::close
	bf->dlHandle = handle;
	bf->destruct = destruct;

	if (!bf->RealLoad(name)) {
		fprintf(stderr, "Loading '%s' failed\n", name);
		BinaryFile::close(bf); bf = NULL;
		return NULL;
	}

	bf->getTextLimits();
	return bf;
}

void BinaryFile::close(BinaryFile *bf)
{
	// Retrieve the stashed pointers
	void *handle = bf->dlHandle;
	destructFcn destruct = bf->destruct;

	// Destruct in an appropriate way.
	// The C++ dlopen mini HOWTO says to always use a matching
	// construct/destruct pair in case of new/delete overloading.
	if (handle != NULL) {
		destruct(bf);
		dlclose(handle);
	} else
		delete bf;
}
