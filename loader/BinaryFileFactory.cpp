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

#ifdef DYNAMIC
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#else
#include    "ElfBinaryFile.h"
#include  "Win32BinaryFile.h"
#include   "PalmBinaryFile.h"
#include  "HpSomBinaryFile.h"
#include    "ExeBinaryFile.h"
#include  "MachOBinaryFile.h"
#include "DOS4GWBinaryFile.h"
#include    "IntelCoffFile.h"
#endif

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

/**
 * Detect the file type and return the loader format.
 *
 * \param f  Opened file to perform detection on.
 */
static LOADFMT magic(FILE *f)
{
	unsigned char buf[64];

	fread(buf, sizeof buf, 1, f);
	if (TESTMAGIC4(buf, 0, '\177', 'E', 'L', 'F')) {
		/* ELF Binary */
		return LOADFMT_ELF;
	} else if (TESTMAGIC2(buf, 0, 'M', 'Z')) {
		/* DOS-based file */
		int peoff = LMMH(buf[0x3c]);
		if (peoff != 0 && fseek(f, peoff, SEEK_SET) == 0) {
			fread(buf, 4, 1, f);
			if (TESTMAGIC4(buf, 0, 'P', 'E', 0, 0)) {
				/* Win32 Binary */
				return LOADFMT_PE;
			} else if (TESTMAGIC2(buf, 0, 'N', 'E')) {
				/* Win16 / Old OS/2 Binary */
			} else if (TESTMAGIC2(buf, 0, 'L', 'E')) {
				/* Win32 VxD (Linear Executable) or DOS4GW app */
				return LOADFMT_LX;
			} else if (TESTMAGIC2(buf, 0, 'L', 'X')) {
				/* New OS/2 Binary */
			}
		}
		/* Assume MS-DOS Real-mode binary. */
		return LOADFMT_EXE;
	} else if (TESTMAGIC4(buf, 0x3c, 'a', 'p', 'p', 'l')
	        || TESTMAGIC4(buf, 0x3c, 'p', 'a', 'n', 'l')) {
		/* PRC Palm-pilot binary */
		return LOADFMT_PALM;
	} else if (TESTMAGIC4(buf, 0, 0xfe, 0xed, 0xfa, 0xce)
	        || TESTMAGIC4(buf, 0, 0xce, 0xfa, 0xed, 0xfe)) {
		/* Mach-O Mac OS-X binary */
		return LOADFMT_MACHO;
	} else if (buf[0] == 0x02
	        && buf[2] == 0x01
	        && (buf[1] == 0x10 || buf[1] == 0x0b)
	        && (buf[3] == 0x07 || buf[3] == 0x08 || buf[4] == 0x0b)) {
		/* HP Som binary (last as it's not really particularly good magic) */
		return LOADFMT_PAR;
	} else if (TESTMAGIC2(buf, 0, 0x4c, 0x01)) {
		return LOADFMT_COFF;
	}
	return LOADFMT_UNKNOWN;
}

/**
 * \overload
 * \param name  Name of file to perform detection on.
 */
static LOADFMT magic(const char *name)
{
	FILE *f = fopen(name, "rb");
	if (f == NULL) {
		fprintf(stderr, "%s: fopen: %s\n", name, strerror(errno));
		return LOADFMT_UNKNOWN;
	}

	LOADFMT format = magic(f);

	fclose(f);
	return format;
}

/**
 * Determines the type of a binary and loads the appropriate loader class
 * dynamically.
 *
 * \param name  Name of the file to open.
 *
 * \returns A new BinaryFile subclass instance.  Use close() to destroy it.
 */
BinaryFile *BinaryFile::open(const char *name)
{
	LOADFMT format = magic(name);
	if (format == LOADFMT_UNKNOWN) {
		fprintf(stderr, "%s: unrecognised binary file\n", name);
		return NULL;
	}

#ifdef DYNAMIC
	const char *libname;
	switch (format) {
	case LOADFMT_ELF:   libname = MODPREFIX    "ElfBinaryFile" MODSUFFIX; break;
	case LOADFMT_PE:    libname = MODPREFIX  "Win32BinaryFile" MODSUFFIX; break;
	case LOADFMT_PALM:  libname = MODPREFIX   "PalmBinaryFile" MODSUFFIX; break;
	case LOADFMT_PAR:   libname = MODPREFIX  "HpSomBinaryFile" MODSUFFIX; break;
	case LOADFMT_EXE:   libname = MODPREFIX    "ExeBinaryFile" MODSUFFIX; break;
	case LOADFMT_MACHO: libname = MODPREFIX  "MachOBinaryFile" MODSUFFIX; break;
	case LOADFMT_LX:    libname = MODPREFIX "DOS4GWBinaryFile" MODSUFFIX; break;
	case LOADFMT_COFF:  libname = MODPREFIX    "IntelCoffFile" MODSUFFIX; break;
	default:            return NULL;
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

#else
	BinaryFile *bf;
	switch (format) {
	case LOADFMT_ELF:   bf = new    ElfBinaryFile; break;
	case LOADFMT_PE:    bf = new  Win32BinaryFile; break;
	case LOADFMT_PALM:  bf = new   PalmBinaryFile; break;
	case LOADFMT_PAR:   bf = new  HpSomBinaryFile; break;
	case LOADFMT_EXE:   bf = new    ExeBinaryFile; break;
	case LOADFMT_MACHO: bf = new  MachOBinaryFile; break;
	case LOADFMT_LX:    bf = new DOS4GWBinaryFile; break;
	case LOADFMT_COFF:  bf = new    IntelCoffFile; break;
	default:            return NULL;
	}
#endif

	if (!bf->RealLoad(name)) {
		fprintf(stderr, "Loading '%s' failed\n", name);
		BinaryFile::close(bf); bf = NULL;
		return NULL;
	}

	bf->getTextLimits();
	return bf;
}

/**
 * \brief Destroys an instance created by open() or new.
 */
void BinaryFile::close(BinaryFile *bf)
{
#ifdef DYNAMIC
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
#endif
		delete bf;
}
