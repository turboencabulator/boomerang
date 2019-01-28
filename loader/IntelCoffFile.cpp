/**
 * \file
 * \brief Contains the implementation of the class IntelCoffFile.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "IntelCoffFile.h"

#include <cassert>
#include <cstdio>
#include <cstring>

/**
 * Segment information, 40 bytes
 */
struct __attribute__((packed)) struc_coff_sect {
	char     sch_sectname[8];
	uint32_t sch_physaddr;
	uint32_t sch_virtaddr;
	uint32_t sch_sectsize;
	uint32_t sch_sectptr;
	uint32_t sch_relptr;
	uint32_t sch_lineno_ptr;
	uint16_t sch_nreloc;
	uint16_t sch_nlineno;
	uint32_t sch_flags;
};

/**
 * Symbol information, 18 bytes
 */
struct __attribute__((packed)) coff_symbol {
	union {
		struct {
			uint32_t zeros;
			uint32_t offset;
		} e;
		char name[8];
	} e;
#define csym_name       e.name
#define csym_zeros      e.e.zeros
#define csym_offset     e.e.offset

	uint32_t csym_value;
	uint16_t csym_sectnum;
#define N_UNDEF 0

	uint16_t csym_type;
#define T_FUNC  0x20

	unsigned char csym_loadclass;
	unsigned char csym_numaux;
};

struct __attribute__((packed)) struct_coff_rel {
	uint32_t r_vaddr;
	uint32_t r_symndx;
	uint16_t r_type;
#define RELOC_ADDR32    6
#define RELOC_REL32     20
};


IntelCoffFile::IntelCoffFile()
{
	bigendian = false;
}

bool
IntelCoffFile::load(std::istream &ifs)
{
	printf("IntelCoffFile::load() called\n");

	ifs.read((char *)&m_Header, sizeof m_Header);
	if (!ifs.good())
		return false;

	printf("Read COFF header\n");

	// Skip the optional header, if present
	ifs.seekg(m_Header.coff_opthead_size, ifs.cur);

	auto psh = new struct struc_coff_sect[m_Header.coff_sections];
	ifs.read((char *)psh, sizeof *psh * m_Header.coff_sections);
	if (!ifs.good()) {
		delete [] psh;
		return false;
	}
	for (int iSection = 0; iSection < m_Header.coff_sections; ++iSection) {
		//assert(0 == psh[iSection].sch_virtaddr);
		//assert(0 == psh[iSection].sch_physaddr);

		auto sectname = std::string(psh[iSection].sch_sectname, sizeof psh[iSection].sch_sectname);
		auto len = sectname.find('\0');
		if (len != sectname.npos)
			sectname.erase(len);

		size_t sidx;
		if (auto si = getSectionInfoByName(sectname)) {
			sidx = si - &sections.front();
		} else {
			sidx = sections.size();
			auto sect = SectionInfo();
			sect.name      = sectname;
			sect.bCode     = 0 != (psh[iSection].sch_flags &   0x20);
			sect.bData     = 0 != (psh[iSection].sch_flags &   0x40);
			sect.bBss      = 0 != (psh[iSection].sch_flags &   0x80);
			sect.bReadOnly = 0 != (psh[iSection].sch_flags & 0x1000);
			sections.push_back(sect);
		}

		auto &sect = sections[sidx];
		psh[iSection].sch_virtaddr = sect.uSectionSize;
		psh[iSection].sch_physaddr = sidx;
		sect.uSectionSize += psh[iSection].sch_sectsize;
	}
	printf("Loaded %d section headers\n", (int)m_Header.coff_sections);

	ADDRESS a = 0x40000000;
	for (auto &sect : sections) {
		if (sect.uSectionSize > 0) {
			auto pData = new char[sect.uSectionSize];
			sect.uHostAddr = pData;
			sect.uNativeAddr = a;
			a += sect.uSectionSize;
		}
	}
	printf("Allocated %d segments. a=%08x", sections.size(), a);

	for (int iSection = 0; iSection < m_Header.coff_sections; ++iSection) {
		printf("Loading section %d of %hd\n", iSection + 1, m_Header.coff_sections);

		const SectionInfo *psi = getSectionInfo(psh[iSection].sch_physaddr);

		char *pData = psi->uHostAddr + psh[iSection].sch_virtaddr;
		if (!(psh[iSection].sch_flags & 0x80)) {
			ifs.seekg(psh[iSection].sch_sectptr);
			ifs.read(pData, psh[iSection].sch_sectsize);
			if (!ifs.good())
				return false;
		}
	}

	// Load the symbol table
	printf("Load symbol table\n");
	auto pSymbols = new struct coff_symbol[m_Header.coff_num_syment];
	ifs.seekg(m_Header.coff_symtab_ofs);
	ifs.read((char *)pSymbols, sizeof *pSymbols * m_Header.coff_num_syment);
	if (!ifs.good())
		return false;

	// TODO: Groesse des Abschnittes vorher bestimmen
	auto pStrings = new char[0x8000];
	ifs.read(pStrings, 0x8000);


	// Run the symbol table
	ADDRESS fakeForImport = (ADDRESS)0xfffe0000;

	printf("Size of one symbol: %u\n", sizeof *pSymbols);
	for (unsigned int iSym = 0; iSym < m_Header.coff_num_syment; iSym += pSymbols[iSym].csym_numaux + 1) {
		char tmp_name[9]; tmp_name[8] = 0;
		std::string name;
		if (pSymbols[iSym].csym_zeros == 0) {
			// TODO: the symbol is found in a string table behind the symbol table at offset csym_offset
			//snprintf(tmp_name, 8, "n%07lx", pSymbols[iSym].csym_offset);
			name = pStrings + pSymbols[iSym].csym_offset;
		} else {
			strncpy(tmp_name, pSymbols[iSym].csym_name, 8);
			name = tmp_name;
		}

		if (!(pSymbols[iSym].csym_loadclass & 0x60) && (pSymbols[iSym].csym_sectnum <= m_Header.coff_sections)) {
			if (pSymbols[iSym].csym_sectnum > 0) {
				const SectionInfo *psi = getSectionInfo(psh[pSymbols[iSym].csym_sectnum - 1].sch_physaddr);
				pSymbols[iSym].csym_value += psh[pSymbols[iSym].csym_sectnum - 1].sch_virtaddr + psi->uNativeAddr;
				if (name != ".strip.")
					m_Symbols.Add(pSymbols[iSym].csym_value, name);
				if (pSymbols[iSym].csym_type & 0x20 && psi->bCode) {
					m_EntryPoints.push_back(pSymbols[iSym].csym_value);
					//printf("Made '%s' an entry point.\n", name.c_str());
				}
			} else {
				if (pSymbols[iSym].csym_type & 0x20) {
					pSymbols[iSym].csym_value = fakeForImport; // TODO: external reference
					fakeForImport -= 0x10000;
					m_Symbols.Add(pSymbols[iSym].csym_value, name);
				} else if (pSymbols[iSym].csym_value != 0) {
					assert(false); //pSymbols[iSym].csym_value = ield_1C->SetName(var_8, 0, this, field_4[var_4].csym_value);
				} else {
					pSymbols[iSym].csym_value = fakeForImport; // TODO: external reference
					fakeForImport -= 0x10000;
					m_Symbols.Add(pSymbols[iSym].csym_value, name);
				}
			}

		}
		printf("Symbol %d: %s %08lx\n", iSym, name.c_str(), pSymbols[iSym].csym_value);
	}

	for (int iSection = 0; iSection < m_Header.coff_sections; ++iSection) {
		//printf("Relocating section %d of %hd\n", iSection + 1, m_Header.coff_sections);
		const SectionInfo *psi = getSectionInfo(psh[iSection].sch_physaddr);
		char *pData = psi->uHostAddr + psh[iSection].sch_virtaddr;

		if (!psh[iSection].sch_nreloc) continue;

		//printf("Relocation table at %08lx\n", psh[iSection].sch_relptr);
		auto pRel = new struct struct_coff_rel[psh[iSection].sch_nreloc];
		ifs.seekg(psh[iSection].sch_relptr);
		ifs.read((char *)pRel, sizeof *pRel * psh[iSection].sch_nreloc);
		if (!ifs.good())
			return false;

		for (int iReloc = 0; iReloc < psh[iSection].sch_nreloc; ++iReloc) {
			struct struct_coff_rel *tRel = pRel + iReloc;
			struct coff_symbol *pSym = pSymbols + tRel->r_symndx;
			uint32_t *pPatch = (uint32_t *)(pData + tRel->r_vaddr);
			//printf("Relocating at %08lx: type %d, dest %08lx\n", tRel->r_vaddr + psi->uNativeAddr + psh[iSection].sch_virtaddr, (int)tRel->r_type, pSym->csym_value);

			switch (tRel->r_type) {
			case RELOC_ADDR32:
			case RELOC_ADDR32 + 1:
				// TODO: Handle external references
				//printf("Relocating at %08lx absulute to %08lx\n", tRel->r_vaddr + psi->uNativeAddr + psh[iSection].sch_virtaddr, pSym->csym_value);
				*pPatch += pSym->csym_value;
				m_Relocations.push_back(tRel->r_vaddr);
				break;

			case RELOC_REL32:
				// TODO: Handle external references
				//printf("Relocating at %08lx relative to %08lx\n", tRel->r_vaddr + psi->uNativeAddr + psh[iSection].sch_virtaddr, pSym->csym_value);
				//printf("Value before relocation: %08lx\n", *pPatch);
				*pPatch += pSym->csym_value - (unsigned long)(tRel->r_vaddr + psi->uNativeAddr + psh[iSection].sch_virtaddr + 4);
				//printf("Value after relocation: %08lx\n", *pPatch);
				m_Relocations.push_back(tRel->r_vaddr);
				break;
			}
		}

		delete [] pRel;

#if 0
		if (iSection == 0) {
			for (int i = 0; i < psh[iSection].sch_sectsize; i += 8) {
				printf("%08x", i);
				for (int j = 0; j < 8; ++j)
					printf(" %02x", pData[i + j] & 0xff);
				printf("\n");
			}
		}
#endif
	}

	// TODO: Perform relocation
	// TODO: Define symbols (internal, exported, imported)

	return true;
}

#if 0 // Cruft?
bool
IntelCoffFile::PostLoad(void *)
{
	// There seems to be no need to implement this since one file is loaded ever.
	printf("IntelCoffFile::PostLoad called\n");
	return false;
}
#endif

#if 0 // Cruft?
bool
IntelCoffFile::isLibrary() const
{
	printf("IntelCoffFile::isLibrary called\n");
	return false;
}

ADDRESS
IntelCoffFile::getImageBase() const
{
	// TODO: Do they really always start at 0?
	return (ADDRESS)0;
}

size_t
IntelCoffFile::getImageSize() const
{
	printf("IntelCoffFile::getImageSize called\n");
	// TODO: Implement it. We will have to load complete before knowing the size
	return 0;
}
#endif

ADDRESS
IntelCoffFile::getMainEntryPoint()
{
	printf("IntelCoffFile::getMainEntryPoint called\n");
	// There is no such thing, but we need to deliver one since the first entry point might
	// be zero and this is skipped when returned by getEntryPoint().
	//return NO_ADDRESS;
	return getEntryPoint();
}

ADDRESS
IntelCoffFile::getEntryPoint() const
{
	printf("IntelCoffFile::getEntryPoint called\n");
	// There is no such thing, but we have to deliver one
	if (m_EntryPoints.empty())
		return NO_ADDRESS;

	printf("IntelCoffFile::getEntryPoint atleast one entry point exists\n");
	printf("IntelCoffFile::getEntryPoint returning %08x\n", m_EntryPoints.front());
	return m_EntryPoints.front();
}

const char *
IntelCoffFile::getSymbolByAddress(ADDRESS dwAddr)
{
	return m_Symbols.find(dwAddr);
}

bool
IntelCoffFile::isDynamicLinkedProc(ADDRESS uNative) const
{
	// Say yes for fake library functions
	return uNative >= (unsigned)0xc0000000;
}

bool
IntelCoffFile::isRelocationAt(ADDRESS uNative) const
{
	for (const auto &reloc : m_Relocations) {
		if (reloc == uNative) {
			return true;
		}
	}
	return false;
}

const std::map<ADDRESS, std::string> &
IntelCoffFile::getSymbols() const
{
	return m_Symbols.getAll();
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
	return new IntelCoffFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (IntelCoffFile *)bf;
}
#endif
