/**
 * \file
 * \brief Contains the implementation of the class ElfBinaryFile.
 *
 * \authors
 * Copyright (C) 1997-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ElfBinaryFile.h"

#include <iostream>

#include <cstdio>
#include <cstring>

ElfBinaryFile::ElfBinaryFile(bool bArchive /* = false */) :
	BinaryFile(bArchive)
{
}

ElfBinaryFile::~ElfBinaryFile()
{
	delete [] m_pImage;
	delete [] m_pImportStubs;
	delete [] m_sh_link;
	delete [] m_sh_info;
}

/**
 * Hand decompiled from sparc library function.
 *
 * C linkage so we can call this with dlopen().
 */
extern "C" unsigned
elf_hash(const char *o0)
{
	int o3 = *o0;
	const char *g1 = o0;
	unsigned o4 = 0;
	while (o3 != 0) {
		o4 <<= 4;
		o3 += o4;
		++g1;
		o4 = o3 & 0xf0000000;
		if (o4 != 0) {
			int o2 = (int)((unsigned)o4 >> 24);
			o3 = o3 ^ o2;
		}
		o4 = o3 & ~o4;
		o3 = *g1;
	}
	return o4;
}

bool
ElfBinaryFile::load(std::istream &ifs)
{
	if (m_bArchive) {
		// This is a member of an archive. Should not be using this function at all
		return false;
	}

	// Determine file size
	ifs.seekg(0, ifs.end);
	if (!ifs.good()) {
		fprintf(stderr, "Error seeking to end of binary file\n");
		return false;
	}
	std::streamsize size = ifs.tellg();
	ifs.seekg(0, ifs.beg);

	// Allocate memory to hold the file
	m_pImage = new char[size];
	Elf32_Ehdr *pHeader = (Elf32_Ehdr *)m_pImage;  // Save a lot of casts

	// Read the whole file in
	ifs.read(m_pImage, size);
	if (!ifs.good())
		fprintf(stderr, "WARNING! Only read %ld of %ld bytes of binary file!\n", ifs.gcount(), size);

	// Basic checks
	if (strncmp(m_pImage, "\x7F""ELF", 4) != 0) {
		fprintf(stderr, "Incorrect header: %02X %02X %02X %02X\n",
		        pHeader->e_ident[0], pHeader->e_ident[1], pHeader->e_ident[2],
		        pHeader->e_ident[3]);
		return false;
	}
	if ((pHeader->endianness != 1) && (pHeader->endianness != 2)) {
		fprintf(stderr, "Unknown endianness %02X\n", pHeader->endianness);
		return false;
	}
	// Needed for elfRead4 to work:
	bigendian = pHeader->endianness - 1;

	// Set up program header pointer (in case needed)
	if (int i = elfRead4(&pHeader->e_phoff))
		m_pPhdrs = (const Elf32_Phdr *)(m_pImage + i);

	// Set up section header pointer
	if (int i = elfRead4(&pHeader->e_shoff))
		m_pShdrs = (const Elf32_Shdr *)(m_pImage + i);

	// Set up section header string table pointer
	// NOTE: it does not appear that endianness affects shorts.. they are always in little endian format
	// Gerard: I disagree. I need the elfRead on linux/i386
	if (int i = elfRead2(&pHeader->e_shstrndx)) // pHeader->e_shstrndx;
		m_pStrings = m_pImage + elfRead4(&m_pShdrs[i].sh_offset);

	// Number of sections
	int numSections = elfRead2(&pHeader->e_shnum);

	// Allocate room for all the Elf sections (including the silly first one)
	sections.reserve(numSections);

	// Set up the m_sh_link and m_sh_info arrays
	m_sh_link = new int[numSections];
	m_sh_info = new int[numSections];

	// Number of elf sections
	bool bGotCode = false;                  // True when have seen a code sect
	ADDRESS arbitaryLoadAddr = 0x08000000;
	for (int i = 0; i < numSections; ++i) {
		// Get section information.
		const Elf32_Shdr *pShdr = m_pShdrs + i;
		if ((const char *)pShdr > m_pImage + size) {
			std::cerr << "section " << i << " header is outside the image size\n";
			return false;
		}
		const char *pName = m_pStrings + elfRead4(&pShdr->sh_name);
		if (pName > m_pImage + size) {
			std::cerr << "name for section " << i << " is outside the image size\n";
			return false;
		}
		auto sect = SectionInfo();
		sect.name = pName;
		if (int off = elfRead4(&pShdr->sh_offset))
			sect.uHostAddr = m_pImage + off;
		sect.uNativeAddr = elfRead4(&pShdr->sh_addr);
		sect.uSectionSize = elfRead4(&pShdr->sh_size);
		if (sect.uNativeAddr == 0 && sect.name.compare(0, 4, ".rel") != 0) {
			int align = elfRead4(&pShdr->sh_addralign);
			if (align > 1) {
				if (arbitaryLoadAddr % align)
					arbitaryLoadAddr += align - (arbitaryLoadAddr % align);
			}
			sect.uNativeAddr = arbitaryLoadAddr;
			arbitaryLoadAddr += sect.uSectionSize;
		}
		sect.uType = elfRead4(&pShdr->sh_type);
		m_sh_link[i] = elfRead4(&pShdr->sh_link);
		m_sh_info[i] = elfRead4(&pShdr->sh_info);
		sect.uSectionEntrySize = elfRead4(&pShdr->sh_entsize);
		if (sect.uNativeAddr + sect.uSectionSize > next_extern)
			first_extern = next_extern = sect.uNativeAddr + sect.uSectionSize;
		if ((elfRead4(&pShdr->sh_flags) & SHF_WRITE) == 0)
			sect.bReadOnly = true;
		// Can't use the SHF_ALLOC bit to determine bss section; the bss section has SHF_ALLOC but also SHT_NOBITS.
		// (But many other sections, such as .comment, also have SHT_NOBITS). So for now, just use the name
		//if ((elfRead4(&pShdr->sh_flags) & SHF_ALLOC) == 0)
		if (sect.name == ".bss")
			sect.bBss = true;
		if (elfRead4(&pShdr->sh_flags) & SHF_EXECINSTR) {
			sect.bCode = true;
			bGotCode = true;            // We've got to a code section
		}
		// Deciding what is data and what is not is actually quite tricky but important.
		// For example, it's crucial to flag the .exception_ranges section as data, otherwise there is a "hole" in the
		// allocation map, that means that there is more than one "delta" from a read-only section to a page, and in the
		// end using -C results in a file that looks OK but when run just says "Killed".
		// So we use the Elf designations; it seems that ALLOC.!EXEC -> data
		// But we don't want sections before the .text section, like .interp, .hash, etc etc. Hence bGotCode.
		// NOTE: this ASSUMES that sections appear in a sensible order in the input binary file:
		// junk, code, rodata, data, bss
		if (bGotCode
		 && ((elfRead4(&pShdr->sh_flags) & (SHF_EXECINSTR | SHF_ALLOC)) == SHF_ALLOC)
		 && (elfRead4(&pShdr->sh_type) != SHT_NOBITS))
			sect.bData = true;
		sections.push_back(sect);
	}  // for each section

	// assign arbitary addresses to .rel.* sections too
	for (auto &sect : sections) {
		if (sect.uNativeAddr == 0 && sect.name.compare(0, 4, ".rel") == 0) {
			sect.uNativeAddr = arbitaryLoadAddr;
			arbitaryLoadAddr += sect.uSectionSize;
		}
	}

	// Add symbol info. Note that some symbols will be in the main table only, and others in the dynamic table only.
	// So the best idea is to add symbols for all sections of the appropriate type
	for (int i = 1; i < sections.size(); ++i) {
		unsigned uType = sections[i].uType;
		if (uType == SHT_SYMTAB || uType == SHT_DYNSYM)
			AddSyms(i);
#if 0  // Ick; bad logic. Done with fake library function pointers now (-2 .. -1024)
		if (uType == SHT_REL || uType == SHT_RELA)
			AddRelocsAsSyms(i);
#endif
	}

	// Save the relocation to symbol table info
	if (auto pRelA = getSectionInfoByName(".rela.text")) {
		m_bAddend = true;               // Remember its a relA table
		m_pReloc = (const Elf32_Rel *)pRelA->uHostAddr;  // Save pointer to reloc table
		//SetRelocInfo(pRelA);
	} else {
		m_bAddend = false;
		if (auto pRel = getSectionInfoByName(".rel.text")) {
			//SetRelocInfo(pRel);
			m_pReloc = (const Elf32_Rel *)pRel->uHostAddr;  // Save pointer to reloc table
		}
	}

	// Find the PLT limits. Required for isDynamicLinkedProc(), e.g.
	if (auto pPlt = getSectionInfoByName(".plt")) {
		m_uPltMin = pPlt->uNativeAddr;
		m_uPltMax = pPlt->uNativeAddr + pPlt->uSectionSize;
	}

	applyRelocations();

	return true;  // Success
}

/**
 * \brief Get string table entry.
 *
 * \param idx     Section index.  Section should be of type SHT_STRTAB.
 * \param offset  Offset (bytes) within section.
 *
 * \returns Pointer into the string table, or nullptr on error.
 */
const char *
ElfBinaryFile::getStrPtr(int idx, int offset) const
{
	if (idx < 0) {
		// Most commonly, this will be an index of -1, because a call to getSectionIndexByName() failed
		fprintf(stderr, "Error! getStrPtr passed index of %d\n", idx);
		return nullptr;
	}
	// Get a pointer to the start of the string table
	const char *pSym = sections[idx].uHostAddr;
	// Just add the offset
	return pSym + offset;
}

/**
 * Search the .rel[a].plt section for an entry with symbol table index i.
 * If found, return the native address of the associated PLT entry.
 *
 * A linear search will be needed.  However, starting at offset i and
 * searching backwards with wraparound should typically minimise the number of
 * entries to search.
 */
ADDRESS
ElfBinaryFile::findRelPltOffset(int i, const char *addrRelPlt, int sizeRelPlt, int numRelPlt, ADDRESS addrPlt) const
{
	int first = i;
	if (first >= numRelPlt)
		first = numRelPlt - 1;
	int curr = first;
	do {
		// Each entry is sizeRelPlt bytes, and will contain the offset, then the info (addend optionally follows)
		const int *pEntry = (const int *)(addrRelPlt + (curr * sizeRelPlt));
		int entry = elfRead4(pEntry + 1);  // Read pEntry[1]
		int sym = entry >> 8;              // The symbol index is in the top 24 bits (Elf32 only)
		if (sym == i) {
			// Found! Now we want the native address of the associated PLT entry.
			// For now, assume a size of 0x10 for each PLT entry, and assume that each entry in the .rel.plt section
			// corresponds exactly to an entry in the .plt (except there is one dummy .plt entry)
			return addrPlt + 0x10 * (curr + 1);
		}
		if (--curr < 0)
			curr = numRelPlt - 1;
	} while (curr != first);  // Will eventually wrap around to first if not present
	return 0;                 // Exit if this happens
}

/**
 * Add appropriate symbols to the symbol table.
 *
 * \param secIndex  The section index of the symbol table.
 */
void
ElfBinaryFile::AddSyms(int secIndex)
{
	int e_type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
	const auto &sect = sections[secIndex];
	// Calc number of symbols
	int nSyms = sect.uSectionSize / sect.uSectionEntrySize;
	m_pSym = (const Elf32_Sym *)sect.uHostAddr;  // Pointer to symbols
	int strIdx = m_sh_link[secIndex];  // sh_link points to the string table

	const SectionInfo *siPlt = getSectionInfoByName(".plt");
	ADDRESS addrPlt = siPlt ? siPlt->uNativeAddr : 0;
	const SectionInfo *siRelPlt = getSectionInfoByName(".rel.plt");
	int sizeRelPlt = 8;  // Size of each entry in the .rel.plt table
	if (!siRelPlt) {
		siRelPlt = getSectionInfoByName(".rela.plt");
		sizeRelPlt = 12;  // Size of each entry in the .rela.plt table is 12 bytes
	}
	const char *addrRelPlt = nullptr;
	int numRelPlt = 0;
	if (siRelPlt) {
		addrRelPlt = siRelPlt->uHostAddr;
		numRelPlt = sizeRelPlt ? siRelPlt->uSectionSize / sizeRelPlt : 0;
	}
	// Number of entries in the PLT:
	// int max_i_for_hack = siPlt ? (int)siPlt->uSectionSize / 0x10 : 0;
	// Index 0 is a dummy entry
	for (int i = 1; i < nSyms; ++i) {
		ADDRESS val = (ADDRESS)elfRead4((const int *)&m_pSym[i].st_value);
		const char *name = getStrPtr(strIdx, elfRead4(&m_pSym[i].st_name));
		if (!name || name[0] == '\0') continue;  // Silly symbols with no names

		// Hack off the "@@GLIBC_2.0" of Linux, if present
		std::string str(name);
		std::string::size_type pos = str.find("@@");
		if (pos != str.npos)
			str.erase(pos);

		// Ensure no overwriting (except functions)
		if (!m_SymTab.count(val) || ELF32_ST_TYPE(m_pSym[i].st_info) == STT_FUNC) {
			if (val == 0 && siPlt) { //&& i < max_i_for_hack) {
				// Special hack for gcc circa 3.3.3: (e.g. test/pentium/settest).  The value in the dynamic symbol table
				// is zero!  I was assuming that index i in the dynamic symbol table would always correspond to index i
				// in the .plt section, but for fedora2_true, this doesn't work. So we have to look in the .rel[a].plt
				// section. Thanks, gcc!  Note that this hack can cause strange symbol names to appear
				val = findRelPltOffset(i, addrRelPlt, sizeRelPlt, numRelPlt, addrPlt);
			} else if (e_type == E_REL) {
				int nsec = elfRead2(&m_pSym[i].st_shndx);
				if (nsec >= 0 && nsec < getNumSections())
					val += getSectionInfo(nsec)->uNativeAddr;
			}

#define ECHO_SYMS 0
#if     ECHO_SYMS
			std::cerr << "Elf AddSym: about to add " << str << " to address " << std::hex << val << std::dec << "\n";
#endif
			m_SymTab[val] = str;
		}
	}
	ADDRESS uMain = getMainEntryPoint();
	if (uMain != NO_ADDRESS && !m_SymTab.count(uMain)) {
		// Ugh - main mustn't have the STT_FUNC attribute. Add it
		m_SymTab[uMain] = "main";
	}
	return;
}

#if 0 // Cruft?
std::vector<ADDRESS>
ElfBinaryFile::getExportedAddresses(bool funcsOnly)
{
	std::vector<ADDRESS> exported;

	int secIndex = 0;
	for (int i = 1; i < sections.size(); ++i) {
		unsigned uType = sections[i].uType;
		if (uType == SHT_SYMTAB) {
			secIndex = i;
			break;
		}
	}
	if (secIndex == 0)
		return exported;

	int e_type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
	const auto &sect = sections[secIndex];
	// Calc number of symbols
	int nSyms = sect.uSectionSize / sect.uSectionEntrySize;
	m_pSym = (const Elf32_Sym *)sect.uHostAddr;  // Pointer to symbols
	int strIdx = m_sh_link[secIndex];  // sh_link points to the string table

	// Index 0 is a dummy entry
	for (int i = 1; i < nSyms; ++i) {
		ADDRESS val = (ADDRESS)elfRead4((const int *)&m_pSym[i].st_value);
		const char *name = getStrPtr(strIdx, elfRead4(&m_pSym[i].st_name));
		if (!name || name[0] == '\0') continue;  // Silly symbols with no names

		// Hack off the "@@GLIBC_2.0" of Linux, if present
		std::string str(name);
		std::string::size_type pos = str.find("@@");
		if (pos != str.npos)
			str.erase(pos);

		if (ELF32_ST_BIND(m_pSym[i].st_info) == STB_GLOBAL || ELF32_ST_BIND(m_pSym[i].st_info) == STB_WEAK) {
			if (!funcsOnly || ELF32_ST_TYPE(m_pSym[i].st_info) == STT_FUNC) {
				if (e_type == E_REL) {
					int nsec = elfRead2(&m_pSym[i].st_shndx);
					if (nsec >= 0 && nsec < getNumSections())
						val += getSectionInfo(nsec)->uNativeAddr;
				}
				exported.push_back(val);
			}
		}
	}
	return exported;
}
#endif

#if 0 // Cruft?
/**
 * FIXME:  This function is way off the rails.  It seems to always overwrite
 * the relocation entry with the 32 bit value from the symbol table.  Totally
 * invalid for SPARC, and most X86 relocations!  So currently not called.
 */
void
ElfBinaryFile::AddRelocsAsSyms(int relSecIdx)
{
	if (relSecIdx >= sections.size()) return;
	const auto &sect = sections[relSecIdx];
	// Calc number of relocations
	int nRelocs = sect.uSectionSize / sect.uSectionEntrySize;
	m_pReloc = (const Elf32_Rel *)sect.uHostAddr;  // Pointer to symbols
	int symSecIdx = m_sh_link[relSecIdx];
	int strSecIdx = m_sh_link[symSecIdx];

	// Index 0 is a dummy entry
	for (int i = 1; i < nRelocs; ++i) {
		ADDRESS val = (ADDRESS)elfRead4((const int *)&m_pReloc[i].r_offset);
		int symIndex = elfRead4(&m_pReloc[i].r_info) >> 8;
		int flags = elfRead4(&m_pReloc[i].r_info);
		if ((flags & 0xFF) == R_386_32) {
			// Lookup the value of the symbol table entry
			ADDRESS a = elfRead4((const int *)&m_pSym[symIndex].st_value);
			if (m_pSym[symIndex].st_info & STT_SECTION)
				a = getSectionInfo(elfRead2(&m_pSym[symIndex].st_shndx))->uNativeAddr;
			// Overwrite the relocation value... ?
			writeNative4(val, a);
			continue;
		}
		if ((flags & R_386_PC32) == 0)
			continue;
		const char *name = getStrPtr(strSecIdx, elfRead4(&m_pSym[symIndex].st_name));
		if (!name || name[0] == '\0') continue;  // Silly symbols with no names

		// Hack off the "@@GLIBC_2.0" of Linux, if present
		std::string str(name);
		std::string::size_type pos = str.find("@@");
		if (pos != str.npos)
			str.erase(pos);

		// Linear search!
		auto it = m_SymTab.begin();
		for (; it != m_SymTab.end(); ++it)
			if (it->second == str)
				break;
		// Add new extern
		if (it == m_SymTab.end()) {
			m_SymTab[next_extern] = str;
			it = m_SymTab.find(next_extern);
			next_extern += 4;
		}
		writeNative4(val, it->first - val - 4);
	}
	return;
}
#endif

/**
 * \note This function overrides a simple "return nullptr" function in the
 * base class (i.e. BinaryFile::getSymbolByAddress())
 */
const char *
ElfBinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	auto aa = m_SymTab.find(dwAddr);
	if (aa != m_SymTab.end())
		return aa->second.c_str();
	return nullptr;
}

bool
ElfBinaryFile::ValueByName(const std::string &name, SymValue *pVal, bool bNoTypeOK /* = false */) const
{
	const SectionInfo *pSect = getSectionInfoByName(".dynsym");
	if (!pSect) {
		// We have a file with no .dynsym section, and hence no .hash section (from my understanding - MVE).
		// It seems that the only alternative is to linearly search the symbol tables.
		// This must be one of the big reasons that linking is so slow! (at least, for statically linked files)
		// Note MVE: We can't use m_SymTab because we may need the size
		return SearchValueByName(name, pVal);
	}
	auto pSym = (const Elf32_Sym *)pSect->uHostAddr;
	if (!pSym) return false;

	pSect = getSectionInfoByName(".hash");
	if (!pSect) return false;
	auto pHash = (const int *)pSect->uHostAddr;

	int iStr = getSectionIndexByName(".dynstr");

	// First organise the hash table
	int numBucket = elfRead4(&pHash[0]);
	int numChain  = elfRead4(&pHash[1]);
	const int *pBuckets = &pHash[2];
	const int *pChains  = &pBuckets[numBucket];

	// Hash the symbol
	int hash = elf_hash(name.c_str()) % numBucket;
	int y = elfRead4(&pBuckets[hash]);  // Look it up in the bucket list
	// Beware of symbol tables with 0 in the buckets, e.g. libstdc++.
	// In that case, set found to false.
	bool found = false;
	while (y) {
		const char *pName = getStrPtr(iStr, elfRead4(&pSym[y].st_name));
		if (pName && pName == name) {
			found = true;
			break;
		}
		y = elfRead4(&pChains[y]);
	}
	// Beware of symbols with STT_NOTYPE, e.g. "open" in libstdc++ !
	// But sometimes "main" has the STT_NOTYPE attribute, so if bNoTypeOK is passed as true, return true
	if (found && (bNoTypeOK || (ELF32_ST_TYPE(pSym[y].st_info) != STT_NOTYPE))) {
		pVal->uSymAddr = elfRead4((const int *)&pSym[y].st_value);
		int e_type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
		if (e_type == E_REL) {
			int nsec = elfRead2(&pSym[y].st_shndx);
			if (nsec >= 0 && nsec < getNumSections())
				pVal->uSymAddr += getSectionInfo(nsec)->uNativeAddr;
		}
		pVal->iSymSize = elfRead4(&pSym[y].st_size);
		return true;
	} else {
		// We may as well do a linear search of the main symbol table. Some symbols (e.g. init_dummy) are
		// in the main symbol table, but not in the hash table
		return SearchValueByName(name, pVal);
	}
}

/**
 * Lookup the symbol table using linear searching.
 * See comments above for why this appears to be needed.
 */
bool
ElfBinaryFile::SearchValueByName(const std::string &name, SymValue *pVal, const std::string &sectName, const std::string &strName) const
{
	// Note: this assumes .symtab. Many files don't have this section!!!

	const SectionInfo *pSect = getSectionInfoByName(sectName);
	if (!pSect) return false;
	const SectionInfo *pStrSect = getSectionInfoByName(strName);
	if (!pStrSect) return false;
	const char *pStr = pStrSect->uHostAddr;
	// Find number of symbols
	int n = pSect->uSectionSize / pSect->uSectionEntrySize;
	const Elf32_Sym *pSym = (const Elf32_Sym *)pSect->uHostAddr;
	// Search all the symbols. It may be possible to start later than index 0
	for (int i = 0; i < n; ++i) {
		int idx = elfRead4(&pSym[i].st_name);
		if (name == pStr + idx) {
			// We have found the symbol
			pVal->uSymAddr = elfRead4((const int *)&pSym[i].st_value);
			int e_type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
			if (e_type == E_REL) {
				int nsec = elfRead2(&pSym[i].st_shndx);
				if (nsec >= 0 && nsec < getNumSections())
					pVal->uSymAddr += getSectionInfo(nsec)->uNativeAddr;
			}
			pVal->iSymSize = elfRead4(&pSym[i].st_size);
			return true;
		}
	}
	return false;  // Not found (this table)
}

/**
 * Search for the given symbol.  First search .symtab (if present); if not
 * found or the table has been stripped, search .dynstr.
 */
bool
ElfBinaryFile::SearchValueByName(const std::string &name, SymValue *pVal) const
{
	return SearchValueByName(name, pVal, ".symtab", ".strtab")
	    || SearchValueByName(name, pVal, ".dynsym", ".dynstr");
}


ADDRESS
ElfBinaryFile::getAddressByName(const std::string &name, bool bNoTypeOK) const
{
	SymValue Val;
	if (ValueByName(name, &Val, bNoTypeOK))
		return Val.uSymAddr;
	return NO_ADDRESS;
}

int
ElfBinaryFile::getSizeByName(const std::string &name, bool bNoTypeOK) const
{
	SymValue Val;
	if (ValueByName(name, &Val, bNoTypeOK))
		return Val.iSymSize;
	return 0;
}

#if 0 // Cruft?
/**
 * \brief Get the size associated with the symbol; guess if necessary.
 *
 * Guess the size of a function by finding the next symbol after it, and
 * subtracting the distance.
 *
 * \note This function is NOT efficient; it has to compare the closeness of
 * ALL symbols in the symbol table.
 */
int
ElfBinaryFile::getDistanceByName(const std::string &name, const std::string &sectName)
{
	if (int size = getSizeByName(name))
		return size;  // No need to guess!
	// No need to guess, but if there are fillers, then subtracting labels will give a better answer for coverage
	// purposes. For example, switch_cc. But some programs (e.g. switch_ps) have the switch tables between the
	// end of _start and main! So we are better off overall not trying to guess the size of _start
	unsigned value = getAddressByName(name);
	if (value == 0) return 0;  // Symbol doesn't even exist!

	const SectionInfo *pSect = getSectionInfoByName(sectName);
	if (!pSect) return 0;
	// Find number of symbols
	int n = pSect->uSectionSize / pSect->uSectionEntrySize;
	const Elf32_Sym *pSym = (const Elf32_Sym *)pSect->uHostAddr;
	// Search all the symbols. It may be possible to start later than index 0
	unsigned closest = 0xFFFFFFFF;
	int idx = -1;
	for (int i = 0; i < n; ++i) {
		if ((pSym[i].st_value > value) && (pSym[i].st_value < closest)) {
			idx = i;
			closest = pSym[i].st_value;
		}
	}
	if (idx == -1) return 0;
	// Do some checks on the symbol's value; it might be at the end of the .text section
	pSect = getSectionInfoByName(".text");
	ADDRESS low = pSect->uNativeAddr;
	ADDRESS hi = low + pSect->uSectionSize;
	if ((value >= low) && (value < hi)) {
		// Our symbol is in the .text section. Put a ceiling of the end of the section on closest.
		if (closest > hi) closest = hi;
	}
	return closest - value;
}

/**
 * \overload
 */
int
ElfBinaryFile::getDistanceByName(const std::string &name)
{
	if (int val = getDistanceByName(name, ".symtab"))
		return val;
	return getDistanceByName(name, ".dynsym");
}
#endif

bool
ElfBinaryFile::isDynamicLinkedProc(ADDRESS uNative) const
{
	if (uNative > (unsigned)-1024 && uNative != (unsigned)-1)
		return true;  // Say yes for fake library functions
	if (uNative >= first_extern && uNative < next_extern)
		return true;  // Yes for externs (not currently used)
	if (m_uPltMin == 0) return false;
	return (uNative >= m_uPltMin) && (uNative < m_uPltMax);  // Yes if a call to the PLT (false otherwise)
}

/**
 * Returns the entry point to main
 * (this should be a label in elf binaries generated by compilers).
 */
ADDRESS
ElfBinaryFile::getMainEntryPoint()
{
	return getAddressByName("main", true);
}

ADDRESS
ElfBinaryFile::getEntryPoint() const
{
	return (ADDRESS)elfRead4(&((Elf32_Ehdr *)m_pImage)->e_entry);
}

#if 0 // Cruft?
/**
 * FIXME:  The below assumes a fixed delta.
 */
const char *
ElfBinaryFile::NativeToHostAddress(ADDRESS uNative) const
{
	if (sections.size() == 0) return nullptr;
	return &sections[1].uHostAddr[uNative - sections[1].uNativeAddr];
}
#endif

#if 0 // Cruft?
ADDRESS
ElfBinaryFile::getRelocatedAddress(ADDRESS uNative)
{
	// Not implemented yet. But we need the function to make it all link
	return 0;
}
#endif

#if 0 // Cruft?
bool
ElfBinaryFile::PostLoad(void *handle)
{
	// This function is called after an archive member has been loaded by ElfArchiveFile

	// Save the elf pointer
	//m_elf = (Elf *)handle;

	//return ProcessElfFile();
	return false;
}
#endif

MACHINE
ElfBinaryFile::getMachine() const
{
	int machine = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_machine);
	if ((machine == EM_SPARC) || (machine == EM_SPARC32PLUS)) return MACHINE_SPARC;
	else if (machine == EM_386)     return MACHINE_PENTIUM;
	else if (machine == EM_PA_RISC) return MACHINE_HPRISC;
	else if (machine == EM_68K)     return MACHINE_PALM;  // Unlikely
	else if (machine == EM_PPC)     return MACHINE_PPC;
	else if (machine == EM_ST20)    return MACHINE_ST20;
	else if (machine == EM_MIPS)    return MACHINE_MIPS;
	else if (machine == EM_X86_64) {
		std::cerr << "Error: ElfBinaryFile::getMachine: The AMD x86-64 architecture is not supported yet\n";
		return (MACHINE)-1;
	}
	// An unknown machine type
	std::cerr << "Error: ElfBinaryFile::getMachine: Unsupported machine type: "
	          << machine << " (0x" << std::hex << machine << ")\n";
	std::cerr << "(Please add a description for this type, thanks!)\n";
	return (MACHINE)-1;
}

#if 0 // Cruft?
std::list<const char *>
ElfBinaryFile::getDependencyList() const
{
	std::list<const char *> result;
	const SectionInfo *dynsect = getSectionInfoByName(".dynamic");
	if (!dynsect)
		return result; /* no dynamic section = statically linked */

	ADDRESS strtab = NO_ADDRESS;
	for (const Elf32_Dyn *dyn = (const Elf32_Dyn *)dynsect->uHostAddr; dyn->d_tag != DT_NULL; ++dyn) {
		if (dyn->d_tag == DT_STRTAB) {
			strtab = (ADDRESS)dyn->d_un.d_ptr;
			break;
		}
	}
	if (strtab == NO_ADDRESS) /* No string table = no names */
		return result;

	const char *stringtab = NativeToHostAddress(strtab);
	for (const Elf32_Dyn *dyn = (const Elf32_Dyn *)dynsect->uHostAddr; dyn->d_tag != DT_NULL; ++dyn) {
		if (dyn->d_tag == DT_NEEDED) {
			if (const char *need = stringtab + dyn->d_un.d_val)
				result.push_back(need);
		}
	}
	return result;
}
#endif

#if 0 // Cruft?
bool
ElfBinaryFile::isLibrary() const
{
	int type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
	return (type == ET_DYN);
}

ADDRESS
ElfBinaryFile::getImageBase() const
{
	return m_uBaseAddr;
}

size_t
ElfBinaryFile::getImageSize() const
{
	return m_uImageSize;
}
#endif

#if 0 // Cruft?
/**
 * \brief Get an array of addresses of imported function stubs.
 *
 * This function relies on the fact that the symbols are sorted by address,
 * and that Elf PLT entries have successive addresses beginning soon after
 * m_PltMin.
 *
 * \param numImports  Reference to integer set to the number of these.
 *
 * \returns An array of native ADDRESSes.
 */
ADDRESS *
ElfBinaryFile::getImportStubs(int &numImports)
{
	ADDRESS a = m_uPltMin;
	int n = 0;
	auto aa = m_SymTab.find(a);
	auto ff = aa;
	bool delDummy = false;
	if (aa == m_SymTab.end()) {
		// Need to insert a dummy entry at m_uPltMin
		delDummy = true;
		m_SymTab[a] = std::string();
		ff = m_SymTab.find(a);
		aa = ff;
		++aa;
	}
	while ((aa != m_SymTab.end()) && (a < m_uPltMax)) {
		++n;
		a = aa->first;
		++aa;
	}
	// Allocate an array of ADDRESSESes
	m_pImportStubs = new ADDRESS[n];
	aa = ff;  // Start at first
	a = aa->first;
	int i = 0;
	while ((aa != m_SymTab.end()) && (a < m_uPltMax)) {
		m_pImportStubs[i++] = a;
		a = aa->first;
		++aa;
	}
	if (delDummy)
		m_SymTab.erase(ff);  // Delete dummy entry
	numImports = n;
	return m_pImportStubs;
}
#endif

#if 0 // Cruft?
std::map<ADDRESS, const char *> *
ElfBinaryFile::getDynamicGlobalMap()
{
	auto ret = new std::map<ADDRESS, const char *>;

	const SectionInfo *pSect = getSectionInfoByName(".rel.bss");
	if (!pSect) pSect = getSectionInfoByName(".rela.bss");
	if (!pSect) {
		// This could easily mean that this file has no dynamic globals, and
		// that is fine.
		return ret;
	}
	const char *p = pSect->uHostAddr;
	int numEnt = pSect->uSectionSize / pSect->uSectionEntrySize;

	const SectionInfo *sym = getSectionInfoByName(".dynsym");
	if (!sym) {
		fprintf(stderr, "Could not find section .dynsym in source binary file");
		return ret;
	}
	const Elf32_Sym *pSym = (const Elf32_Sym *)sym->uHostAddr;

	int idxStr = getSectionIndexByName(".dynstr");
	if (idxStr == -1) {
		fprintf(stderr, "Could not find section .dynstr in source binary file");
		return ret;
	}

	for (int i = 0; i < numEnt; ++i) {
		// The ugly p[1] below is because it p might point to an Elf32_Rela struct, or an Elf32_Rel struct
		int sym = ELF32_R_SYM(((const int *)p)[1]);
		const char *s = getStrPtr(idxStr, pSym[sym].st_name);
		ADDRESS val = ((const int *)p)[0];
		(*ret)[val] = s;  // Add the (val, s) mapping to ret
		p += pSect->uSectionEntrySize;
	}

	return ret;
}
#endif

/**
 * \brief Read a short with endianness care.
 *
 * Read a 2 byte quantity from host address (C pointer) p.
 *
 * \note Takes care of reading the correct endianness.
 *
 * \param ps  Host pointer to the data.
 *
 * \returns An integer representing the data.
 */
int
ElfBinaryFile::elfRead2(const short *ps) const
{
	return bigendian ? (int)BH16(ps) : (int)LH16(ps);
}

/**
 * \brief Read an int with endianness care.
 *
 * Read a 4 byte quantity from host address (C pointer) p
 *
 * \note Takes care of reading the correct endianness.
 *
 * \param pi  Host pointer to the data.
 *
 * \returns An integer representing the data.
 */
int
ElfBinaryFile::elfRead4(const int *pi) const
{
	return bigendian ? (int)BH32(pi) : (int)LH32(pi);
}

/**
 * \brief Write an int with endianness care.
 */
void
ElfBinaryFile::elfWrite4(int *pi, int val)
{
	char *p = (char *)pi;
	if (bigendian) {
		*p++ = (char)(val >> 24);
		*p++ = (char)(val >> 16);
		*p++ = (char)(val >>  8);
		*p   = (char)val;
	} else {
		*p++ = (char)val;
		*p++ = (char)(val >>  8);
		*p++ = (char)(val >> 16);
		*p   = (char)(val >> 24);
	}
}

#if 0 // Cruft?
void
ElfBinaryFile::writeNative4(ADDRESS nat, unsigned int n)
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return;
	char *host = &si->uHostAddr[nat - si->uNativeAddr];
	if (bigendian) {
		*(unsigned char *)host       = (n >> 24) & 0xff;
		*(unsigned char *)(host + 1) = (n >> 16) & 0xff;
		*(unsigned char *)(host + 2) = (n >>  8) & 0xff;
		*(unsigned char *)(host + 3) =  n        & 0xff;
	} else {
		*(unsigned char *)(host + 3) = (n >> 24) & 0xff;
		*(unsigned char *)(host + 2) = (n >> 16) & 0xff;
		*(unsigned char *)(host + 1) = (n >>  8) & 0xff;
		*(unsigned char *)host       =  n        & 0xff;
	}
}
#endif

/**
 * Apply relocations; important when the input program
 * is not compiled with -fPIC.
 */
void
ElfBinaryFile::applyRelocations()
{
	int nextFakeLibAddr = -2;  // See R_386_PC32 below; -1 sometimes used for main
	if (!m_pImage) return;  // No file loaded
	int machine = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_machine);
	int e_type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
	switch (machine) {
	case EM_SPARC:
		break;  // Not implemented yet
	case EM_386:
		{
			for (int i = 1; i < sections.size(); ++i) {
				const auto &sect = sections[i];
				if (sect.uType == SHT_REL) {
					// A section such as .rel.dyn or .rel.plt (without an addend field).
					// Each entry has 2 words: r_offet and r_info. The r_offset is just the offset from the beginning
					// of the section (section given by the section header's sh_info) to the word to be modified.
					// r_info has the type in the bottom byte, and a symbol table index in the top 3 bytes.
					// A symbol table offset of 0 (STN_UNDEF) means use value 0. The symbol table involved comes from
					// the section header's sh_link field.
					const int *pReloc = (const int *)sect.uHostAddr;
					unsigned size = sect.uSectionSize;
					// NOTE: the r_offset is different for .o files (E_REL in the e_type header field) than for exe's
					// and shared objects!
					ADDRESS destNatOrigin = 0;
					char *destHostOrigin = nullptr;
					if (e_type == E_REL) {
						int destSection = m_sh_info[i];
						destNatOrigin  = sections[destSection].uNativeAddr;
						destHostOrigin = sections[destSection].uHostAddr;
					}
					int symSection = m_sh_link[i];          // Section index for the associated symbol table
					int strSection = m_sh_link[symSection]; // Section index for the string section assoc with this
					const char *pStrSection = (const char *)sections[strSection].uHostAddr;
					const Elf32_Sym *symOrigin = (const Elf32_Sym *)sections[symSection].uHostAddr;
					for (unsigned u = 0; u < size; u += 2 * sizeof (unsigned)) {
						unsigned r_offset = elfRead4(pReloc++);
						unsigned info = elfRead4(pReloc++);
						unsigned char relType = (unsigned char)info;
						unsigned symTabIndex = info >> 8;
						int *pRelWord;  // Pointer to the word to be relocated
						if (e_type == E_REL)
							pRelWord = ((int *)(destHostOrigin + r_offset));
						else {
							const SectionInfo *destSec = getSectionInfoByAddr(r_offset);
							pRelWord = (int *)(destSec->uHostAddr - destSec->uNativeAddr + r_offset);
							destNatOrigin = 0;
						}
						ADDRESS A, S = 0, P;
						switch (relType) {
						case 0:  // R_386_NONE: just ignore (common)
							break;
						case 1:  // R_386_32: S + A
							S = elfRead4((const int *)&symOrigin[symTabIndex].st_value);
							if (e_type == E_REL) {
								int nsec = elfRead2(&symOrigin[symTabIndex].st_shndx);
								if (nsec >= 0 && nsec < getNumSections())
									S += getSectionInfo(nsec)->uNativeAddr;
							}
							A = elfRead4(pRelWord);
							elfWrite4(pRelWord, S + A);
							break;
						case 2:  // R_386_PC32: S + A - P
							if (ELF32_ST_TYPE(symOrigin[symTabIndex].st_info) == STT_SECTION) {
								int nsec = elfRead2(&symOrigin[symTabIndex].st_shndx);
								if (nsec >= 0 && nsec < getNumSections())
									S = getSectionInfo(nsec)->uNativeAddr;
							} else {
								S = elfRead4((const int *)&symOrigin[symTabIndex].st_value);
								if (S == 0) {
									// This means that the symbol doesn't exist in this module, and is not accessed
									// through the PLT, i.e. it will be statically linked, e.g. strcmp. We have the
									// name of the symbol right here in the symbol table entry, but the only way
									// to communicate with the loader is through the target address of the call.
									// So we use some very improbable addresses (e.g. -1, -2, etc) and give them entries
									// in the symbol table
									int nameOffset = elfRead4(&symOrigin[symTabIndex].st_name);
									const char *pName = pStrSection + nameOffset;
									// this is too slow, I'm just going to assume it is 0
									//S = getAddressByName(pName);
									//if (S == (e_type == E_REL ? 0x8000000 : 0)) {
										S = nextFakeLibAddr--;  // Allocate a new fake address
										addSymbol(S, pName);
									//}
								} else if (e_type == E_REL) {
									int nsec = elfRead2(&symOrigin[symTabIndex].st_shndx);
									if (nsec >= 0 && nsec < getNumSections())
										S += getSectionInfo(nsec)->uNativeAddr;
								}
							}
							A = elfRead4(pRelWord);
							P = destNatOrigin + r_offset;
							elfWrite4(pRelWord, S + A - P);
							break;
						case 7:
						case 8:  // R_386_RELATIVE
							break;  // No need to do anything with these, if a shared object
						default:
							// std::cout << "Relocation type " << (int)relType << " not handled yet\n";
							;
						}
					}
				}
			}
		}
	default:
		break;  // Not implemented
	}
}

bool
ElfBinaryFile::isRelocationAt(ADDRESS uNative) const
{
	//int nextFakeLibAddr = -2;  // See R_386_PC32 below; -1 sometimes used for main
	if (!m_pImage) return false;  // No file loaded
	int machine = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_machine);
	int e_type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
	switch (machine) {
	case EM_SPARC:
		break;  // Not implemented yet
	case EM_386:
		{
			for (int i = 1; i < sections.size(); ++i) {
				const auto &sect = sections[i];
				if (sect.uType == SHT_REL) {
					// A section such as .rel.dyn or .rel.plt (without an addend field).
					// Each entry has 2 words: r_offet and r_info. The r_offset is just the offset from the beginning
					// of the section (section given by the section header's sh_info) to the word to be modified.
					// r_info has the type in the bottom byte, and a symbol table index in the top 3 bytes.
					// A symbol table offset of 0 (STN_UNDEF) means use value 0. The symbol table involved comes from
					// the section header's sh_link field.
					const int *pReloc = (const int *)sect.uHostAddr;
					unsigned size = sect.uSectionSize;
					// NOTE: the r_offset is different for .o files (E_REL in the e_type header field) than for exe's
					// and shared objects!
					ADDRESS destNatOrigin = 0;
					//char *destHostOrigin;
					if (e_type == E_REL) {
						int destSection = m_sh_info[i];
						destNatOrigin  = sections[destSection].uNativeAddr;
						//destHostOrigin = sections[destSection].uHostAddr;
					}
					//int symSection = m_sh_link[i];          // Section index for the associated symbol table
					//int strSection = m_sh_link[symSection]; // Section index for the string section assoc with this
					//const char *pStrSection = (const char *)sections[strSection].uHostAddr;
					//const Elf32_Sym *symOrigin = (const Elf32_Sym *)sections[symSection].uHostAddr;
					for (unsigned u = 0; u < size; u += 2 * sizeof (unsigned)) {
						unsigned r_offset = elfRead4(pReloc++);
						//unsigned info = elfRead4(pReloc);
						++pReloc;
						//unsigned char relType = (unsigned char)info;
						//unsigned symTabIndex = info >> 8;
						ADDRESS pRelWord;  // Pointer to the word to be relocated
						if (e_type == E_REL)
							pRelWord = destNatOrigin + r_offset;
						else {
							const SectionInfo *destSec = getSectionInfoByAddr(r_offset);
							pRelWord = destSec->uNativeAddr + r_offset;
							destNatOrigin = 0;
						}
						if (uNative == pRelWord)
							return true;
					}
				}
			}
		}
	default:
		break;  // Not implemented
	}
	return false;
}

/**
 * Given a symbol by name, try to find the corresponding symbol of the source
 * code file name.
 *
 * \param sym  Symbol name.
 *
 * \returns Pointer into a string table, pointing to a filename, or nullptr if
 * not found or if it is a zero-length string.
 */
const char *
ElfBinaryFile::getFilenameSymbolFor(const std::string &sym)
{
	int secIndex = 0;
	for (int i = 1; i < sections.size(); ++i) {
		unsigned uType = sections[i].uType;
		if (uType == SHT_SYMTAB) {
			secIndex = i;
			break;
		}
	}
	if (secIndex == 0)
		return nullptr;

	//int e_type = elfRead2(&((Elf32_Ehdr *)m_pImage)->e_type);
	const auto &sect = sections[secIndex];
	// Calc number of symbols
	int nSyms = sect.uSectionSize / sect.uSectionEntrySize;
	m_pSym = (const Elf32_Sym *)sect.uHostAddr;  // Pointer to symbols
	int strIdx = m_sh_link[secIndex];  // sh_link points to the string table

	const char *filename = nullptr;

	// Index 0 is a dummy entry
	for (int i = 1; i < nSyms; ++i) {
		//ADDRESS val = (ADDRESS)elfRead4((const int *)&m_pSym[i].st_value);
		const char *name = getStrPtr(strIdx, elfRead4(&m_pSym[i].st_name));
		if (!name || name[0] == '\0') continue;  // Silly symbols with no names

		if (ELF32_ST_TYPE(m_pSym[i].st_info) == STT_FILE) {
			filename = name;
			continue;
		}

		// Hack off the "@@GLIBC_2.0" of Linux, if present
		std::string str(name);
		std::string::size_type pos = str.find("@@");
		if (pos != str.npos)
			str.erase(pos);

		if (str == sym)
			return filename;
	}
	return nullptr;
}

/**
 * A map for extra symbols, those not in the usual Elf symbol tables.
 */
void
ElfBinaryFile::addSymbol(ADDRESS uNative, const std::string &name)
{
	m_SymTab[uNative] = name;
}

#if 0 // Cruft?
/**
 * \brief For debugging.
 */
void
ElfBinaryFile::dumpSymbols() const
{
	std::cerr << std::hex;
	for (const auto &sym : m_SymTab)
		std::cerr << "0x" << sym.first << " " << sym.second << "        ";
	std::cerr << std::dec << "\n";
}
#endif

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
	return new ElfBinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (ElfBinaryFile *)bf;
}
#endif
