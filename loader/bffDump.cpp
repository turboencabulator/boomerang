/**
 * \file
 * \brief Skeleton driver for a binary-file dumper program.
 *
 * This file is a generic skeleton for a binary-file dumper, it dumps all the
 * information it finds about sections in the file, and it displays any code
 * sections in raw hexadecimal notation.
 *
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2001, The University of Queensland
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

#include <cstdio>

static void
print_section(const SectionInfo *pSect)
{
	ADDRESS a = pSect->uNativeAddr;
	unsigned char *p = (unsigned char *)pSect->uHostAddr;
	for (unsigned off = 0; off < pSect->uSectionSize;) {
		printf("%04x:", a);
		for (int j = 0; (j < 16) && (off < pSect->uSectionSize); ++j) {
			printf(" %02x", *p++);
			++a;
			++off;
		}
		printf("\n");
	}
}

int
main(int argc, char *argv[])
{
	// Usage
	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		printf("%s dumps the contents of the given executable file\n", argv[0]);
		return 1;
	}

	// Load the file
	BinaryFile *bf = BinaryFile::open(argv[1]);
	if (!bf) {
		return 2;
	}

	// Display the code section(s) in raw hexadecimal notation.
	// Note: this is traditionally the ".text" section in Elf binaries.
	// In the case of Prc files (Palm), the code section is named "code0".
	for (int i = 0; i < bf->getNumSections(); ++i) {
		const SectionInfo *pSect = bf->getSectionInfo(i);
		if (pSect->bCode) {
			printf("  Code section: %s\n", pSect->pSectionName);
			print_section(pSect);
			printf("\n");
		}
	}

	// Display the data section(s) in raw hexadecimal notation.
	for (int i = 0; i < bf->getNumSections(); ++i) {
		const SectionInfo *pSect = bf->getSectionInfo(i);
		if (pSect->bData) {
			printf("  Data section: %s\n", pSect->pSectionName);
			print_section(pSect);
			printf("\n");
		}
	}

	BinaryFile::close(bf);
	return 0;
}
