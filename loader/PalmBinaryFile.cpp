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

#include <fstream>

#include <cassert>
#include <cstdlib>
#include <cstring>

// Macro to convert a pointer to a Big Endian integer into a host integer
#define UC(p) ((unsigned char*)p)
#define UINT4(p) ((UC(p)[0] << 24) + (UC(p)[1] << 16) + (UC(p)[2] << 8) + \
    UC(p)[3])

PalmBinaryFile::PalmBinaryFile() :
	m_pImage(NULL),
	m_pData(NULL)
{
}

PalmBinaryFile::~PalmBinaryFile()
{
	for (int i = 0; i < m_iNumSections; i++)
		delete [] m_pSections[i].pSectionName;
	delete [] m_pSections;
	delete [] m_pImage;
	delete [] m_pData;
}

bool PalmBinaryFile::RealLoad(const char *sName)
{
	std::ifstream ifs;
	ifs.open(sName, ifs.binary);
	if (!ifs.good()) {
		fprintf(stderr, "Could not open binary file %s\n", sName);
		return false;
	}

	ifs.seekg(0, ifs.end);
	std::streamsize size = ifs.tellg();

	// Allocate a buffer for the image
	m_pImage = new unsigned char[size];
	memset(m_pImage, 0, size);

	ifs.seekg(0, ifs.beg);
	ifs.read((char *)m_pImage, size);
	if (!ifs.good()) {
		fprintf(stderr, "Error reading binary file %s\n", sName);
		return false;
	}

	ifs.close();

	// Check type at offset 0x3C; should be "appl" (or "palm"; ugh!)
	if (strncmp((char *)(m_pImage + 0x3C), "appl", 4) != 0
	 && strncmp((char *)(m_pImage + 0x3C), "panl", 4) != 0
	 && strncmp((char *)(m_pImage + 0x3C), "libr", 4) != 0) {
		fprintf(stderr, "%s is not a standard .prc file\n", sName);
		return false;
	}

	// Get the number of resource headers (one section per resource)
	m_iNumSections = (m_pImage[0x4C] << 8) + m_pImage[0x4D];

	// Allocate the section information
	m_pSections = new SectionInfo[m_iNumSections];

	// Iterate through the resource headers (generating section info structs)
	unsigned char *p = m_pImage + 0x4E;          // First resource header
	unsigned off = 0;
	for (int i = 0; i < m_iNumSections; i++) {
		// First get the name (4 alpha)
		char *name = new char[10];
		strncpy(name, (char *)p, 4);
		name[4] = '\0';
		p += 4;

		// Now get the identifier (2 byte binary)
		unsigned id = (p[0] << 8) + p[1];
		p += 2;

		// Decide if code or data; note that code0 is a special case (not code)
		m_pSections[i].bCode = strcmp(name, "code") == 0 && id != 0;
		m_pSections[i].bData = strcmp(name, "data") == 0;

		// Join the id to the name, e.g. code0, data12
		snprintf(name + 4, 6, "%u", id);
		m_pSections[i].pSectionName = name;

		off = UINT4(p);
		p += 4;
		m_pSections[i].uNativeAddr = off;
		m_pSections[i].uHostAddr = off + (ADDRESS)m_pImage;

		// Guess the length
		if (i > 0) {
			m_pSections[i - 1].uSectionSize = off - m_pSections[i - 1].uNativeAddr;
			m_pSections[i].uSectionEntrySize = 1;        // No info available
		}
	}

	// Set the length for the last section
	m_pSections[m_iNumSections - 1].uSectionSize = size - off;

	// Create a separate, uncompressed, initialised data section
	SectionInfo *pData = getSectionInfoByName("data0");
	if (pData == 0) {
		fprintf(stderr, "No data section!\n");
		return false;
	}

	SectionInfo *pCode0 = getSectionInfoByName("code0");
	if (pCode0 == 0) {
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
			unsigned char rle = *p++; in--;
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
	pData->uHostAddr = (ADDRESS)m_pData;
	pData->uSectionSize = sizeData;
	// May as well make the native address zero; certainly the offset in the
	// file is no longer appropriate (and is confusing)
	pData->uNativeAddr = 0;

	return true;
}

ADDRESS PalmBinaryFile::getEntryPoint()
{
	assert(0); /* FIXME: Need to be implemented */
	return 0;
}

#if 0 // Cruft?
bool PalmBinaryFile::PostLoad(void *handle)
{
	// Not needed: for archives only
	return false;
}
#endif

bool PalmBinaryFile::isLibrary() const
{
	return strncmp((char *)(m_pImage + 0x3C), "libr", 4) == 0;
}

std::list<const char *> PalmBinaryFile::getDependencyList()
{
	return std::list<const char *>(); /* doesn't really exist on palm */
}

ADDRESS PalmBinaryFile::getImageBase() const
{
	return 0; /* FIXME */
}

size_t PalmBinaryFile::getImageSize() const
{
	return 0; /* FIXME */
}

/**
 * We at least need to be able to name the main function and system calls.
 */
const char *PalmBinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	if ((dwAddr & 0xFFFFF000) == 0xAAAAA000) {
		// This is the convention used to indicate an A-line system call
		unsigned offset = dwAddr & 0xFFF;
		if (offset < numTrapStrings)
			return trapNames[offset];
		else
			return 0;
	}
	if (dwAddr == getMainEntryPoint())
		return "PilotMain";
	else return 0;
}

/**
 * \returns true if the address matches the convention for A-line system
 * calls.
 *
 * Not really dynamically linked, but the closest thing.
 */
bool PalmBinaryFile::isDynamicLinkedProc(ADDRESS uNative)
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
std::pair<unsigned, unsigned> PalmBinaryFile::getGlobalPointerInfo()
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
int PalmBinaryFile::getAppID() const
{
	// The answer is in the header. Return 0 if file not loaded
	if (m_pImage == 0)
		return 0;
	// Beware the endianness (large)
#define OFFSET_ID 0x40
	return (m_pImage[OFFSET_ID]     << 24)
	     + (m_pImage[OFFSET_ID + 1] << 16)
	     + (m_pImage[OFFSET_ID + 2] <<  8)
	     + (m_pImage[OFFSET_ID + 3]);
}

// Patterns for Code Warrior
#define WILD 0x4AFC
static SWord CWFirstJump[] = {
	0x0, 0x1,           // ? All Pilot programs seem to start with this
	0x487a, 0x4,        // pea 4(pc)
	0x0697, WILD, WILD, // addil #number, (a7)
	0x4e75              // rts
};
static SWord CWCallMain[] = {
	0x487a, 14,         // pea 14(pc)
	0x487a, 4,          // pea 4(pc)
	0x0697, WILD, WILD, // addil #number, (a7)
	0x4e75              // rts
};
static SWord GccCallMain[] = {
	0x3F04,             // movew d4, -(a7)
	0x6100, WILD,       // bsr xxxx
	0x3F04,             // movew d4, -(a7)
	0x2F05,             // movel d5, -(a7)
	0x3F06,             // movew d6, -(a7)
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
 * \returns 0 if no match; pointer to start of match if found.
 */
static SWord *findPattern(SWord *start, const SWord *patt, int pattSize, int max)
{
	const SWord *last = start + max;
	for (; start < last; start++) {
		bool found = true;
		for (int i = 0; i < pattSize; i++) {
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
	return 0;
}

/**
 * Find the native address for the start of the main entry function.
 * For Palm binaries, this is PilotMain.
 */
ADDRESS PalmBinaryFile::getMainEntryPoint()
{
	SectionInfo *pSect = getSectionInfoByName("code1");
	if (pSect == 0)
		return 0;  // Failed
	// Return the start of the code1 section
	SWord *startCode = (SWord *)pSect->uHostAddr;
	int delta = pSect->uHostAddr - pSect->uNativeAddr;

	// First try the CW first jump pattern
	SWord *res = findPattern(startCode, CWFirstJump, sizeof CWFirstJump / sizeof *CWFirstJump, 1);
	if (res) {
		// We have the code warrior first jump. Get the addil operand
		int addilOp = (startCode[5] << 16) + startCode[6];
		SWord *startupCode = (SWord *)((int)startCode + 10 + addilOp);
		// Now check the next 60 SWords for the call to PilotMain
		res = findPattern(startupCode, CWCallMain, sizeof CWCallMain / sizeof *CWCallMain, 60);
		if (res) {
			// Get the addil operand
			addilOp = (res[5] << 16) + res[6];
			// That operand plus the address of that operand is PilotMain
			return (ADDRESS)res + 10 + addilOp - delta;
		} else {
			fprintf(stderr, "Could not find call to PilotMain in CW app\n");
			return 0;
		}
	}
	// Check for gcc call to main
	res = findPattern(startCode, GccCallMain, sizeof GccCallMain / sizeof *GccCallMain, 75);
	if (res) {
		// Get the operand to the bsr
		SWord bsrOp = res[7];
		return (ADDRESS)res + 14 + bsrOp - delta;
	}

	fprintf(stderr, "Cannot find call to PilotMain\n");
	return 0;
}

/**
 * Generate binary files for non code and data sections.
 */
void PalmBinaryFile::generateBinFiles(const std::string &path) const
{
	for (int i = 0; i < m_iNumSections; i++) {
		SectionInfo *pSect = m_pSections + i;
		if (strncmp(pSect->pSectionName, "code", 4) != 0
		 && strncmp(pSect->pSectionName, "data", 4) != 0) {
			// Save this section in a file
			// First construct the file name
			char name[20];
			strncpy(name, pSect->pSectionName, 4);
			sprintf(name + 4, "%04x.bin", atoi(pSect->pSectionName + 4));
			std::string fullName(path);
			fullName += name;
			// Create the file
			FILE *f = fopen(fullName.c_str(), "w");
			if (f == NULL) {
				fprintf(stderr, "Could not open %s for writing binary file\n", fullName.c_str());
				return;
			}
			fwrite((void *)pSect->uHostAddr, pSect->uSectionSize, 1, f);
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
extern "C" BinaryFile *construct()
{
	return new PalmBinaryFile();
}
extern "C" void destruct(BinaryFile *bf)
{
	delete (PalmBinaryFile *)bf;
}
#endif
