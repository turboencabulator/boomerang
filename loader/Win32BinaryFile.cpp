/**
 * \file
 * \brief Contains the implementation of the class Win32BinaryFile.
 *
 * This file implements the class Win32BinaryFile, derived from class
 * BinaryFile.  See Win32BinaryFile.h and BinaryFile.h for details.
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
#include "config.h"
#endif

#include "BinaryFile.h"
#include "Win32BinaryFile.h"

#include <iostream>
#include <sstream>

#include <cstdlib>
#include <cstring>
#include <cassert>

extern "C" int microX86Dis(void *p);  // From microX86dis.c


#ifndef IMAGE_SCN_CNT_CODE // Assume that if one is not defined, the rest isn't either.
#define IMAGE_SCN_CNT_CODE               0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA   0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_READ               0x40000000
#define IMAGE_SCN_MEM_WRITE              0x80000000
#endif


namespace {

// Due to the current rigid design, where BinaryFile holds a C-style array of
// SectionInfo's, we can't extend a subclass of SectionInfo with the data required
// to express the semantics of a PE section. We therefore need this external mapping
// from SectionInfo's to PEObject's, that contain the info we need.
// TODO: Refactor BinaryFile to not expose its private parts in public. Design both
// a protected (for subclasses) and public (for users) interface.
typedef std::map<const class PESectionInfo *, const PEObject *> SectionObjectMap;

SectionObjectMap s_sectionObjects;


// Note that PESectionInfo currently must be the exact same size as
// SectionInfo due to the already mentioned array held by BinaryFile.
class PESectionInfo : public SectionInfo {
	virtual bool isAddressBss(ADDRESS a) const
	{
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
		const SectionObjectMap::iterator it = s_sectionObjects.find(this);
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

// attempt at a compile-time assert for the size requirement.
// If the sizes differs, this statement will try to define a negative-sized array, which is invalid.
typedef char ct_failure[sizeof (SectionInfo) == sizeof (PESectionInfo) ? 1 : -1];

}


Win32BinaryFile::Win32BinaryFile() : m_pFilename(NULL), mingw_main(false)
{
}

Win32BinaryFile::~Win32BinaryFile()
{
	for (int i = 0; i < m_iNumSections; i++) {
		if (m_pSections[i].pSectionName)
			delete [] m_pSections[i].pSectionName;
	}
	if (m_pSections) delete [] m_pSections;
}

bool Win32BinaryFile::Open(const char *sName) {
	//return Load(sName) != 0;
	return false;
}

void Win32BinaryFile::Close()
{
	UnLoad();
}

std::list<SectionInfo *> &Win32BinaryFile::getEntryPoints(const char *pEntry)
{
	fprintf(stderr, "really don't know how to implement getEntryPoints\n");
	exit(0);
	static std::list<SectionInfo *> l;
	return l;
}

ADDRESS Win32BinaryFile::getEntryPoint()
{
	return (ADDRESS)(LMMH(m_pPEHeader->EntrypointRVA)
	               + LMMH(m_pPEHeader->Imagebase));
}

// This is a bit of a hack, but no more than the rest of Windows :-O  The pattern is to look for an indirect call (FF 15
// opcode) to exit; within 10 instructions before that should be the call to WinMain (with no other calls inbetween).
// This pattern should work for "old style" and "new style" PE executables, as well as console mode PE files.
ADDRESS Win32BinaryFile::getMainEntryPoint()
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
	unsigned char op1, op2;
	unsigned addr, lastOrdCall = 0;
	int gap;               // Number of instructions from the last ordinary call
	int borlandState = 0;  // State machine for Borland

	SectionInfo *si = getSectionInfoByName(".text");
	if (si == NULL) si = getSectionInfoByName("CODE");
	assert(si);
	unsigned textSize = si->uSectionSize;
	if (textSize < 0x200)
		lim = p + textSize;

	if (m_pPEHeader->Subsystem == 1)  // native
		return LMMH(m_pPEHeader->EntrypointRVA) + LMMH(m_pPEHeader->Imagebase);

	gap = 0xF0000000;  // Large positive number (in case no ordinary calls)
	while (p < lim) {
		op1 = *(unsigned char *)(p + base);
		op2 = *(unsigned char *)(p + base + 1);
		//std::cerr << std::hex << "At " << p << ", ops " << (unsigned)op1 << ", " << (unsigned)op2 << std::dec << "\n";
		switch (op1) {
		case 0xE8:
			{
				// An ordinary call; this could be to winmain/main
				lastOrdCall = p;
				gap = 0;
				if (borlandState == 1)
					borlandState++;
				else
					borlandState = 0;
			}
			break;
		case 0xFF:
			if (op2 == 0x15) {  // Opcode FF 15 is indirect call
				// Get the 4 byte address from the instruction
				addr = LMMH(*(p + base + 2));
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
			gap++;
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
				borlandState++;
			else
				borlandState = 0;
			break;
		default:
			borlandState = 0;
			break;
		}
		int size = microX86Dis(p + base);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
		gap++;
	}

	// VS.NET release console mode pattern
	p = LMMH(m_pPEHeader->EntrypointRVA);
	if (*(unsigned char *)(p + base + 0x20) == 0xff
	 && *(unsigned char *)(p + base + 0x21) == 0x15) {
		unsigned int desti = LMMH(*(p + base + 0x22));
		if (dlprocptrs.find(desti) != dlprocptrs.end()
		 && dlprocptrs[desti] == "GetVersionExA") {
			if (*(unsigned char *)(p + base + 0x6d) == 0xff
			 && *(unsigned char *)(p + base + 0x6e) == 0x15) {
				desti = LMMH(*(p + base + 0x6f));
				if (dlprocptrs.find(desti) != dlprocptrs.end()
				 && dlprocptrs[desti] == "GetModuleHandleA") {
					if (*(unsigned char *)(p + base + 0x16e) == 0xe8) {
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
		count--;
		op1 = *(unsigned char *)(p + base);
		if (op1 == 0xE8) {  // CALL opcode
			if (pushes == 3) {
				// Get the offset
				int off = LMMH(*(p + base + 1));
				unsigned dest = (unsigned)p + 5 + off;
				// Check for a jump there
				op1 = *(unsigned char *)(dest + base);
				if (op1 == 0xE9) {
					// Follow that jump
					off = LMMH(*(dest + base + 1));
					dest = dest + 5 + off;
				}
				return dest + LMMH(m_pPEHeader->Imagebase);
			} else
				pushes = 0;  // Assume pushes don't accumulate over calls
		} else if (op1 >= 0x50 && op1 <= 0x57) {  // PUSH opcode
			pushes++;
		} else if (op1 == 0xFF) {
			// FF 35 is push m[K]
			op2 = *(unsigned char *)(p + 1 + base);
			if (op2 == 0x35)
				pushes++;
		} else if (op1 == 0xE9) {
			// Follow the jump
			int off = LMMH(*(p + base + 1));
			p += off + 5;
			continue;
		}


		int size = microX86Dis(p + base);
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
		op1 = *(unsigned char *)(p + base);
		if (op1 == 0xE8) {  // CALL opcode
			unsigned int dest = p + 5 + LMMH(*(p + base + 1));
			if (in_mingw_CRTStartup) {
				op2 = *(unsigned char *)(dest + base);
				unsigned char op2a = *(unsigned char *)(dest + base + 1);
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

		int size = microX86Dis(p + base);
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
		op1 = *(unsigned char *)(p + base);
		op2 = *(unsigned char *)(p + base + 1);
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

		int size = microX86Dis(p + base);
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

bool Win32BinaryFile::RealLoad(const char *sName)
{
	m_pFilename = sName;
	FILE *fp = fopen(sName, "rb");

	DWord peoffLE, peoff;
	fseek(fp, 0x3c, SEEK_SET);
	fread(&peoffLE, sizeof peoffLE, 1, fp);  // Note: peoffLE will be in Little Endian
	peoff = LMMH(peoffLE);

	PEHeader tmphdr;

	fseek(fp, peoff, SEEK_SET);
	fread(&tmphdr, sizeof tmphdr, 1, fp);
	// Note: all tmphdr fields will be little endian

	base = (char *)malloc(LMMH(tmphdr.ImageSize));

	if (!base) {
		fprintf(stderr, "Cannot allocate memory for copy of image\n");
		return false;
	}

	fseek(fp, 0, SEEK_SET);

	fread(base, LMMH(tmphdr.HeaderSize), 1, fp);

	m_pHeader = (Header *)base;
	if (m_pHeader->sigLo != 'M' || m_pHeader->sigHi != 'Z') {
		fprintf(stderr, "error loading file %s, bad magic\n", sName);
		return false;
	}

	m_pPEHeader = (PEHeader *)(base + peoff);
	if (m_pPEHeader->sigLo != 'P' || m_pPEHeader->sigHi != 'E') {
		fprintf(stderr, "error loading file %s, bad PE magic\n", sName);
		return false;
	}

//printf("Image Base %08X, real base %p\n", LMMH(m_pPEHeader->Imagebase), base);

	const PEObject *o = (PEObject *)(((char *)m_pPEHeader) + LH(&m_pPEHeader->NtHdrSize) + 24);
	m_iNumSections = LH(&m_pPEHeader->numObjects);
	m_pSections = new PESectionInfo[m_iNumSections];
	//SectionInfo *reloc = NULL;
	for (int i = 0; i < m_iNumSections; i++, o++) {
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
		sect.uHostAddr = (ADDRESS)(LMMH(o->RVA) + base);
		sect.uSectionSize = LMMH(o->VirtualSize);
		DWord Flags = LMMH(o->Flags);
		sect.bBss      = Flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA ? 1 : 0;
		sect.bCode     = Flags & IMAGE_SCN_CNT_CODE               ? 1 : 0;
		sect.bData     = Flags & IMAGE_SCN_CNT_INITIALIZED_DATA   ? 1 : 0;
		sect.bReadOnly = Flags & IMAGE_SCN_MEM_WRITE              ? 0 : 1;
		// TODO: Check for unreadable sections (!IMAGE_SCN_MEM_READ)?
		fseek(fp, LMMH(o->PhysicalOffset), SEEK_SET);
		memset(base + LMMH(o->RVA), 0, LMMH(o->VirtualSize));
		fread(base + LMMH(o->RVA), LMMH(o->PhysicalSize), 1, fp);
		s_sectionObjects[static_cast<const PESectionInfo *>(&sect)] = o;
	}

	// Add the Import Address Table entries to the symbol table
	PEImportDtor *id = (PEImportDtor *)(LMMH(m_pPEHeader->ImportTableRVA) + base);
	if (m_pPEHeader->ImportTableRVA) {  // If any import table entry exists
		while (id->name != 0) {
			char *dllName = LMMH(id->name) + base;
			unsigned thunk = id->originalFirstThunk ? id->originalFirstThunk : id->firstThunk;
			unsigned *iat = (unsigned *)(LMMH(thunk) + base);
			unsigned iatEntry = LMMH(*iat);
			ADDRESS paddr = LMMH(id->firstThunk) + LMMH(m_pPEHeader->Imagebase);
			while (iatEntry) {
				if (iatEntry >> 31) {
					// This is an ordinal number (stupid idea)
					std::ostringstream ost;
					std::string nodots(dllName);
					int len = nodots.size();
					for (int j = 0; j < len; j++)
						if (nodots[j] == '.')
							nodots[j] = '_';  // Dots can't be in identifiers
					ost << nodots << "_" << (iatEntry & 0x7FFFFFFF);
					dlprocptrs[paddr] = ost.str();
					// printf("Added symbol %s value %x\n", ost.str().c_str(), paddr);
				} else {
					// Normal case (IMAGE_IMPORT_BY_NAME). Skip the useless hint (2 bytes)
					std::string name((const char *)(iatEntry + 2 + base));
					dlprocptrs[paddr] = name;
					if ((unsigned)paddr != (unsigned)iat - (unsigned)base + LMMH(m_pPEHeader->Imagebase))
						dlprocptrs[(unsigned)iat - (unsigned)base + LMMH(m_pPEHeader->Imagebase)] = std::string("old_") + name; // add both possibilities
					//printf("Added symbol %s value %x\n", name.c_str(), paddr);
					//printf("Also added old_%s value %x\n", name.c_str(), (int)iat - (int)base + LMMH(m_pPEHeader->Imagebase));
				}
				iat++;
				iatEntry = LMMH(*iat);
				paddr += 4;
			}
			id++;
		}
	}

	// Was hoping that _main or main would turn up here for Borland console mode programs. No such luck.
	// I think IDA Pro must find it by a combination of FLIRT and some pattern matching
	//PEExportDtor *eid = (PEExportDtor *)(LMMH(m_pPEHeader->ExportTableRVA) + base);


	// Give the entry point a symbol
	ADDRESS entry = getMainEntryPoint();
	if (entry != NO_ADDRESS) {
		std::map<ADDRESS, std::string>::iterator it = dlprocptrs.find(entry);
		if (it == dlprocptrs.end())
			dlprocptrs[entry] = "main";
	}

	// Give a name to any jumps you find to these import entries
	// NOTE: VERY early MSVC specific!! Temporary till we can think of a better way.
	ADDRESS start = getEntryPoint();
	findJumps(start);

	fclose(fp);
	return true;
}

// Used above for a hack to find jump instructions pointing to IATs.
// Heuristic: start just before the "start" entry point looking for FF 25 opcodes followed by a pointer to an import
// entry.  E.g. FF 25 58 44 40 00  where 00404458 is the IAT for _ftol.
// Note: some are on 0x10 byte boundaries, some on 2 byte boundaries (6 byte jumps packed), and there are often up to
// 0x30 bytes of statically linked library code (e.g. _atexit, __onexit) with sometimes two static libs in a row.
// So keep going until there is about 0x60 bytes with no match.
// Note: slight chance of coming across a misaligned match; probability is about 1/65536 times dozens in 2^32 ~= 10^-13
void Win32BinaryFile::findJumps(ADDRESS curr)
{
	int cnt = 0;  // Count of bytes with no match
	SectionInfo *sec = getSectionInfoByName(".text");
	if (sec == NULL) sec = getSectionInfoByName("CODE");
	assert(sec);
	// Add to native addr to get host:
	int delta = sec->uHostAddr - sec->uNativeAddr;
	while (cnt < 0x60) {  // Max of 0x60 bytes without a match
		curr -= 2;  // Has to be on 2-byte boundary
		cnt += 2;
		if (LH(delta + curr) != 0xFF + (0x25 << 8)) continue;
		ADDRESS operand = LMMH2(delta + curr + 2);
		std::map<ADDRESS, std::string>::iterator it;
		it = dlprocptrs.find(operand);
		if (it == dlprocptrs.end()) continue;
		std::string sym = it->second;
		dlprocptrs[operand] = "__imp_" + sym;
		dlprocptrs[curr] = sym;   // Add new entry
		// std::cerr << "Added " << sym << " at 0x" << std::hex << curr << "\n";
		curr -= 4;  // Next match is at least 4+2 bytes away
		cnt = 0;
	}
}

// Clean up and unload the binary image
void Win32BinaryFile::UnLoad()
{
}

bool Win32BinaryFile::PostLoad(void *handle)
{
	return false;
}

const char *Win32BinaryFile::getSymbolByAddress(ADDRESS dwAddr)
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

	std::map<ADDRESS, std::string>::iterator it = dlprocptrs.find(dwAddr);
	if (it == dlprocptrs.end())
		return 0;
	return it->second.c_str();
}

ADDRESS Win32BinaryFile::getAddressByName(const char *pName, bool bNoTypeOK /* = false */)
{
	// This is "looking up the wrong way" and hopefully is uncommon.  Use linear search
	std::map<ADDRESS, std::string>::iterator it = dlprocptrs.begin();
	while (it != dlprocptrs.end()) {
		// std::cerr << "Symbol: " << it->second.c_str() << " at 0x" << std::hex << it->first << "\n";
		if (it->second == pName)
			return it->first;
		it++;
	}
	return NO_ADDRESS;
}

void Win32BinaryFile::addSymbol(ADDRESS uNative, const char *pName)
{
	dlprocptrs[uNative] = pName;
}

bool Win32BinaryFile::DisplayDetails(const char *fileName, FILE *f /* = stdout */)
{
	return false;
}

int Win32BinaryFile::win32Read2(short *ps) const
{
	unsigned char *p = (unsigned char *)ps;
	// Little endian
	int n = (int)(p[0] + (p[1] << 8));
	return n;
}

int Win32BinaryFile::win32Read4(int *pi) const
{
	short *p = (short *)pi;
	int n1 = win32Read2(p);
	int n2 = win32Read2(p + 1);
	int n = (int)(n1 | (n2 << 16));
	return n;
}

// Read 2 bytes from given native address
int Win32BinaryFile::readNative1(ADDRESS nat)
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0)
		return -1;
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	return *(char *)host;
}

// Read 2 bytes from given native address
int Win32BinaryFile::readNative2(ADDRESS nat)
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0) return 0;
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	int n = win32Read2((short *)host);
	return n;
}

// Read 4 bytes from given native address
int Win32BinaryFile::readNative4(ADDRESS nat)
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0) return 0;
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	int n = win32Read4((int *)host);
	return n;
}

// Read 8 bytes from given native address
QWord Win32BinaryFile::readNative8(ADDRESS nat)
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

// Read 4 bytes as a float
float Win32BinaryFile::readNativeFloat4(ADDRESS nat) {
	int raw = readNative4(nat);
	// Ugh! gcc says that reinterpreting from int to float is invalid!!
	//return reinterpret_cast<float>(raw);  // Note: cast, not convert!!
	return *(float *)&raw;  // Note: cast, not convert
}

// Read 8 bytes as a float
double Win32BinaryFile::readNativeFloat8(ADDRESS nat) {
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

bool Win32BinaryFile::isDynamicLinkedProcPointer(ADDRESS uNative)
{
	if (dlprocptrs.find(uNative) != dlprocptrs.end())
		return true;
	return false;
}

bool Win32BinaryFile::isStaticLinkedLibProc(ADDRESS uNative)
{
	if (isMinGWsAllocStack(uNative)
	 || isMinGWsFrameInit(uNative)
	 || isMinGWsFrameEnd(uNative)
	 || isMinGWsCleanupSetup(uNative)
	 || isMinGWsMalloc(uNative))
		return true;
	return false;
}

bool Win32BinaryFile::isMinGWsAllocStack(ADDRESS uNative)
{
	if (mingw_main) {
		SectionInfo *si = getSectionInfoByAddr(uNative);
		if (si) {
			ADDRESS host = si->uHostAddr - si->uNativeAddr + uNative;
			unsigned char pat[] = {
				0x51, 0x89, 0xE1, 0x83, 0xC1, 0x08, 0x3D, 0x00,
				0x10, 0x00, 0x00, 0x72, 0x10, 0x81, 0xE9, 0x00,
				0x10, 0x00, 0x00, 0x83, 0x09, 0x00, 0x2D, 0x00,
				0x10, 0x00, 0x00, 0xEB, 0xE9, 0x29, 0xC1, 0x83,
				0x09, 0x00, 0x89, 0xE0, 0x89, 0xCC, 0x8B, 0x08,
				0x8B, 0x40, 0x04, 0xFF, 0xE0
			};
			if (memcmp((void *)host, pat, sizeof pat) == 0) {
				return true;
			}
		}
	}
	return false;
}

bool Win32BinaryFile::isMinGWsFrameInit(ADDRESS uNative)
{
	if (mingw_main) {
		SectionInfo *si = getSectionInfoByAddr(uNative);
		if (si) {
			ADDRESS host = si->uHostAddr - si->uNativeAddr + uNative;
			unsigned char pat1[] = {
				0x55, 0x89, 0xE5, 0x83, 0xEC, 0x18, 0x89, 0x7D,
				0xFC, 0x8B, 0x7D, 0x08, 0x89, 0x5D, 0xF4, 0x89,
				0x75, 0xF8
			};
			if (memcmp((void *)host, pat1, sizeof pat1) == 0) {
				unsigned char pat2[] = {
					0x85, 0xD2, 0x74, 0x24, 0x8B, 0x42, 0x2C, 0x85,
					0xC0, 0x78, 0x3D, 0x8B, 0x42, 0x2C, 0x85, 0xC0,
					0x75, 0x56, 0x8B, 0x42, 0x28, 0x89, 0x07, 0x89,
					0x7A, 0x28, 0x8B, 0x5D, 0xF4, 0x8B, 0x75, 0xF8,
					0x8B, 0x7D, 0xFC, 0x89, 0xEC, 0x5D, 0xC3
				};
				if (memcmp((void *)(host + sizeof pat1 + 6), pat2, sizeof pat2) == 0) {
					return true;
				}
			}
		}
	}
	return false;
}

bool Win32BinaryFile::isMinGWsFrameEnd(ADDRESS uNative)
{
	if (mingw_main) {
		SectionInfo *si = getSectionInfoByAddr(uNative);
		if (si) {
			ADDRESS host = si->uHostAddr - si->uNativeAddr + uNative;
			unsigned char pat1[] = {
				0x55, 0x89, 0xE5, 0x53, 0x83, 0xEC, 0x14, 0x8B, 0x45, 0x08, 0x8B, 0x18
			};
			if (memcmp((void *)host, pat1, sizeof pat1) == 0) {
				unsigned char pat2[] = {
					0x85, 0xC0, 0x74, 0x1B, 0x8B, 0x48, 0x2C, 0x85, 0xC9, 0x78, 0x34, 0x8B,
					0x50, 0x2C, 0x85, 0xD2, 0x75, 0x4D, 0x89, 0x58, 0x28, 0x8B, 0x5D, 0xFC,
					0xC9, 0xC3
				};
				if (memcmp((void *)(host + sizeof pat1 + 5), pat2, sizeof pat2) == 0) {
					return true;
				}
			}
		}
	}
	return false;
}

bool Win32BinaryFile::isMinGWsCleanupSetup(ADDRESS uNative)
{
	if (mingw_main) {
		SectionInfo *si = getSectionInfoByAddr(uNative);
		if (si) {
			ADDRESS host = si->uHostAddr - si->uNativeAddr + uNative;
			unsigned char pat1[] = {
				0x55, 0x89, 0xE5, 0x53, 0x83, 0xEC, 0x04
			};
			if (memcmp((void *)host, pat1, sizeof pat1) == 0) {
				unsigned char pat2[] = {
					0x85, 0xDB, 0x75, 0x35
				};
				if (memcmp((void *)(host + sizeof pat1 + 6), pat2, sizeof pat2) == 0) {
					unsigned char pat3[] = {
						0x83, 0xF8, 0xFF, 0x74, 0x24, 0x85, 0xC0, 0x89, 0xC3, 0x74, 0x0E,
						0x8D, 0x74, 0x26, 0x00
					};
					if (memcmp((void *)(host + sizeof pat1 + 6 + sizeof pat2 + 16), pat3, sizeof pat3) == 0) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool Win32BinaryFile::isMinGWsMalloc(ADDRESS uNative)
{
	if (mingw_main) {
		SectionInfo *si = getSectionInfoByAddr(uNative);
		if (si) {
			ADDRESS host = si->uHostAddr - si->uNativeAddr + uNative;
			unsigned char pat1[] = {
				0x55, 0x89, 0xE5, 0x8D, 0x45, 0xF4, 0x83, 0xEC, 0x58, 0x89, 0x45, 0xE0, 0x8D, 0x45, 0xC0, 0x89,
				0x04, 0x24, 0x89, 0x5D, 0xF4, 0x89, 0x75, 0xF8, 0x89, 0x7D, 0xFC
			};
			if (memcmp((void *)host, pat1, sizeof pat1) == 0) {
				unsigned char pat2[] = {
					0x89, 0x65, 0xE8
				};
				if (memcmp((void *)(host + sizeof pat1 + 0x15), pat2, sizeof pat2) == 0) {
					return true;
				}
			}
		}
	}
	return false;
}

ADDRESS Win32BinaryFile::isJumpToAnotherAddr(ADDRESS uNative)
{
	if ((readNative1(uNative) & 0xff) != 0xe9)
		return NO_ADDRESS;
	return readNative4(uNative + 1) + uNative + 5;
}

const char *Win32BinaryFile::getDynamicProcName(ADDRESS uNative)
{
	return dlprocptrs[uNative].c_str();
}

bool Win32BinaryFile::isLibrary() const
{
	return (m_pPEHeader->Flags & 0x2000) != 0;
}

ADDRESS Win32BinaryFile::getImageBase()
{
	return m_pPEHeader->Imagebase;
}

size_t Win32BinaryFile::getImageSize()
{
	return m_pPEHeader->ImageSize;
}

std::list<const char *> Win32BinaryFile::getDependencyList()
{
	return std::list<const char *>(); /* FIXME */
}

DWord Win32BinaryFile::getDelta()
{
	// Stupid function anyway: delta depends on section
	// This should work for the header only
	//return (DWord)base - LMMH(m_pPEHeader->Imagebase);
	return (DWord)base - (DWord)m_pPEHeader->Imagebase;
}

/**
 * This function is called via dlopen/dlsym; it returns a new BinaryFile
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" BinaryFile *construct()
{
	return new Win32BinaryFile;
}

void Win32BinaryFile::dumpSymbols()
{
	std::map<ADDRESS, std::string>::iterator it;
	std::cerr << std::hex;
	for (it = dlprocptrs.begin(); it != dlprocptrs.end(); ++it)
		std::cerr << "0x" << it->first << " " << it->second << "        ";
	std::cerr << std::dec << "\n";
}
