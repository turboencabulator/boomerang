/**
 * \file
 * \brief Contains the implementation of the class BinaryFile.
 *
 * \authors
 * Copyright (C) 1997-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BinaryFile.h"

#include <iostream>

SectionInfo::SectionInfo() :
	bCode(false),
	bData(false),
	bBss(false),
	bReadOnly(false)
{
}

SectionInfo::~SectionInfo()
{
}

BinaryFile::BinaryFile(bool bArch /*= false*/) :
	m_bArchive(bArch)  // Remember whether an archive member
{
}

BinaryFile::~BinaryFile()
{
}

/**
 * \brief Return number of sections.
 */
int
BinaryFile::getNumSections() const
{
	return sections.size();
}

/**
 * \brief Find section index given name, or -1 if not found.
 */
int
BinaryFile::getSectionIndexByName(const char *sName) const
{
	for (int i = 0; i < sections.size(); ++i) {
		if (sections[i].name == sName) {
			return i;
		}
	}
	return -1;
}

/**
 * \brief Return section struct.
 */
const SectionInfo *
BinaryFile::getSectionInfo(int idx) const
{
	if (idx < sections.size()) {
		return &sections[idx];
	}
	return nullptr;
}

/**
 * \brief Find section info given an address in the section.
 */
const SectionInfo *
BinaryFile::getSectionInfoByAddr(ADDRESS uEntry) const
{
	for (auto &sect : sections) {
		if ((uEntry >= sect.uNativeAddr)
		 && (uEntry <  sect.uNativeAddr + sect.uSectionSize)) {
			return &sect;
		}
	}
	return nullptr;
}

/**
 * \brief Find section info given name, or nullptr if not found.
 */
const SectionInfo *
BinaryFile::getSectionInfoByName(const char *sName) const
{
	for (auto &sect : sections) {
		if (sect.name == sName) {
			return &sect;
		}
	}
	return nullptr;
}

/**
 * \brief Returns true if the given address is in a read only section.
 */
bool
BinaryFile::isReadOnly(ADDRESS uEntry) const
{
	const SectionInfo *p = getSectionInfoByAddr(uEntry);
	return p && p->bReadOnly;
}

///////////////////////
// Trivial functions //
// Overridden if reqd//
///////////////////////

/**
 * \brief Read 1 byte from given native address a; considers endianness.
 */
int
BinaryFile::readNative1(ADDRESS a) const
{
	return 0;
}

/**
 * \brief Read 2 bytes from given native address a; considers endianness.
 */
int
BinaryFile::readNative2(ADDRESS a) const
{
	return 0;
}

/**
 * \brief Read 4 bytes from given native address a; considers endianness.
 */
int
BinaryFile::readNative4(ADDRESS a) const
{
	return 0;
}

/**
 * \brief Read 8 bytes from given native address a; considers endianness.
 */
QWord
BinaryFile::readNative8(ADDRESS a) const
{
	return 0;
}

/**
 * \brief Read 4 bytes as a float; considers endianness.
 */
float
BinaryFile::readNativeFloat4(ADDRESS a) const
{
	return 0.;
}

/**
 * \brief Read 8 bytes as a float; considers endianness.
 */
double
BinaryFile::readNativeFloat8(ADDRESS a) const
{
	return 0.;
}

/**
 * \brief Add an extra symbol.
 */
void
BinaryFile::addSymbol(ADDRESS uNative, const char *pName)
{
}

/**
 * \brief Get name of symbol.
 *
 * Lookup the address, return the name, or nullptr if not found.
 *
 * Overridden by subclasses that support syms.
 */
const char *
BinaryFile::getSymbolByAddress(ADDRESS uNative)
{
	return nullptr;
}

/**
 * \brief Get value of symbol, if any.
 *
 * \returns NO_ADDRESS if not found.
 */
ADDRESS
BinaryFile::getAddressByName(const char *pName, bool bNoTypeOK) const
{
	return 0;
}

/**
 * \brief Get the size associated with the symbol.
 */
int
BinaryFile::getSizeByName(const char *pName, bool bNoTypeOK) const
{
	return 0;
}

ADDRESS
BinaryFile::isJumpToAnotherAddr(ADDRESS uNative) const
{
	return NO_ADDRESS;
}

bool
BinaryFile::isStaticLinkedLibProc(ADDRESS uNative) const
{
	return false;
}

bool
BinaryFile::isDynamicLinkedProc(ADDRESS uNative) const
{
	return false;
}

bool
BinaryFile::isDynamicLinkedProcPointer(ADDRESS uNative) const
{
	return false;
}

const char *
BinaryFile::getDynamicProcName(ADDRESS uNative) const
{
	return nullptr;
}

#if 0 // Cruft?
/**
 * Specific to BinaryFile objects that implement a "global pointer".  Gets a
 * pair of unsigned integers representing the address of the abstract global
 * pointer (%agp) (in first) and a constant that will be available in the
 * csrparser as GLOBALOFFSET (second).
 *
 * At present, the latter is only used by the Palm machine, to represent the
 * space allocated below the %a5 register (i.e. the difference between %a5 and
 * %agp).  This value could possibly be used for other purposes.
 *
 * \note This is a stub routine that should be overridden if required.
 */
std::pair<unsigned, unsigned>
BinaryFile::getGlobalPointerInfo()
{
	return std::pair<unsigned, unsigned>(0, 0);
}
#endif

#if 0 // Cruft?
/**
 * \brief Get a pointer to a new map of dynamic global data items.
 *
 * Get a map from ADDRESS to const char*.  This map contains the native
 * addresses and symbolic names of global data items (if any) which are shared
 * with dynamically linked libraries.
 *
 * Example: __iob (basis for stdout).  The ADDRESS is the native address of a
 * pointer to the real dynamic data object.
 *
 * If the derived class doesn't implement this function, return an empty map.
 *
 * \note The caller should delete the returned map.
 *
 * \returns Pointer to a new map with the info, or 0 if none.
 */
std::map<ADDRESS, const char *> *
BinaryFile::getDynamicGlobalMap()
{
	return new std::map<ADDRESS, const char *>;
}
#endif

/**
 * \brief Get an array of exported function stub addresses.
 * Normally overridden.
 *
 * Get an array of addresses of imported function stubs.
 * \param[out] numImports  Number of array elements.
 */
ADDRESS *
BinaryFile::getImportStubs(int &numImports)
{
	numImports = 0;
	return nullptr;
}

/**
 * \brief Get the lower and upper limits of the text segment.
 */
void
BinaryFile::getTextLimits()
{
	limitTextLow = 0xFFFFFFFF;
	limitTextHigh = 0;
	textDelta = 0;
	for (auto &sect : sections) {
		if (sect.bCode) {
			// The .plt section is an anomaly. It's code, but we never want to
			// decode it, and in Sparc ELF files, it's actually in the data
			// segment (so it can be modified). For now, we make this ugly
			// exception
			if (sect.name == ".plt")
				continue;
			if (limitTextLow > sect.uNativeAddr)
				limitTextLow = sect.uNativeAddr;
			ADDRESS hiAddress = sect.uNativeAddr + sect.uSectionSize;
			if (limitTextHigh < hiAddress)
				limitTextHigh = hiAddress;
			if (textDelta == 0)
				textDelta = sect.uHostAddr - (char *)sect.uNativeAddr;
			else if (textDelta != sect.uHostAddr - (char *)sect.uNativeAddr)
				std::cerr << "warning: textDelta different for section "
				          << sect.name
				          << " (ignoring).\n";
		}
	}
}
