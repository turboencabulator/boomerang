/**
 * \file
 * \brief Contains the implementation of the class Win32BinaryFile.
 *
 * \authors
 * Copyright (C) 2000, The University of Queensland
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

#include "Win32BinaryFile.h"

#include <iostream>
#include <sstream>

#include <cassert>
#include <cstdio>
#include <cstring>

extern "C" size_t microX86Dis(const unsigned char *p);  // From microX86dis.c


#ifndef IMAGE_SCN_CNT_CODE // Assume that if one is not defined, the rest isn't either.
#define IMAGE_SCN_CNT_CODE               0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA   0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_READ               0x40000000
#define IMAGE_SCN_MEM_WRITE              0x80000000
#endif


namespace {

/**
 * Due to the current rigid design, where BinaryFile holds a C-style array of
 * SectionInfo's, we can't extend a subclass of SectionInfo with the data
 * required to express the semantics of a PE section. We therefore need this
 * external mapping from SectionInfo's to PEObject's, that contain the info we
 * need.
 *
 * \todo Refactor BinaryFile to not expose its private parts in public. Design
 * both a protected (for subclasses) and public (for users) interface.
 */
typedef std::map<const class PESectionInfo *, const PEObject *> SectionObjectMap;

SectionObjectMap s_sectionObjects;


/**
 * Note that PESectionInfo currently must be the exact same size as
 * SectionInfo due to the already mentioned array held by BinaryFile.
 */
class PESectionInfo : public SectionInfo {
	bool isAddressBss(ADDRESS a) const override {
		if (a < uNativeAddr || a >= uNativeAddr + uSectionSize) {
			return false; // not even within this section
		}
		if (bBss) {
			return true; // obvious
		}
		if (bReadOnly) {
			return false; // R/O BSS makes no sense.
		}
		// Don't check for bData here. So long as the section has slack at end, that space can contain BSS.
		auto it = s_sectionObjects.find(this);
		assert(it != s_sectionObjects.end());
		assert(it->second);
		assert(this == it->first);
		const PEObject *sectionHeader = it->second;
		const bool has_slack = LMMH(sectionHeader->VirtualSize) > LMMH(sectionHeader->PhysicalSize);
		if (!has_slack) {
			return false; // BSS not possible.
		}
		if (a >= uNativeAddr + LMMH(sectionHeader->PhysicalSize)) {
			return true;
		}
		return false;
	}
};

static_assert(sizeof (SectionInfo) == sizeof (PESectionInfo), "PESectionInfo size mismatch");

}


Win32BinaryFile::Win32BinaryFile()
{
}

Win32BinaryFile::~Win32BinaryFile()
{
	for (int i = 0; i < m_iNumSections; ++i)
		delete [] m_pSections[i].pSectionName;
	delete [] m_pSections;
	delete [] base;
}

ADDRESS
Win32BinaryFile::getEntryPoint() const
{
	return (ADDRESS)(LMMH(m_pPEHeader->EntrypointRVA)
	               + LMMH(m_pPEHeader->Imagebase));
}

/**
 * This is a bit of a hack, but no more than the rest of Windows :-O
 *
 * The pattern is to look for an indirect call (FF 15 opcode) to exit; within
 * 10 instructions before that should be the call to WinMain (with no other
 * calls inbetween).  This pattern should work for "old style" and "new style"
 * PE executables, as well as console mode PE files.
 */
ADDRESS
Win32BinaryFile::getMainEntryPoint()
{
	ADDRESS aMain = getAddressByName("main", true);
	if (aMain != NO_ADDRESS)
		return aMain;
	aMain = getAddressByName("_main", true);  // Example: MinGW
	if (aMain != NO_ADDRESS)
		return aMain;

	// Start at program entry point
	unsigned p = LMMH(m_pPEHeader->EntrypointRVA);
	unsigned lim = p + 0x200;
	unsigned lastOrdCall = 0;
	int gap;               // Number of instructions from the last ordinary call
	int borlandState = 0;  // State machine for Borland

	const SectionInfo *si = getSectionInfoByName(".text");
	if (!si) si = getSectionInfoByName("CODE");
	assert(si);
	unsigned textSize = si->uSectionSize;
	if (textSize < 0x200)
		lim = p + textSize;

	if (m_pPEHeader->Subsystem == 1)  // native
		return LMMH(m_pPEHeader->EntrypointRVA) + LMMH(m_pPEHeader->Imagebase);

	gap = 0xF0000000;  // Large positive number (in case no ordinary calls)
	while (p < lim) {
		unsigned char op1 = *(p + base);
		unsigned char op2 = *(p + base + 1);
		//std::cerr << std::hex << "At " << p << ", ops " << (unsigned)op1 << ", " << (unsigned)op2 << std::dec << "\n";
		switch (op1) {
		case 0xE8:
			{
				// An ordinary call; this could be to winmain/main
				lastOrdCall = p;
				gap = 0;
				if (borlandState == 1)
					++borlandState;
				else
					borlandState = 0;
			}
			break;
		case 0xFF:
			if (op2 == 0x15) {  // Opcode FF 15 is indirect call
				// Get the 4 byte address from the instruction
				unsigned addr = LMMH(*(p + base + 2));
				//const char *c = dlprocptrs[addr].c_str();
				//printf("Checking %x finding %s\n", addr, c);
				if (dlprocptrs[addr] == "exit") {
					if (gap <= 10) {
						// This is it. The instruction at lastOrdCall is (win)main
						addr = LMMH(*(lastOrdCall + base + 1));
						addr += lastOrdCall + 5;  // Addr is dest of call
						//printf("*** MAIN AT 0x%x ***\n", addr);
						return addr + LMMH(m_pPEHeader->Imagebase);
					}
				}
			} else
				borlandState = 0;
			break;
		case 0xEB:  // Short relative jump, e.g. Borland
			if (op2 >= 0x80)  // Branch backwards?
				break;  // Yes, just ignore it
			// Otherwise, actually follow the branch. May have to modify this some time...
			p += op2 + 2;  // +2 for the instruction itself, and op2 for the displacement
			++gap;
			continue;
		case 0x6A:
			if (op2 == 0) {  // Push 00
				// Borland pattern: push 0 / call __ExceptInit / pop ecx / push offset mainInfo / push 0
				// Borland state before: 0              1              2            3               4
				if (borlandState == 0)
					borlandState = 1;
				else if (borlandState == 4) {
					// Borland pattern succeeds. p-4 has the offset of mainInfo
					ADDRESS mainInfo = LMMH(*(base + p - 4));
					ADDRESS main = readNative4(mainInfo + 0x18);  // Address of main is at mainInfo+18
					return main;
				}
			} else
				borlandState = 0;
			break;
		case 0x59:  // Pop ecx
			if (borlandState == 2)
				borlandState = 3;
			else
				borlandState = 0;
			break;
		case 0x68:  // Push 4 byte immediate
			if (borlandState == 3)
				++borlandState;
			else
				borlandState = 0;
			break;
		default:
			borlandState = 0;
			break;
		}
		size_t size = microX86Dis(p + base);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		++gap;
	}

	// VS.NET release console mode pattern
	p = LMMH(m_pPEHeader->EntrypointRVA);
	if (*(p + base + 0x20) == 0xff
	 && *(p + base + 0x21) == 0x15) {
		unsigned int desti = LMMH(*(p + base + 0x22));
		if (dlprocptrs.find(desti) != dlprocptrs.end()
		 && dlprocptrs[desti] == "GetVersionExA") {
			if (*(p + base + 0x6d) == 0xff
			 && *(p + base + 0x6e) == 0x15) {
				desti = LMMH(*(p + base + 0x6f));
				if (dlprocptrs.find(desti) != dlprocptrs.end()
				 && dlprocptrs[desti] == "GetModuleHandleA") {
					if (*(p + base + 0x16e) == 0xe8) {
						unsigned int dest = p + 0x16e + 5 + LMMH(*(p + base + 0x16f));
						return dest + LMMH(m_pPEHeader->Imagebase);
					}
				}
			}
		}
	}

	// For VS.NET, need an old favourite: find a call with three pushes in the first 100 instructions
	int count = 100;
	int pushes = 0;
	p = LMMH(m_pPEHeader->EntrypointRVA);
	while (count > 0) {
		--count;
		unsigned char op1 = *(p + base);
		if (op1 == 0xE8) {  // CALL opcode
			if (pushes == 3) {
				// Get the offset
				int off = LMMH(*(p + base + 1));
				unsigned dest = (unsigned)p + 5 + off;
				// Check for a jump there
				op1 = *(dest + base);
				if (op1 == 0xE9) {
					// Follow that jump
					off = LMMH(*(dest + base + 1));
					dest = dest + 5 + off;
				}
				return dest + LMMH(m_pPEHeader->Imagebase);
			} else
				pushes = 0;  // Assume pushes don't accumulate over calls
		} else if (op1 >= 0x50 && op1 <= 0x57) {  // PUSH opcode
			++pushes;
		} else if (op1 == 0xFF) {
			// FF 35 is push m[K]
			unsigned char op2 = *(p + 1 + base);
			if (op2 == 0x35)
				++pushes;
		} else if (op1 == 0xE9) {
			// Follow the jump
			int off = LMMH(*(p + base + 1));
			p += off + 5;
			continue;
		}


		size_t size = microX86Dis(p + base);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		if (p >= textSize)
			break;
	}

	// mingw pattern
	p = LMMH(m_pPEHeader->EntrypointRVA);
	bool in_mingw_CRTStartup = false;
	unsigned int lastcall = 0, lastlastcall = 0;
	while (1) {
		unsigned char op1 = *(p + base);
		if (op1 == 0xE8) {  // CALL opcode
			unsigned int dest = p + 5 + LMMH(*(p + base + 1));
			if (in_mingw_CRTStartup) {
				unsigned char op2 = *(dest + base);
				unsigned char op2a = *(dest + base + 1);
				unsigned int desti = LMMH(*(dest + base + 2));
				// skip all the call statements until we hit a call to an indirect call to ExitProcess
				// main is the 2nd call before this one
				if (op2 == 0xff && op2a == 0x25
				 && dlprocptrs.find(desti) != dlprocptrs.end()
				 && dlprocptrs[desti] == "ExitProcess") {
					mingw_main = true;
					return lastlastcall + 5 + LMMH(*(lastlastcall + base + 1)) + LMMH(m_pPEHeader->Imagebase);
				}
				lastlastcall = lastcall;
				lastcall = p;
			} else {
				p = dest;
				in_mingw_CRTStartup = true;
				continue;
			}
		}

		size_t size = microX86Dis(p + base);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		if (p >= textSize)
			break;
	}

	// Microsoft VisualC 2-6/net runtime
	p = LMMH(m_pPEHeader->EntrypointRVA);
	bool gotGMHA = false;
	while (1) {
		unsigned char op1 = *(p + base);
		unsigned char op2 = *(p + base + 1);
		if (op1 == 0xFF && op2 == 0x15) { // indirect CALL opcode
			unsigned int desti = LMMH(*(p + base + 2));
			if (dlprocptrs.find(desti) != dlprocptrs.end()
			 && dlprocptrs[desti] == "GetModuleHandleA") {
				gotGMHA = true;
			}
		}
		if (op1 == 0xE8 && gotGMHA) {  // CALL opcode
			unsigned int dest = p + 5 + LMMH(*(p + base + 1));
			addSymbol(dest + LMMH(m_pPEHeader->Imagebase), "WinMain");
			return dest + LMMH(m_pPEHeader->Imagebase);
		}
		if (op1 == 0xc3)   // ret ends search
			break;

		size_t size = microX86Dis(p + base);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		if (p >= textSize)
			break;
	}

	return NO_ADDRESS;
}

bool
Win32BinaryFile::load(std::istream &ifs)
{
	DWord peoffLE, peoff;
	ifs.seekg(0x3c);
	ifs.read((char *)&peoffLE, sizeof peoffLE);  // Note: peoffLE will be in Little Endian
	peoff = LMMH(peoffLE);

	PEHeader tmphdr;

	ifs.seekg(peoff);
	ifs.read((char *)&tmphdr, sizeof tmphdr);
	// Note: all tmphdr fields will be little endian

	base = new unsigned char[LMMH(tmphdr.ImageSize)];
	ifs.seekg(0);
	ifs.read((char *)base, LMMH(tmphdr.HeaderSize));

	m_pHeader = (Header *)base;
	if (m_pHeader->sigLo != 'M' || m_pHeader->sigHi != 'Z') {
		fprintf(stderr, "error loading file %s, bad magic\n", getFilename());
		return false;
	}

	m_pPEHeader = (PEHeader *)(base + peoff);
	if (m_pPEHeader->sigLo != 'P' || m_pPEHeader->sigHi != 'E') {
		fprintf(stderr, "error loading file %s, bad PE magic\n", getFilename());
		return false;
	}

//printf("Image Base %08X, real base %p\n", LMMH(m_pPEHeader->Imagebase), base);

	const PEObject *o = (PEObject *)(((char *)m_pPEHeader) + LH(&m_pPEHeader->NtHdrSize) + 24);
	m_iNumSections = LH(&m_pPEHeader->numObjects);
	m_pSections = new PESectionInfo[m_iNumSections];
	//SectionInfo *reloc = nullptr;
	for (int i = 0; i < m_iNumSections; ++i, ++o) {
		SectionInfo &sect = m_pSections[i];
		//printf("%.8s RVA=%08X Offset=%08X size=%08X\n", (char*)o->ObjectName, LMMH(o->RVA), LMMH(o->PhysicalOffset), LMMH(o->VirtualSize));
		char *name = new char[9];
		strncpy(name, o->ObjectName, 8);
		name[8] = '\0';
		sect.pSectionName = name;
#if 0
		if (!strcmp(sect.pSectionName, ".reloc"))
			reloc = &sect;
#endif
		sect.uNativeAddr = (ADDRESS)(LMMH(o->RVA) + LMMH(m_pPEHeader->Imagebase));
		sect.uHostAddr = (char *)base + LMMH(o->RVA);
		sect.uSectionSize = LMMH(o->VirtualSize);
		DWord Flags = LMMH(o->Flags);
		sect.bBss      = (Flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0;
		sect.bCode     = (Flags & IMAGE_SCN_CNT_CODE)               != 0;
		sect.bData     = (Flags & IMAGE_SCN_CNT_INITIALIZED_DATA)   != 0;
		sect.bReadOnly = (Flags & IMAGE_SCN_MEM_WRITE)              == 0;
		// TODO: Check for unreadable sections (!IMAGE_SCN_MEM_READ)?
		ifs.seekg(LMMH(o->PhysicalOffset));
		memset(base + LMMH(o->RVA), 0, LMMH(o->VirtualSize));
		ifs.read((char *)(base + LMMH(o->RVA)), LMMH(o->PhysicalSize));
		s_sectionObjects[static_cast<const PESectionInfo *>(&sect)] = o;
	}

	// Add the Import Address Table entries to the symbol table
	if (m_pPEHeader->ImportTableRVA) {  // If any import table entry exists
		const PEImportDtor *id = (const PEImportDtor *)(base + LMMH(m_pPEHeader->ImportTableRVA));
		while (id->name != 0) {
			const char *dllName = (const char *)(base + LMMH(id->name));
			unsigned thunk = id->originalFirstThunk ? id->originalFirstThunk : id->firstThunk;
			const unsigned *iat = (const unsigned *)(base + LMMH(thunk));
			unsigned iatEntry = LMMH(*iat);
			ADDRESS paddr = LMMH(m_pPEHeader->Imagebase) + LMMH(id->firstThunk);
			while (iatEntry) {
				if (iatEntry >> 31) {
					// This is an ordinal number (stupid idea)
					std::string nodots(dllName);
					for (auto it = nodots.begin(); it != nodots.end(); ++it)
						if (*it == '.')
							*it = '_';  // Dots can't be in identifiers
					std::ostringstream ost;
					ost << nodots << "_" << (iatEntry & 0x7FFFFFFF);
					dlprocptrs[paddr] = ost.str();
					// printf("Added symbol %s value %x\n", ost.str().c_str(), paddr);
				} else {
					// Normal case (IMAGE_IMPORT_BY_NAME). Skip the useless hint (2 bytes)
					std::string name((const char *)(base + iatEntry + 2));
					dlprocptrs[paddr] = name;
					//printf("Added symbol %s value %x\n", name.c_str(), paddr);
					if (paddr != LMMH(m_pPEHeader->Imagebase) + ((const char *)iat - (const char *)base)) {
						dlprocptrs[LMMH(m_pPEHeader->Imagebase) + ((const char *)iat - (const char *)base)] = name.insert(0, "old_"); // add both possibilities
						//printf("Also added %s value %x\n", name.c_str(), LMMH(m_pPEHeader->Imagebase) + ((const char *)iat - (const char *)base));
					}
				}
				++iat;
				iatEntry = LMMH(*iat);
				paddr += 4;
			}
			++id;
		}
	}

	// Was hoping that _main or main would turn up here for Borland console mode programs. No such luck.
	// I think IDA Pro must find it by a combination of FLIRT and some pattern matching
	//PEExportDtor *eid = (PEExportDtor *)(LMMH(m_pPEHeader->ExportTableRVA) + base);


	// Give the entry point a symbol
	ADDRESS entry = getMainEntryPoint();
	if (entry != NO_ADDRESS) {
		auto it = dlprocptrs.find(entry);
		if (it == dlprocptrs.end())
			dlprocptrs[entry] = "main";
	}

	// Give a name to any jumps you find to these import entries
	// NOTE: VERY early MSVC specific!! Temporary till we can think of a better way.
	ADDRESS start = getEntryPoint();
	findJumps(start);

	return true;
}

/**
 * \brief Find names for jumps to IATs.
 *
 * Used above for a hack to find jump instructions pointing to IATs.
 *
 * Heuristic: start just before the "start" entry point looking for FF 25
 * opcodes followed by a pointer to an import entry.  E.g. FF 25 58 44 40 00
 * where 00404458 is the IAT for _ftol.
 *
 * \note Some are on 0x10 byte boundaries, some on 2 byte boundaries (6 byte
 * jumps packed), and there are often up to 0x30 bytes of statically linked
 * library code (e.g. _atexit, __onexit) with sometimes two static libs in a
 * row.  So keep going until there is about 0x60 bytes with no match.
 *
 * \note Slight chance of coming across a misaligned match; probability is
 * about 1/65536 times dozens in 2^32 ~= 10^-13.
 */
void
Win32BinaryFile::findJumps(ADDRESS curr)
{
	int cnt = 0;  // Count of bytes with no match
	const SectionInfo *sec = getSectionInfoByName(".text");
	if (!sec) sec = getSectionInfoByName("CODE");
	assert(sec);
	// Add to native addr to get host:
	ptrdiff_t delta = sec->uHostAddr - (char *)sec->uNativeAddr;
	while (cnt < 0x60) {  // Max of 0x60 bytes without a match
		curr -= 2;  // Has to be on 2-byte boundary
		cnt += 2;
		if (LH(delta + curr) != 0xFF + (0x25 << 8)) continue;
		ADDRESS operand = LMMH2(delta + curr + 2);
		auto it = dlprocptrs.find(operand);
		if (it == dlprocptrs.end()) continue;
		std::string sym = it->second;
		dlprocptrs[operand] = "__imp_" + sym;
		dlprocptrs[curr] = sym;   // Add new entry
		// std::cerr << "Added " << sym << " at 0x" << std::hex << curr << "\n";
		curr -= 4;  // Next match is at least 4+2 bytes away
		cnt = 0;
	}
}

#if 0 // Cruft?
bool
Win32BinaryFile::PostLoad(void *handle)
{
	return false;
}
#endif

const char *
Win32BinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	if (m_pPEHeader->Subsystem == 1  // native
	 && LMMH(m_pPEHeader->EntrypointRVA) + LMMH(m_pPEHeader->Imagebase) == dwAddr)
		return "DriverEntry";

	if (isMinGWsAllocStack(dwAddr))
		return "__mingw_allocstack";
	if (isMinGWsFrameInit(dwAddr))
		return "__mingw_frame_init";
	if (isMinGWsFrameEnd(dwAddr))
		return "__mingw_frame_end";
	if (isMinGWsCleanupSetup(dwAddr))
		return "__mingw_cleanup_setup";
	if (isMinGWsMalloc(dwAddr))
		return "malloc";

	auto it = dlprocptrs.find(dwAddr);
	if (it == dlprocptrs.end())
		return nullptr;
	return it->second.c_str();
}

ADDRESS
Win32BinaryFile::getAddressByName(const char *pName, bool bNoTypeOK /* = false */)
{
	// This is "looking up the wrong way" and hopefully is uncommon.  Use linear search
	auto it = dlprocptrs.begin();
	while (it != dlprocptrs.end()) {
		// std::cerr << "Symbol: " << it->second.c_str() << " at 0x" << std::hex << it->first << "\n";
		if (it->second == pName)
			return it->first;
		++it;
	}
	return NO_ADDRESS;
}

void
Win32BinaryFile::addSymbol(ADDRESS uNative, const char *pName)
{
	dlprocptrs[uNative] = pName;
}

/**
 * \brief Read 2 bytes from native addr.
 */
int
Win32BinaryFile::win32Read2(const short *ps) const
{
	const unsigned char *p = (const unsigned char *)ps;
	// Little endian
	int n = (int)(p[0] + (p[1] << 8));
	return n;
}

/**
 * \brief Read 4 bytes from native addr.
 */
int
Win32BinaryFile::win32Read4(const int *pi) const
{
	const short *p = (const short *)pi;
	int n1 = win32Read2(p);
	int n2 = win32Read2(p + 1);
	int n = (int)(n1 | (n2 << 16));
	return n;
}

int
Win32BinaryFile::readNative1(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return -1;
	return si->uHostAddr[nat - si->uNativeAddr];
}

int
Win32BinaryFile::readNative2(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return 0;
	return win32Read2((const short *)&si->uHostAddr[nat - si->uNativeAddr]);
}

int
Win32BinaryFile::readNative4(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return 0;
	return win32Read4((const int *)&si->uHostAddr[nat - si->uNativeAddr]);
}

QWord
Win32BinaryFile::readNative8(ADDRESS nat) const
{
	int raw[2];
#ifdef WORDS_BIGENDIAN  // This tests the host machine
	// Source and host are different endianness
	raw[1] = readNative4(nat);
	raw[0] = readNative4(nat + 4);
#else
	// Source and host are same endianness
	raw[0] = readNative4(nat);
	raw[1] = readNative4(nat + 4);
#endif
	return *(QWord *)raw;
}

float
Win32BinaryFile::readNativeFloat4(ADDRESS nat) const
{
	int raw = readNative4(nat);
	// Ugh! gcc says that reinterpreting from int to float is invalid!!
	//return reinterpret_cast<float>(raw);  // Note: cast, not convert!!
	return *(float *)&raw;  // Note: cast, not convert
}

double
Win32BinaryFile::readNativeFloat8(ADDRESS nat) const
{
	int raw[2];
#ifdef WORDS_BIGENDIAN  // This tests the host machine
	// Source and host are different endianness
	raw[1] = readNative4(nat);
	raw[0] = readNative4(nat + 4);
#else
	// Source and host are same endianness
	raw[0] = readNative4(nat);
	raw[1] = readNative4(nat + 4);
#endif
	//return reinterpret_cast<double>(*raw);  // Note: cast, not convert!!
	return *(double *)raw;
}

bool
Win32BinaryFile::isDynamicLinkedProcPointer(ADDRESS uNative) const
{
	return dlprocptrs.find(uNative) != dlprocptrs.end();
}

bool
Win32BinaryFile::isStaticLinkedLibProc(ADDRESS uNative) const
{
	return isMinGWsAllocStack(uNative)
	    || isMinGWsFrameInit(uNative)
	    || isMinGWsFrameEnd(uNative)
	    || isMinGWsCleanupSetup(uNative)
	    || isMinGWsMalloc(uNative);
}

bool
Win32BinaryFile::isMinGWsAllocStack(ADDRESS uNative) const
{
	if (!mingw_main) return false;
	const SectionInfo *si = getSectionInfoByAddr(uNative);
	if (!si) return false;

	const char *host = &si->uHostAddr[uNative - si->uNativeAddr];
	const unsigned char pat[] = {
		0x51, 0x89, 0xe1, 0x83, 0xc1, 0x08, 0x3d, 0x00,
		0x10, 0x00, 0x00, 0x72, 0x10, 0x81, 0xe9, 0x00,
		0x10, 0x00, 0x00, 0x83, 0x09, 0x00, 0x2d, 0x00,
		0x10, 0x00, 0x00, 0xeb, 0xe9, 0x29, 0xc1, 0x83,
		0x09, 0x00, 0x89, 0xe0, 0x89, 0xcc, 0x8b, 0x08,
		0x8b, 0x40, 0x04, 0xff, 0xe0,
	};
	if (memcmp(host, pat, sizeof pat)) return false;

	return true;
}

bool
Win32BinaryFile::isMinGWsFrameInit(ADDRESS uNative) const
{
	if (!mingw_main) return false;
	const SectionInfo *si = getSectionInfoByAddr(uNative);
	if (!si) return false;

	const char *host = &si->uHostAddr[uNative - si->uNativeAddr];
	const unsigned char pat1[] = {
		0x55, 0x89, 0xe5, 0x83, 0xec, 0x18, 0x89, 0x7d,
		0xfc, 0x8b, 0x7d, 0x08, 0x89, 0x5d, 0xf4, 0x89,
		0x75, 0xf8,
	};
	if (memcmp(host, pat1, sizeof pat1)) return false;

	host += sizeof pat1 + 6;
	const unsigned char pat2[] = {
		0x85, 0xd2, 0x74, 0x24, 0x8b, 0x42, 0x2c, 0x85,
		0xc0, 0x78, 0x3d, 0x8b, 0x42, 0x2c, 0x85, 0xc0,
		0x75, 0x56, 0x8b, 0x42, 0x28, 0x89, 0x07, 0x89,
		0x7a, 0x28, 0x8b, 0x5d, 0xf4, 0x8b, 0x75, 0xf8,
		0x8b, 0x7d, 0xfc, 0x89, 0xec, 0x5d, 0xc3,
	};
	if (memcmp(host, pat2, sizeof pat2)) return false;

	return true;
}

bool
Win32BinaryFile::isMinGWsFrameEnd(ADDRESS uNative) const
{
	if (!mingw_main) return false;
	const SectionInfo *si = getSectionInfoByAddr(uNative);
	if (!si) return false;

	const char *host = &si->uHostAddr[uNative - si->uNativeAddr];
	const unsigned char pat1[] = {
		0x55, 0x89, 0xe5, 0x53, 0x83, 0xec, 0x14, 0x8b,
		0x45, 0x08, 0x8b, 0x18,
	};
	if (memcmp(host, pat1, sizeof pat1)) return false;

	host += sizeof pat1 + 5;
	const unsigned char pat2[] = {
		0x85, 0xc0, 0x74, 0x1b, 0x8b, 0x48, 0x2c, 0x85,
		0xc9, 0x78, 0x34, 0x8b, 0x50, 0x2c, 0x85, 0xd2,
		0x75, 0x4d, 0x89, 0x58, 0x28, 0x8b, 0x5d, 0xfc,
		0xc9, 0xc3,
	};
	if (memcmp(host, pat2, sizeof pat2)) return false;

	return true;
}

bool
Win32BinaryFile::isMinGWsCleanupSetup(ADDRESS uNative) const
{
	if (!mingw_main) return false;
	const SectionInfo *si = getSectionInfoByAddr(uNative);
	if (!si) return false;

	const char *host = &si->uHostAddr[uNative - si->uNativeAddr];
	const unsigned char pat1[] = {
		0x55, 0x89, 0xe5, 0x53, 0x83, 0xec, 0x04,
	};
	if (memcmp(host, pat1, sizeof pat1)) return false;

	host += sizeof pat1 + 6;
	const unsigned char pat2[] = {
		0x85, 0xdb, 0x75, 0x35,
	};
	if (memcmp(host, pat2, sizeof pat2)) return false;

	host += sizeof pat2 + 16;
	const unsigned char pat3[] = {
		0x83, 0xf8, 0xff, 0x74, 0x24, 0x85, 0xc0, 0x89,
		0xc3, 0x74, 0x0e, 0x8d, 0x74, 0x26, 0x00,
	};
	if (memcmp(host, pat3, sizeof pat3)) return false;

	return true;
}

bool
Win32BinaryFile::isMinGWsMalloc(ADDRESS uNative) const
{
	if (!mingw_main) return false;
	const SectionInfo *si = getSectionInfoByAddr(uNative);
	if (!si) return false;

	const char *host = &si->uHostAddr[uNative - si->uNativeAddr];
	const unsigned char pat1[] = {
		0x55, 0x89, 0xe5, 0x8d, 0x45, 0xf4, 0x83, 0xec,
		0x58, 0x89, 0x45, 0xe0, 0x8d, 0x45, 0xc0, 0x89,
		0x04, 0x24, 0x89, 0x5d, 0xf4, 0x89, 0x75, 0xf8,
		0x89, 0x7d, 0xfc,
	};
	if (memcmp(host, pat1, sizeof pat1)) return false;

	host += sizeof pat1 + 0x15;
	const unsigned char pat2[] = {
		0x89, 0x65, 0xe8,
	};
	if (memcmp(host, pat2, sizeof pat2)) return false;

	return true;
}

ADDRESS
Win32BinaryFile::isJumpToAnotherAddr(ADDRESS uNative) const
{
	if ((readNative1(uNative) & 0xff) != 0xe9)
		return NO_ADDRESS;
	return readNative4(uNative + 1) + uNative + 5;
}

const char *
Win32BinaryFile::getDynamicProcName(ADDRESS uNative)
{
	return dlprocptrs[uNative].c_str();
}

bool
Win32BinaryFile::isLibrary() const
{
	return (m_pPEHeader->Flags & 0x2000) != 0;
}

ADDRESS
Win32BinaryFile::getImageBase() const
{
	return m_pPEHeader->Imagebase;
}

size_t
Win32BinaryFile::getImageSize() const
{
	return m_pPEHeader->ImageSize;
}

std::list<const char *>
Win32BinaryFile::getDependencyList()
{
	return std::list<const char *>(); /* FIXME */
}

#if 0 // Cruft?
DWord
Win32BinaryFile::getDelta()
{
	// Stupid function anyway: delta depends on section
	// This should work for the header only
	//return (DWord)base - LMMH(m_pPEHeader->Imagebase);
	return (DWord)base - (DWord)m_pPEHeader->Imagebase;
}
#endif

/**
 * \brief For debugging.
 */
void
Win32BinaryFile::dumpSymbols() const
{
	std::cerr << std::hex;
	for (auto it = dlprocptrs.begin(); it != dlprocptrs.end(); ++it)
		std::cerr << "0x" << it->first << " " << it->second << "        ";
	std::cerr << std::dec << "\n";
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
	return new Win32BinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (Win32BinaryFile *)bf;
}
#endif
