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

#include <fstream>

#include <cassert>
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
 * \param ifs  Opened stream to perform detection on.
 */
static LOADFMT
magic(std::istream &ifs)
{
	char buf[0x40];
	ifs.read(buf, sizeof buf);
	if (!ifs.good()) return LOADFMT_UNKNOWN;

	if (TESTMAGIC4(buf, 0, '\x7f', 'E', 'L', 'F')) {
		/* ELF Binary */
		return LOADFMT_ELF;
	} else if (TESTMAGIC2(buf, 0, '\x4c', '\x01')) {
		return LOADFMT_COFF;
	} else if (TESTMAGIC2(buf, 0, 'M', 'Z')) {
		/* DOS-based file */
		int peoff = LH32(&buf[0x3c]);
		if (peoff != 0
		 && ifs.seekg(peoff).read(buf, 4).good()) {
			if (TESTMAGIC4(buf, 0, 'P', 'E', '\0', '\0')) {
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
	} else if (TESTMAGIC4(buf, 0, '\xfe', '\xed', '\xfa', '\xce')
	        || TESTMAGIC4(buf, 0, '\xce', '\xfa', '\xed', '\xfe')) {
		/* Mach-O Mac OS-X binary */
		return LOADFMT_MACHO;
	} else if ((buf[0] == '\x02')
	        && (buf[1] == '\x10' || buf[1] == '\x0b')
	        && (buf[2] == '\x01')
	        && (buf[3] == '\x07' || buf[3] == '\x08' || buf[3] == '\x0b')) {
		/* HP Som binary (last as it's not really particularly good magic) */
		return LOADFMT_PAR;
	}
	return LOADFMT_UNKNOWN;
}

/**
 * Determines the type of a binary and loads the appropriate loader class
 * dynamically.
 *
 * \param name  Name of the file to open.
 *
 * \returns A new BinaryFile subclass instance.  Use close() to destroy it.
 */
BinaryFile *
BinaryFile::open(const char *name)
{
	std::ifstream ifs;
	ifs.open(name, ifs.binary);
	if (!ifs.good()) {
		fprintf(stderr, "%s: opening failed\n", name);
		return nullptr;
	}

	LOADFMT format = magic(ifs);
	if (format == LOADFMT_UNKNOWN) {
		fprintf(stderr, "%s: unrecognised binary file\n", name);
		ifs.close();
		return nullptr;
	}
	ifs.seekg(0);

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
	default:            libname = nullptr; assert(0);  // found a LOADFMT not listed above
	}

	// Load the specific loader library
	void *handle = dlopen(libname, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "cannot load library: %s\n", dlerror());
		ifs.close();
		return nullptr;
	}

	// Reset errors
	const char *error = dlerror();

	// Use the handle to find symbols
	const char *symbol = "construct";
	constructFcn construct = (constructFcn)dlsym(handle, symbol);
	error = dlerror();
	if (error) {
		fprintf(stderr, "cannot load symbol '%s': %s\n", symbol, error);
		dlclose(handle);
		ifs.close();
		return nullptr;
	}
	symbol = "destruct";
	destructFcn destruct = (destructFcn)dlsym(handle, symbol);
	error = dlerror();
	if (error) {
		fprintf(stderr, "cannot load symbol '%s': %s\n", symbol, error);
		dlclose(handle);
		ifs.close();
		return nullptr;
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
	default:            bf = nullptr; assert(0);  // found a LOADFMT not listed above
	}
#endif

	bf->m_pFilename = name;

	if (!bf->load(ifs)) {
		fprintf(stderr, "%s: loading failed\n", name);
		BinaryFile::close(bf); bf = nullptr;
	}
	ifs.close();

	if (bf) bf->getTextLimits();
	return bf;
}

/**
 * \brief Destroys an instance created by open() or new.
 */
void
BinaryFile::close(BinaryFile *bf)
{
#ifdef DYNAMIC
	// Retrieve the stashed pointers
	void *handle = bf->dlHandle;
	destructFcn destruct = bf->destruct;

	// Destruct in an appropriate way.
	// The C++ dlopen mini HOWTO says to always use a matching
	// construct/destruct pair in case of new/delete overloading.
	if (handle) {
		destruct(bf);
		dlclose(handle);
	} else
#endif
		delete bf;
}
