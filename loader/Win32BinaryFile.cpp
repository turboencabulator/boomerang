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

#include <algorithm>
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
		if (a < uNativeAddr || a >= uNativeAddr + uSectionSize)
			return false; // not even within this section
		if (bBss)
			return true; // obvious
		if (bReadOnly)
			return false; // R/O BSS makes no sense.
		// Don't check for bData here. So long as the section has slack at end, that space can contain BSS.
		auto it = s_sectionObjects.find(this);
		assert(it != s_sectionObjects.end());
		assert(it->second);
		assert(this == it->first);
		const PEObject *sectionHeader = it->second;
		bool has_slack = sectionHeader->VirtualSize > sectionHeader->PhysicalSize;
		if (!has_slack)
			return false; // BSS not possible.
		if (a >= uNativeAddr + sectionHeader->PhysicalSize)
			return true;
		return false;
	}
};

static_assert(sizeof (SectionInfo) == sizeof (PESectionInfo), "PESectionInfo size mismatch");

}


Win32BinaryFile::Win32BinaryFile()
{
	bigendian = false;
}

Win32BinaryFile::~Win32BinaryFile()
{
	delete [] base;
}

ADDRESS
Win32BinaryFile::getEntryPoint() const
{
	return (ADDRESS)(m_pPEHeader->EntrypointRVA
	               + m_pPEHeader->Imagebase);
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

	if (m_pPEHeader->Subsystem == 1)  // native
		return m_pPEHeader->EntrypointRVA + m_pPEHeader->Imagebase;

	const SectionInfo *si = getSectionInfoByName(".text");
	if (!si) si = getSectionInfoByName("CODE");
	assert(si);
	unsigned textSize = si->uSectionSize;

	// Start at program entry point
	unsigned p = m_pPEHeader->EntrypointRVA;
	unsigned lim = p + 0x200;
	if (textSize < 0x200)
		lim = p + textSize;

	unsigned lastOrdCall = 0;
	int gap = 0xF0000000;  // Number of instructions from the last ordinary call
	                       // Large positive number (in case no ordinary calls)
	int borlandState = 0;  // State machine for Borland
	while (p < lim) {
		unsigned char op1 = base[p];
		unsigned char op2 = base[p + 1];
		//std::cerr << std::hex << "At " << p << ", ops " << (unsigned)op1 << ", " << (unsigned)op2 << std::dec << "\n";
		switch (op1) {
		case 0xE8:
			// An ordinary call; this could be to winmain/main
			lastOrdCall = p;
			gap = 0;
			if (borlandState == 1)
				++borlandState;
			else
				borlandState = 0;
			break;
		case 0xFF:
			if (op2 == 0x15) {  // Opcode FF 15 is indirect call
				// Get the 4 byte address from the instruction
				unsigned addr = LH32(&base[p + 2]);
				//const char *c = dlprocptrs[addr].c_str();
				//printf("Checking %x finding %s\n", addr, c);
				auto it = dlprocptrs.find(addr);
				if (it != dlprocptrs.end()
				 && it->second == "exit") {
					if (gap <= 10) {
						// This is it. The instruction at lastOrdCall is (win)main
						addr = LH32(&base[lastOrdCall + 1]);
						addr += lastOrdCall + 5;  // Addr is dest of call
						//printf("*** MAIN AT 0x%x ***\n", addr);
						return addr + m_pPEHeader->Imagebase;
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
					ADDRESS mainInfo = LH32(&base[p - 4]);
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
		size_t size = microX86Dis(&base[p]);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		++gap;
	}

	// VS.NET release console mode pattern
	p = m_pPEHeader->EntrypointRVA;
	if (base[p + 0x20] == 0xff
	 && base[p + 0x21] == 0x15) {
		unsigned int desti = LH32(&base[p + 0x22]);
		auto it = dlprocptrs.find(desti);
		if (it != dlprocptrs.end()
		 && it->second == "GetVersionExA") {
			if (base[p + 0x6d] == 0xff
			 && base[p + 0x6e] == 0x15) {
				desti = LH32(&base[p + 0x6f]);
				it = dlprocptrs.find(desti);
				if (it != dlprocptrs.end()
				 && it->second == "GetModuleHandleA") {
					if (base[p + 0x16e] == 0xe8) {
						unsigned int dest = p + 0x16e + 5 + LH32(&base[p + 0x16f]);
						return dest + m_pPEHeader->Imagebase;
					}
				}
			}
		}
	}

	// For VS.NET, need an old favourite: find a call with three pushes in the first 100 instructions
	int count = 100;
	int pushes = 0;
	p = m_pPEHeader->EntrypointRVA;
	while (count > 0) {
		--count;
		unsigned char op1 = base[p];
		if (op1 == 0xE8) {  // CALL opcode
			if (pushes == 3) {
				// Get the offset
				int off = LH32(&base[p + 1]);
				unsigned dest = (unsigned)p + 5 + off;
				// Check for a jump there
				op1 = base[dest];
				if (op1 == 0xE9) {
					// Follow that jump
					off = LH32(&base[dest + 1]);
					dest = dest + 5 + off;
				}
				return dest + m_pPEHeader->Imagebase;
			} else
				pushes = 0;  // Assume pushes don't accumulate over calls
		} else if (op1 >= 0x50 && op1 <= 0x57) {  // PUSH opcode
			++pushes;
		} else if (op1 == 0xFF) {
			// FF 35 is push m[K]
			unsigned char op2 = base[p + 1];
			if (op2 == 0x35)
				++pushes;
		} else if (op1 == 0xE9) {
			// Follow the jump
			int off = LH32(&base[p + 1]);
			p += off + 5;
			continue;
		}

		size_t size = microX86Dis(&base[p]);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		if (p >= textSize)
			break;
	}

	// mingw pattern
	p = m_pPEHeader->EntrypointRVA;
	bool in_mingw_CRTStartup = false;
	unsigned int lastcall = 0, lastlastcall = 0;
	while (1) {
		unsigned char op1 = base[p];
		if (op1 == 0xE8) {  // CALL opcode
			unsigned int dest = p + 5 + LH32(&base[p + 1]);
			if (in_mingw_CRTStartup) {
				unsigned char op2 = base[dest];
				unsigned char op2a = base[dest + 1];
				unsigned int desti = LH32(&base[dest + 2]);
				auto it = dlprocptrs.find(desti);
				// skip all the call statements until we hit a call to an indirect call to ExitProcess
				// main is the 2nd call before this one
				if (op2 == 0xff && op2a == 0x25
				 && it != dlprocptrs.end()
				 && it->second == "ExitProcess") {
					mingw_main = true;
					return lastlastcall + 5 + LH32(&base[lastlastcall + 1]) + m_pPEHeader->Imagebase;
				}
				lastlastcall = lastcall;
				lastcall = p;
			} else {
				p = dest;
				in_mingw_CRTStartup = true;
				continue;
			}
		}

		size_t size = microX86Dis(&base[p]);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		if (p >= textSize)
			break;
	}

	// Microsoft VisualC 2-6/net runtime
	p = m_pPEHeader->EntrypointRVA;
	bool gotGMHA = false;
	while (1) {
		unsigned char op1 = base[p];
		unsigned char op2 = base[p + 1];
		if (op1 == 0xFF && op2 == 0x15) { // indirect CALL opcode
			unsigned int desti = LH32(&base[p + 2]);
			auto it = dlprocptrs.find(desti);
			if (it != dlprocptrs.end()
			 && it->second == "GetModuleHandleA") {
				gotGMHA = true;
			}
		}
		if (op1 == 0xE8 && gotGMHA) {  // CALL opcode
			unsigned int dest = p + 5 + LH32(&base[p + 1]);
			addSymbol(dest + m_pPEHeader->Imagebase, "WinMain");
			return dest + m_pPEHeader->Imagebase;
		}
		if (op1 == 0xc3)   // ret ends search
			break;

		size_t size = microX86Dis(&base[p]);
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
	uint32_t peoff;
	ifs.seekg(0x3c);
	ifs.read((char *)&peoff, sizeof peoff);
	peoff = LH32(&peoff);

	PEHeader tmphdr;
	ifs.seekg(peoff);
	ifs.read((char *)&tmphdr, sizeof tmphdr);
	// Note: all tmphdr fields will be little endian
	tmphdr.ImageSize  = LH32(&tmphdr.ImageSize);
	tmphdr.HeaderSize = LH32(&tmphdr.HeaderSize);

	base = new unsigned char[tmphdr.ImageSize];
	ifs.seekg(0);
	ifs.read((char *)base, tmphdr.HeaderSize);

	m_pPEHeader = (PEHeader *)&base[peoff];
	assert(m_pPEHeader->sigLo == 'P' && m_pPEHeader->sigHi == 'E');

	m_pPEHeader->sigver                 = LH16(&m_pPEHeader->sigver);
	m_pPEHeader->cputype                = LH16(&m_pPEHeader->cputype);
	m_pPEHeader->numObjects             = LH16(&m_pPEHeader->numObjects);
	m_pPEHeader->TimeDate               = LH32(&m_pPEHeader->TimeDate);
	m_pPEHeader->Reserved1              = LH32(&m_pPEHeader->Reserved1);
	m_pPEHeader->Reserved2              = LH32(&m_pPEHeader->Reserved2);
	m_pPEHeader->NtHdrSize              = LH16(&m_pPEHeader->NtHdrSize);
	m_pPEHeader->Flags                  = LH16(&m_pPEHeader->Flags);
	m_pPEHeader->Reserved3              = LH16(&m_pPEHeader->Reserved3);
	m_pPEHeader->Reserved4              = LH32(&m_pPEHeader->Reserved4);
	m_pPEHeader->Reserved5              = LH32(&m_pPEHeader->Reserved5);
	m_pPEHeader->Reserved6              = LH32(&m_pPEHeader->Reserved6);
	m_pPEHeader->EntrypointRVA          = LH32(&m_pPEHeader->EntrypointRVA);
	m_pPEHeader->Reserved7              = LH32(&m_pPEHeader->Reserved7);
	m_pPEHeader->Reserved8              = LH32(&m_pPEHeader->Reserved8);
	m_pPEHeader->Imagebase              = LH32(&m_pPEHeader->Imagebase);
	m_pPEHeader->ObjectAlign            = LH32(&m_pPEHeader->ObjectAlign);
	m_pPEHeader->FileAlign              = LH32(&m_pPEHeader->FileAlign);
	m_pPEHeader->OSMajor                = LH16(&m_pPEHeader->OSMajor);
	m_pPEHeader->OSMinor                = LH16(&m_pPEHeader->OSMinor);
	m_pPEHeader->UserMajor              = LH16(&m_pPEHeader->UserMajor);
	m_pPEHeader->UserMinor              = LH16(&m_pPEHeader->UserMinor);
	m_pPEHeader->SubsysMajor            = LH16(&m_pPEHeader->SubsysMajor);
	m_pPEHeader->SubsysMinor            = LH16(&m_pPEHeader->SubsysMinor);
	m_pPEHeader->Reserved9              = LH32(&m_pPEHeader->Reserved9);
	m_pPEHeader->ImageSize              = LH32(&m_pPEHeader->ImageSize);
	m_pPEHeader->HeaderSize             = LH32(&m_pPEHeader->HeaderSize);
	m_pPEHeader->FileChecksum           = LH32(&m_pPEHeader->FileChecksum);
	m_pPEHeader->Subsystem              = LH16(&m_pPEHeader->Subsystem);
	m_pPEHeader->DLLFlags               = LH16(&m_pPEHeader->DLLFlags);
	m_pPEHeader->StackReserveSize       = LH32(&m_pPEHeader->StackReserveSize);
	m_pPEHeader->StackCommitSize        = LH32(&m_pPEHeader->StackCommitSize);
	m_pPEHeader->HeapReserveSize        = LH32(&m_pPEHeader->HeapReserveSize);
	m_pPEHeader->HeapCommitSize         = LH32(&m_pPEHeader->HeapCommitSize);
	m_pPEHeader->Reserved10             = LH32(&m_pPEHeader->Reserved10);
	m_pPEHeader->nInterestingRVASizes   = LH32(&m_pPEHeader->nInterestingRVASizes);
	m_pPEHeader->ExportTableRVA         = LH32(&m_pPEHeader->ExportTableRVA);
	m_pPEHeader->TotalExportDataSize    = LH32(&m_pPEHeader->TotalExportDataSize);
	m_pPEHeader->ImportTableRVA         = LH32(&m_pPEHeader->ImportTableRVA);
	m_pPEHeader->TotalImportDataSize    = LH32(&m_pPEHeader->TotalImportDataSize);
	m_pPEHeader->ResourceTableRVA       = LH32(&m_pPEHeader->ResourceTableRVA);
	m_pPEHeader->TotalResourceDataSize  = LH32(&m_pPEHeader->TotalResourceDataSize);
	m_pPEHeader->ExceptionTableRVA      = LH32(&m_pPEHeader->ExceptionTableRVA);
	m_pPEHeader->TotalExceptionDataSize = LH32(&m_pPEHeader->TotalExceptionDataSize);
	m_pPEHeader->SecurityTableRVA       = LH32(&m_pPEHeader->SecurityTableRVA);
	m_pPEHeader->TotalSecurityDataSize  = LH32(&m_pPEHeader->TotalSecurityDataSize);
	m_pPEHeader->FixupTableRVA          = LH32(&m_pPEHeader->FixupTableRVA);
	m_pPEHeader->TotalFixupDataSize     = LH32(&m_pPEHeader->TotalFixupDataSize);
	m_pPEHeader->DebugTableRVA          = LH32(&m_pPEHeader->DebugTableRVA);
	m_pPEHeader->TotalDebugDirectories  = LH32(&m_pPEHeader->TotalDebugDirectories);
	m_pPEHeader->ImageDescriptionRVA    = LH32(&m_pPEHeader->ImageDescriptionRVA);
	m_pPEHeader->TotalDescriptionSize   = LH32(&m_pPEHeader->TotalDescriptionSize);
	m_pPEHeader->MachineSpecificRVA     = LH32(&m_pPEHeader->MachineSpecificRVA);
	m_pPEHeader->MachineSpecificSize    = LH32(&m_pPEHeader->MachineSpecificSize);
	m_pPEHeader->ThreadLocalStorageRVA  = LH32(&m_pPEHeader->ThreadLocalStorageRVA);
	m_pPEHeader->TotalTLSSize           = LH32(&m_pPEHeader->TotalTLSSize);

	//printf("Image Base %08X, real base %p\n", m_pPEHeader->Imagebase, base);

	auto o = (PEObject *)(((char *)m_pPEHeader) + m_pPEHeader->NtHdrSize + 24);
	sections.reserve(m_pPEHeader->numObjects);
	for (int i = 0; i < m_pPEHeader->numObjects; ++i, ++o) {
		o->VirtualSize    = LH32(&o->VirtualSize);
		o->RVA            = LH32(&o->RVA);
		o->PhysicalSize   = LH32(&o->PhysicalSize);
		o->PhysicalOffset = LH32(&o->PhysicalOffset);
		o->Reserved1      = LH32(&o->Reserved1);
		o->Reserved2      = LH32(&o->Reserved2);
		o->Reserved3      = LH32(&o->Reserved3);
		o->Flags          = LH32(&o->Flags);

		//printf("%.8s RVA=%08X Offset=%08X size=%08X\n", o->ObjectName, o->RVA, o->PhysicalOffset, o->VirtualSize);

		auto name = std::string(o->ObjectName, sizeof o->ObjectName);
		auto len = name.find('\0');
		if (len != name.npos)
			name.erase(len);

		auto sect = PESectionInfo();
		sect.name = name;
		sect.uNativeAddr = (ADDRESS)(o->RVA + m_pPEHeader->Imagebase);
		sect.uHostAddr = (char *)&base[o->RVA];
		sect.uSectionSize = o->VirtualSize;
		uint32_t Flags = o->Flags;
		sect.bBss      = (Flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0;
		sect.bCode     = (Flags & IMAGE_SCN_CNT_CODE)               != 0;
		sect.bData     = (Flags & IMAGE_SCN_CNT_INITIALIZED_DATA)   != 0;
		sect.bReadOnly = (Flags & IMAGE_SCN_MEM_WRITE)              == 0;
		// TODO: Check for unreadable sections (!IMAGE_SCN_MEM_READ)?
		sections.push_back(sect);

		ifs.seekg(o->PhysicalOffset);
		memset(&base[o->RVA], 0, o->VirtualSize);
		ifs.read((char *)&base[o->RVA], o->PhysicalSize);
		s_sectionObjects[static_cast<const PESectionInfo *>(&sections.back())] = o;
	}

	// Add the Import Address Table entries to the symbol table
	if (m_pPEHeader->ImportTableRVA) {  // If any import table entry exists
		auto id = (PEImportDtor *)&base[m_pPEHeader->ImportTableRVA];

		id->originalFirstThunk = LH32(&id->originalFirstThunk);
		id->preSnapDate        = LH32(&id->preSnapDate);
		id->verMajor           = LH16(&id->verMajor);
		id->verMinor           = LH16(&id->verMinor);
		id->name               = LH32(&id->name);
		id->firstThunk         = LH32(&id->firstThunk);

		while (id->name != 0) {
			auto dllName = (const char *)&base[id->name];
			unsigned thunk = id->originalFirstThunk ? id->originalFirstThunk : id->firstThunk;
			auto iat = (const unsigned *)&base[thunk];
			unsigned iatEntry = LH32(iat);
			ADDRESS paddr = m_pPEHeader->Imagebase + id->firstThunk;
			while (iatEntry) {
				if (iatEntry >> 31) {
					// This is an ordinal number (stupid idea)
					std::string nodots(dllName);
					std::replace(nodots.begin(), nodots.end(), '.', '_');  // Dots can't be in identifiers
					std::ostringstream ost;
					ost << nodots << "_" << (iatEntry & 0x7FFFFFFF);
					dlprocptrs[paddr] = ost.str();
					// printf("Added symbol %s value %x\n", ost.str().c_str(), paddr);
				} else {
					// Normal case (IMAGE_IMPORT_BY_NAME). Skip the useless hint (2 bytes)
					std::string name((const char *)&base[iatEntry + 2]);
					dlprocptrs[paddr] = name;
					//printf("Added symbol %s value %x\n", name.c_str(), paddr);
					if (paddr != m_pPEHeader->Imagebase + ((const char *)iat - (const char *)base)) {
						dlprocptrs[m_pPEHeader->Imagebase + ((const char *)iat - (const char *)base)] = name.insert(0, "old_"); // add both possibilities
						//printf("Also added %s value %x\n", name.c_str(), m_pPEHeader->Imagebase + ((const char *)iat - (const char *)base));
					}
				}
				++iat;
				iatEntry = LH32(iat);
				paddr += 4;
			}
			++id;
		}
	}

	// Was hoping that _main or main would turn up here for Borland console mode programs. No such luck.
	// I think IDA Pro must find it by a combination of FLIRT and some pattern matching
	//auto eid = (PEExportDtor *)&base[m_pPEHeader->ExportTableRVA];


	// Give the entry point a symbol
	ADDRESS entry = getMainEntryPoint();
	if (entry != NO_ADDRESS) {
		if (!dlprocptrs.count(entry))
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
		if (LH16(delta + curr) != 0xFF + (0x25 << 8)) continue;
		ADDRESS operand = LH32(delta + curr + 2);
		auto it = dlprocptrs.find(operand);
		if (it == dlprocptrs.end()) continue;
		std::string sym = it->second;
		dlprocptrs[operand] = "__imp_" + sym;
		dlprocptrs[curr] = sym;   // Add new entry
		// std::cerr << "Added " << sym << " at 0x" << std::hex << curr << std::dec << "\n";
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
	 && m_pPEHeader->EntrypointRVA + m_pPEHeader->Imagebase == dwAddr)
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
	if (it != dlprocptrs.end())
		return it->second.c_str();
	return nullptr;
}

ADDRESS
Win32BinaryFile::getAddressByName(const std::string &name, bool bNoTypeOK) const
{
	// This is "looking up the wrong way" and hopefully is uncommon.  Use linear search
	for (const auto &sym : dlprocptrs) {
		// std::cerr << "Symbol: " << sym.second << " at 0x" << std::hex << sym.first << std::dec << "\n";
		if (sym.second == name)
			return sym.first;
	}
	return NO_ADDRESS;
}

void
Win32BinaryFile::addSymbol(ADDRESS uNative, const std::string &name)
{
	dlprocptrs[uNative] = name;
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
Win32BinaryFile::isDynamicLinkedProcPointer(ADDRESS uNative) const
{
	return !!dlprocptrs.count(uNative);
}

const char *
Win32BinaryFile::getDynamicProcName(ADDRESS uNative) const
{
	auto it = dlprocptrs.find(uNative);
	if (it != dlprocptrs.end())
		return it->second.c_str();
	return nullptr;
}

#if 0 // Cruft?
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
#endif

#if 0 // Cruft?
/**
 * \brief For debugging.
 */
void
Win32BinaryFile::dumpSymbols() const
{
	std::cerr << std::hex;
	for (const auto &sym : dlprocptrs)
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
	return new Win32BinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (Win32BinaryFile *)bf;
}
#endif
