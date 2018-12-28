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
BinaryFile::getSectionIndexByName(const std::string &name) const
{
	for (int i = 0; i < sections.size(); ++i) {
		if (sections[i].name == name) {
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
	for (const auto &sect : sections) {
		if (uEntry >= sect.uNativeAddr
		 && (uEntry - sect.uNativeAddr) < sect.uSectionSize) {
			return &sect;
		}
	}
	return nullptr;
}

/**
 * \brief Find section info given name, or nullptr if not found.
 */
const SectionInfo *
BinaryFile::getSectionInfoByName(const std::string &name) const
{
	for (const auto &sect : sections) {
		if (sect.name == name) {
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

char *
BinaryFile::getSectionData(ADDRESS a, ADDRESS range) const
{
	for (const auto &sect : sections) {
		if (a >= sect.uNativeAddr
		 && (a - sect.uNativeAddr) < sect.uSectionSize
		 && range <= sect.uSectionSize - (a - sect.uNativeAddr)) {
			return sect.uHostAddr + (a - sect.uNativeAddr);
		}
	}
	return nullptr;
}

/**
 * \brief Read 1 byte from given native address a; considers endianness.
 */
int
BinaryFile::readNative1(ADDRESS a) const
{
	auto data = (unsigned char *)getSectionData(a, 1);
	if (!data) return 0;
	return *data;
}

/**
 * \brief Read 2 bytes from given native address a; considers endianness.
 */
int
BinaryFile::readNative2(ADDRESS a) const
{
	auto data = (unsigned char *)getSectionData(a, 2);
	if (!data) return 0;

	uint16_t raw = 0;
	if (bigendian)
		for (int i = 0; i < 2; ++i)
			raw = (raw << 8) | data[i];
	else
		for (int i = 1; i >= 0; --i)
			raw = (raw << 8) | data[i];
	return raw;
}

/**
 * \brief Read 4 bytes from given native address a; considers endianness.
 */
int
BinaryFile::readNative4(ADDRESS a) const
{
	auto data = (unsigned char *)getSectionData(a, 4);
	if (!data) return 0;

	uint32_t raw = 0;
	if (bigendian)
		for (int i = 0; i < 4; ++i)
			raw = (raw << 8) | data[i];
	else
		for (int i = 3; i >= 0; --i)
			raw = (raw << 8) | data[i];
	return raw;
}

/**
 * \brief Read 8 bytes from given native address a; considers endianness.
 */
uint64_t
BinaryFile::readNative8(ADDRESS a) const
{
	auto data = (unsigned char *)getSectionData(a, 8);
	if (!data) return 0;

	uint64_t raw = 0;
	if (bigendian)
		for (int i = 0; i < 8; ++i)
			raw = (raw << 8) | data[i];
	else
		for (int i = 7; i >= 0; --i)
			raw = (raw << 8) | data[i];
	return raw;
}

/**
 * \brief Read 4 bytes as a float; considers endianness.
 */
float
BinaryFile::readNativeFloat4(ADDRESS a) const
{
	auto data = (unsigned char *)getSectionData(a, 4);
	if (!data) return 0.0f;

	uint32_t raw = 0;
	if (bigendian)
		for (int i = 0; i < 4; ++i)
			raw = (raw << 8) | data[i];
	else
		for (int i = 3; i >= 0; --i)
			raw = (raw << 8) | data[i];

	union { uint32_t i; float f; } u;
	u.i = raw;
	return u.f;
}

/**
 * \brief Read 8 bytes as a float; considers endianness.
 */
double
BinaryFile::readNativeFloat8(ADDRESS a) const
{
	auto data = (unsigned char *)getSectionData(a, 8);
	if (!data) return 0.0;

	uint64_t raw = 0;
	if (bigendian)
		for (int i = 0; i < 8; ++i)
			raw = (raw << 8) | data[i];
	else
		for (int i = 7; i >= 0; --i)
			raw = (raw << 8) | data[i];

	union { uint64_t i; double f; } u;
	u.i = raw;
	return u.f;
}

/**
 * \brief Add an extra symbol.
 */
void
BinaryFile::addSymbol(ADDRESS uNative, const std::string &name)
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
BinaryFile::getAddressByName(const std::string &name, bool bNoTypeOK) const
{
	return 0;
}

/**
 * \brief Get the size associated with the symbol.
 */
int
BinaryFile::getSizeByName(const std::string &name, bool bNoTypeOK) const
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

#if 0 // Cruft?
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
#endif

/**
 * \brief Get the lower and upper limits of the text segment.
 */
void
BinaryFile::getTextLimits()
{
	limitTextLow = 0xFFFFFFFF;
	limitTextHigh = 0;
	for (const auto &sect : sections) {
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
		}
	}
}
