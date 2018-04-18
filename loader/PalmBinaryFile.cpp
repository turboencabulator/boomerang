/**
 * \file
 * \brief Contains the implementation of the class PalmBinaryFile.
 *
 * \authors
 * Copyright (C) 2000, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "PalmBinaryFile.h"

#include "palmsystraps.h"

#include <cassert>
#include <cstdio>
#include <cstring>

// Macro to convert a pointer to a Big Endian integer into a host integer
#define UC(p) ((unsigned char*)p)
#define UINT4(p) ((UC(p)[0] << 24) + (UC(p)[1] << 16) + (UC(p)[2] << 8) + \
    UC(p)[3])

PalmBinaryFile::PalmBinaryFile()
{
}

PalmBinaryFile::~PalmBinaryFile()
{
	delete [] m_pImage;
	delete [] m_pData;
}

bool
PalmBinaryFile::load(std::istream &ifs)
{
	ifs.seekg(0, ifs.end);
	std::streamsize size = ifs.tellg();

	// Allocate a buffer for the image
	m_pImage = new unsigned char[size];

	ifs.seekg(0, ifs.beg);
	ifs.read((char *)m_pImage, size);
	if (!ifs.good()) {
		fprintf(stderr, "Error reading binary file %s\n", getFilename());
		return false;
	}

	// Check type at offset 0x3C; should be "appl" (or "palm"; ugh!)
	if (strncmp((char *)(m_pImage + 0x3C), "appl", 4) != 0
	 && strncmp((char *)(m_pImage + 0x3C), "panl", 4) != 0
	 && strncmp((char *)(m_pImage + 0x3C), "libr", 4) != 0) {
		fprintf(stderr, "%s is not a standard .prc file\n", getFilename());
		return false;
	}

	// Get the number of resource headers (one section per resource)
	int numSections = (m_pImage[0x4C] << 8) + m_pImage[0x4D];

	// Allocate the section information
	sections.reserve(numSections);
	SectionInfo *pData = nullptr;
	const SectionInfo *pCode0 = nullptr;

	// Iterate through the resource headers (generating section info structs)
	unsigned char *p = m_pImage + 0x4E;          // First resource header
	unsigned off = 0;
	for (int i = 0; i < numSections; ++i) {
		auto sect = SectionInfo();

		// First get the name (4 alpha)
		// XXX:  Assumes no embedded NULs
		auto name = std::string((char *)p, 4);
		p += 4;

		// Now get the identifier (2 byte binary)
		unsigned id = (p[0] << 8) + p[1];
		p += 2;

		// Decide if code or data
		// Note that code0 is a special case (not code)
		sect.bCode = name == "code" && id != 0;
		sect.bData = name == "data";

		// Join the id to the name, e.g. code0, data12
		sect.name = name + std::to_string(id);

		off = UINT4(p);
		p += 4;
		sect.uNativeAddr = off;
		sect.uHostAddr = (char *)m_pImage + off;

		// Guess the length
		if (i > 0) {
			sections.back().uSectionSize = off - sections.back().uNativeAddr;
			sect.uSectionEntrySize = 1;        // No info available
		}

		sections.push_back(sect);
		if (id == 0) {
			if (name == "code")
				pCode0 = &sections.back();
			else if (name == "data")
				pData = &sections.back();
		}
	}

	// Set the length for the last section
	sections.back().uSectionSize = size - off;

	// Create a separate, uncompressed, initialised data section
	if (!pData) {
		fprintf(stderr, "No data section!\n");
		return false;
	}
	if (!pCode0) {
		fprintf(stderr, "No code 0 section!\n");
		return false;
	}

	// When the info is all boiled down, the two things we need from the
	// code 0 section are at offset 0, the size of data above a5, and at
	// offset 4, the size below. Save the size below as a member variable
	m_SizeBelowA5 = UINT4(pCode0->uHostAddr + 4);
	// Total size is this plus the amount above (>=) a5
	unsigned sizeData = m_SizeBelowA5 + UINT4(pCode0->uHostAddr);

	// Allocate a new data section
	m_pData = new unsigned char[sizeData];
	// Assume anything not filled in is 0?
	memset(m_pData, 0, sizeData);

	// Skip first long (offset of CODE1 "xrefs")
	p = (unsigned char *)(pData->uHostAddr + 4);
	unsigned in = pData->uSectionSize - 4;

	// Next are 3 global initializers, each a 32-bit offset from A5
	// followed by a compressed stream.  Uncompress into that offset.
	//
	// Let p := input data,  in  := input bytes remaining,
	//     q := output data, out := bytes to end of output buffer.
	// Break out of the loops with done == false to indicate error.
	bool done;
	for (int i = 0; i < 3; ++i) {
		done = false;
		if (in < 4) break;
		int start = (int)UINT4(p);
		p += 4; in -= 4;
		start += m_SizeBelowA5;
		if (start < 0 || start >= sizeData) break;
		unsigned char *q = m_pData + start;
		unsigned out = sizeData - start;

		while (in) {
			unsigned char rle = *p++; --in;
			if (rle >= 0x80) {
				// (0x80 + n) b_0 b_1 ... b_n
				// => n+1 bytes of literal data (n <= 127)
				rle -= 0x7f;
				if (in < rle || out < rle) break;
				in -= rle; out -= rle;
				for (int k = 0; k < rle; ++k)
					*q++ = *p++;
			} else if (rle >= 0x40) {
				// (0x40 + n)
				// => n+1 repetitions of 0x00 (n <= 63)
				rle -= 0x3f;
				if (out < rle) break;
				out -= rle;
				for (int k = 0; k < rle; ++k)
					*q++ = 0x00;
			} else if (rle >= 0x20) {
				// (0x20 + n) b
				// => n+2 repetitions of b (n <= 31)
				rle -= 0x1e;
				if (in < 1 || out < rle) break;
				in -= 1; out -= rle;
				unsigned char b = *p++;
				for (int k = 0; k < rle; ++k)
					*q++ = b;
			} else if (rle >= 0x10) {
				// (0x10 + n)
				// => n+1 repetitions of 0xff (n <= 15)
				rle -= 0x0f;
				if (out < rle) break;
				out -= rle;
				for (int k = 0; k < rle; ++k)
					*q++ = 0xff;
			} else if (rle == 1) {
				// 0x01 b_0 b_1
				// => 0x00 0x00 0x00 0x00 0xff 0xff b_0 b_1
				if (in < 2 || out < 8) break;
				in -= 2; out -= 8;
				*q++ = 0x00; *q++ = 0x00; *q++ = 0x00; *q++ = 0x00;
				*q++ = 0xff; *q++ = 0xff; *q++ = *p++; *q++ = *p++;
			} else if (rle == 2) {
				// 0x02 b_0 b_1 b_2
				// => 0x00 0x00 0x00 0x00 0xff b_0 b_1 b_2
				if (in < 3 || out < 8) break;
				in -= 3; out -= 8;
				*q++ = 0x00; *q++ = 0x00; *q++ = 0x00; *q++ = 0x00;
				*q++ = 0xff; *q++ = *p++; *q++ = *p++; *q++ = *p++;
			} else if (rle == 3) {
				// 0x03 b_0 b_1 b_2
				// => 0xa9 0xf0 0x00 0x00 b_0 b_1 0x00 b_2
				if (in < 3 || out < 8) break;
				in -= 3; out -= 8;
				*q++ = 0xa9; *q++ = 0xf0; *q++ = 0x00; *q++ = 0x00;
				*q++ = *p++; *q++ = *p++; *q++ = 0x00; *q++ = *p++;
			} else if (rle == 4) {
				// 0x04 b_0 b_1 b_2 b_3
				// => 0xA9 0xF0 0x00 b_0 b_1 b_2 0x00 b_3
				if (in < 4 || out < 8) break;
				in -= 4; out -= 8;
				*q++ = 0xa9; *q++ = 0xf0; *q++ = 0x00; *q++ = *p++;
				*q++ = *p++; *q++ = *p++; *q++ = 0x00; *q++ = *p++;
			} else if (rle == 0) {
				done = true;
				break;
			} else {
				// 5-0xf are invalid.
				assert(0);
			}
		}
		if (!done) break;
	}

	if (!done)
		fprintf(stderr, "Warning! Compressed data section premature end\n");
	//printf("Used %u bytes of %u in decompressing data section\n",
	//       pData->uSectionSize - in, pData->uSectionSize);

	// Replace the data pointer and size with the uncompressed versions
	pData->uHostAddr = (char *)m_pData;
	pData->uSectionSize = sizeData;
	// May as well make the native address zero; certainly the offset in the
	// file is no longer appropriate (and is confusing)
	pData->uNativeAddr = 0;

	return true;
}

ADDRESS
PalmBinaryFile::getEntryPoint() const
{
	assert(0); /* FIXME: Need to be implemented */
	return 0;
}

#if 0 // Cruft?
bool
PalmBinaryFile::PostLoad(void *handle)
{
	// Not needed: for archives only
	return false;
}
#endif

#if 0 // Cruft?
bool
PalmBinaryFile::isLibrary() const
{
	return strncmp((char *)(m_pImage + 0x3C), "libr", 4) == 0;
}
#endif

ADDRESS
PalmBinaryFile::getImageBase() const
{
	return 0; /* FIXME */
}

size_t
PalmBinaryFile::getImageSize() const
{
	return 0; /* FIXME */
}

/**
 * We at least need to be able to name the main function and system calls.
 */
const char *
PalmBinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	if ((dwAddr & 0xFFFFF000) == 0xAAAAA000) {
		// This is the convention used to indicate an A-line system call
		unsigned offset = dwAddr & 0xFFF;
		if (offset < numTrapStrings)
			return trapNames[offset];
		else
			return nullptr;
	}
	if (dwAddr == getMainEntryPoint())
		return "PilotMain";
	else
		return nullptr;
}

/**
 * \returns true if the address matches the convention for A-line system
 * calls.
 *
 * Not really dynamically linked, but the closest thing.
 */
bool
PalmBinaryFile::isDynamicLinkedProc(ADDRESS uNative) const
{
	return ((uNative & 0xFFFFF000) == 0xAAAAA000);
}

#if 0 // Cruft?
/**
 * Specific to BinaryFile objects that implement a "global pointer".  Gets a
 * pair of unsigned integers representing the address of %agp, and the value
 * for GLOBALOFFSET.
 *
 * For Palm, the latter is the amount of space allocated below %a5, i.e. the
 * difference between %a5 and %agp (%agp points to the bottom of the global
 * data area).
 */
std::pair<unsigned, unsigned>
PalmBinaryFile::getGlobalPointerInfo()
{
	unsigned agp = 0;
	const SectionInfo *ps = getSectionInfoByName("data0");
	if (ps) agp = ps->uNativeAddr;
	std::pair<unsigned, unsigned> ret(agp, m_SizeBelowA5);
	return ret;
}
#endif

/**
 * Get the ID number for this application.  It's possible that the app uses
 * this number internally, so this needs to be used in the final make.
 */
int
PalmBinaryFile::getAppID() const
{
	// The answer is in the header. Return 0 if file not loaded
	if (!m_pImage)
		return 0;
	// Beware the endianness (large)
#define OFFSET_ID 0x40
	return (m_pImage[OFFSET_ID]     << 24)
	     + (m_pImage[OFFSET_ID + 1] << 16)
	     + (m_pImage[OFFSET_ID + 2] <<  8)
	     + (m_pImage[OFFSET_ID + 3]);
}

// Patterns for Code Warrior
#define WILD 0x4afc
static const SWord CWFirstJump[] = {
	0x0, 0x1,           // ? All Pilot programs seem to start with this
	0x487a, 0x4,        // pea 4(pc)
	0x0697, WILD, WILD, // addil #number, (a7)
	0x4e75              // rts
};
static const SWord CWCallMain[] = {
	0x487a, 14,         // pea 14(pc)
	0x487a, 4,          // pea 4(pc)
	0x0697, WILD, WILD, // addil #number, (a7)
	0x4e75              // rts
};
static const SWord GccCallMain[] = {
	0x3f04,             // movew d4, -(a7)
	0x6100, WILD,       // bsr xxxx
	0x3f04,             // movew d4, -(a7)
	0x2f05,             // movel d5, -(a7)
	0x3f06,             // movew d6, -(a7)
	0x6100, WILD        // bsr PilotMain
};

/**
 * Try to find a pattern.
 *
 * \param start     Pointer to code to start searching.
 * \param patt      Pattern to look for.
 * \param pattSize  Size of the pattern (in SWords).
 * \param max       Max number of SWords to search.
 *
 * \returns nullptr if no match; pointer to start of match if found.
 */
static const SWord *
findPattern(const SWord *start, const SWord *patt, size_t pattSize, ptrdiff_t max)
{
	const SWord *last = start + max;
	for (; start < last; ++start) {
		bool found = true;
		for (size_t i = 0; i < pattSize; ++i) {
			SWord curr = patt[i];
			if ((curr != WILD) && (curr != start[i])) {
				found = false;
				break;              // Mismatch
			}
		}
		if (found)
			// All parts of the pattern matched
			return start;
	}
	// Each start position failed
	return nullptr;
}

/**
 * Find the native address for the start of the main entry function.
 * For Palm binaries, this is PilotMain.
 */
ADDRESS
PalmBinaryFile::getMainEntryPoint()
{
	const SectionInfo *pSect = getSectionInfoByName("code1");
	if (!pSect) return NO_ADDRESS;
	const SWord *startCode = (const SWord *)pSect->uHostAddr;

	// First try the CW first jump pattern
	if (auto jump = findPattern(startCode, CWFirstJump, sizeof CWFirstJump / sizeof *CWFirstJump, 1)) {
		// We have the code warrior first jump. Get the addil operand
		int addilOp = (jump[5] << 16) + jump[6];
		startCode = (const SWord *)((const char *)jump + 10 + addilOp);
		// Now check the next 60 SWords for the call to PilotMain
		if (auto call = findPattern(startCode, CWCallMain, sizeof CWCallMain / sizeof *CWCallMain, 60)) {
			// Get the addil operand
			addilOp = (call[5] << 16) + call[6];
			// That operand plus the address of that operand is PilotMain
			return pSect->uNativeAddr + (((const char *)call + 10 + addilOp) - pSect->uHostAddr);
		} else {
			fprintf(stderr, "Could not find call to PilotMain in CW app\n");
			return NO_ADDRESS;
		}
	}
	// Check for gcc call to main
	if (auto call = findPattern(startCode, GccCallMain, sizeof GccCallMain / sizeof *GccCallMain, 75)) {
		// Get the operand to the bsr
		SWord bsrOp = call[7];
		return pSect->uNativeAddr + (((const char *)call + 14 + bsrOp) - pSect->uHostAddr);
	}

	fprintf(stderr, "Cannot find call to PilotMain\n");
	return NO_ADDRESS;
}

/**
 * Generate binary files for non code and data sections.
 */
void
PalmBinaryFile::generateBinFiles(const std::string &path) const
{
	for (auto &sect : sections) {
		if (sect.name.compare(0, 4, "code") != 0
		 && sect.name.compare(0, 4, "data") != 0) {
			// Save this section in a file
			auto name = std::string(path + sect.name + ".bin");
			FILE *f = fopen(name.c_str(), "w");
			if (!f) {
				fprintf(stderr, "Could not open %s for writing binary file\n", name.c_str());
				return;
			}
			fwrite(sect.uHostAddr, sect.uSectionSize, 1, f);
			fclose(f);
		}
	}
}

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new BinaryFile
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" BinaryFile *
construct()
{
	return new PalmBinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (PalmBinaryFile *)bf;
}
#endif
