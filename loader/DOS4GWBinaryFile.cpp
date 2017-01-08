/**
 * \file
 * \brief Contains the implementation of the class DOS4GWBinaryFile.
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

#include "DOS4GWBinaryFile.h"

#include <cassert>
#include <cstdio>

extern "C" size_t microX86Dis(const unsigned char *p);  // From microX86dis.c

DOS4GWBinaryFile::DOS4GWBinaryFile() :
	m_pLXHeader(NULL),
	m_pLXObjects(NULL),
	m_pLXPages(NULL),
	base(NULL)
{
}

DOS4GWBinaryFile::~DOS4GWBinaryFile()
{
	for (int i = 0; i < m_iNumSections; i++)
		delete [] m_pSections[i].pSectionName;
	delete [] m_pSections;
	delete    m_pLXHeader;
	delete [] m_pLXObjects;
	delete [] m_pLXPages;
	delete [] base;
}

ADDRESS DOS4GWBinaryFile::getEntryPoint()
{
	return (ADDRESS)(LMMH(m_pLXObjects[LMMH(m_pLXHeader->eipobjectnum)].RelocBaseAddr) + LMMH(m_pLXHeader->eip));
}

ADDRESS DOS4GWBinaryFile::getMainEntryPoint()
{
	ADDRESS aMain = getAddressByName("main", true);
	if (aMain != NO_ADDRESS)
		return aMain;
	aMain = getAddressByName("__CMain", true);
	if (aMain != NO_ADDRESS)
		return aMain;

	// Search with this crude pattern: call, sub ebp, ebp, call __Cmain in the first 0x300 bytes
	// Start at program entry point
	unsigned p = LMMH(m_pLXHeader->eip);
	unsigned lim = p + 0x300;
	unsigned char op1, op2;
	unsigned addr, lastOrdCall = 0;
	bool gotSubEbp = false;         // True if see sub ebp, ebp
	bool lastWasCall = false;       // True if the last instruction was a call

	SectionInfo *si = getSectionInfoByName("seg0");     // Assume the first section is text
	if (si == NULL) si = getSectionInfoByName(".text");
	if (si == NULL) si = getSectionInfoByName("CODE");
	assert(si);
	ADDRESS nativeOrigin = si->uNativeAddr;
	unsigned textSize = si->uSectionSize;
	if (textSize < 0x300)
		lim = p + textSize;

	while (p < lim) {
		op1 = *(p + base);
		op2 = *(p + base + 1);
		//std::cerr << std::hex << "At " << p << ", ops " << (unsigned)op1 << ", " << (unsigned)op2 << std::dec << "\n";
		switch (op1) {
		case 0xE8:
			{
				// An ordinary call
				if (gotSubEbp) {
					// This is the call we want. Get the offset from the call instruction
					addr = nativeOrigin + p + 5 + LMMH(*(p + base + 1));
					// std::cerr << "__CMain at " << std::hex << addr << "\n";
					return addr;
				}
				lastOrdCall = p;
				lastWasCall = true;
			}
			break;
		case 0x2B:  // 0x2B 0xED is sub ebp,ebp
			if (op2 == 0xED && lastWasCall)
				gotSubEbp = true;
			lastWasCall = false;
			break;
		default:
			gotSubEbp = false;
			lastWasCall = false;
			break;
		case 0xEB:  // Short relative jump
			if (op2 >= 0x80)  // Branch backwards?
				break;  // Yes, just ignore it
			// Otherwise, actually follow the branch. May have to modify this some time...
			p += op2 + 2;  // +2 for the instruction itself, and op2 for the displacement
			continue;      // Don't break, we have the new "pc" set already
		}
		size_t size = microX86Dis(p + base);
		if (size == 0x40) {
			fprintf(stderr, "Warning! Microdisassembler out of step at offset 0x%x\n", p);
			size = 1;
		}
		p += size;
	}
	return NO_ADDRESS;
}

bool DOS4GWBinaryFile::load(std::istream &ifs)
{
	DWord lxoffLE, lxoff;
	ifs.seekg(0x3c);
	ifs.read((char *)&lxoffLE, sizeof lxoffLE);  // Note: peoffLE will be in Little Endian
	lxoff = LMMH(lxoffLE);

	m_pLXHeader = new LXHeader;
	ifs.seekg(lxoff);
	ifs.read((char *)m_pLXHeader, sizeof *m_pLXHeader);

	if (m_pLXHeader->sigLo != 'L' || (m_pLXHeader->sigHi != 'X' && m_pLXHeader->sigHi != 'E')) {
		fprintf(stderr, "error loading file %s, bad LE/LX magic\n", getFilename());
		return false;
	}

	m_pLXObjects = new LXObject[LMMH(m_pLXHeader->numobjsinmodule)];
	ifs.seekg(lxoff + LMMH(m_pLXHeader->objtbloffset));
	ifs.read((char *)m_pLXObjects, sizeof *m_pLXObjects * LMMH(m_pLXHeader->numobjsinmodule));

	// at this point we're supposed to read in the page table and fuss around with it
	// but I'm just going to assume the file is flat.
#if 0
	unsigned npagetblentries = 0;
	m_cbImage = 0;
	for (unsigned n = 0; n < LMMH(m_pLXHeader->numobjsinmodule); n++) {
		if (LMMH(m_pLXObjects[n].PageTblIdx) + LMMH(m_pLXObjects[n].NumPageTblEntries) - 1 > npagetblentries)
			npagetblentries = LMMH(m_pLXObjects[n].PageTblIdx) + LMMH(m_pLXObjects[n].NumPageTblEntries) - 1;
		if (LMMH(m_pLXObjects[n].ObjectFlags) & 0x40)
			if (LMMH(m_pLXObjects[n].RelocBaseAddr) + LMMH(m_pLXObjects[n].VirtualSize) > m_cbImage)
				m_cbImage = LMMH(m_pLXObjects[n].RelocBaseAddr) + LMMH(m_pLXObjects[n].VirtualSize);
	}
	m_cbImage -= LMMH(m_pLXObjects[0].RelocBaseAddr);

	m_pLXPages = new LXPage[npagetblentries];
	ifs.seekg(lxoff + LMMH(m_pLXHeader->objpagetbloffset));
	ifs.read((char *)m_pLXPages, sizeof *m_pLXPages * npagetblentries);
#endif

	unsigned npages = 0;
	m_cbImage = 0;
	for (unsigned n = 0; n < LMMH(m_pLXHeader->numobjsinmodule); n++)
		if (LMMH(m_pLXObjects[n].ObjectFlags) & 0x40) {
			if (LMMH(m_pLXObjects[n].PageTblIdx) + LMMH(m_pLXObjects[n].NumPageTblEntries) - 1 > npages)
				npages = LMMH(m_pLXObjects[n].PageTblIdx) + LMMH(m_pLXObjects[n].NumPageTblEntries) - 1;
			m_cbImage = LMMH(m_pLXObjects[n].RelocBaseAddr) + LMMH(m_pLXObjects[n].VirtualSize);
		}

	m_cbImage -= LMMH(m_pLXObjects[0].RelocBaseAddr);

	base = new unsigned char[m_cbImage];

	m_iNumSections = LMMH(m_pLXHeader->numobjsinmodule);
	m_pSections = new SectionInfo[m_iNumSections];
	for (unsigned n = 0; n < LMMH(m_pLXHeader->numobjsinmodule); n++)
		if (LMMH(m_pLXObjects[n].ObjectFlags) & 0x40) {
			printf("vsize %x reloc %x flags %x page %i npage %i\n",
			       LMMH(m_pLXObjects[n].VirtualSize),
			       LMMH(m_pLXObjects[n].RelocBaseAddr),
			       LMMH(m_pLXObjects[n].ObjectFlags),
			       LMMH(m_pLXObjects[n].PageTblIdx),
			       LMMH(m_pLXObjects[n].NumPageTblEntries));

			char *name = new char[9];
			snprintf(name, sizeof (char[9]), "seg%i", n);  // no section names in LX
			m_pSections[n].pSectionName = name;
			m_pSections[n].uNativeAddr = (ADDRESS)LMMH(m_pLXObjects[n].RelocBaseAddr);
			m_pSections[n].uHostAddr = (ADDRESS)(LMMH(m_pLXObjects[n].RelocBaseAddr) - LMMH(m_pLXObjects[0].RelocBaseAddr) + base);
			m_pSections[n].uSectionSize = LMMH(m_pLXObjects[n].VirtualSize);
			DWord Flags = LMMH(m_pLXObjects[n].ObjectFlags);
			m_pSections[n].bBss      = false; // TODO
			m_pSections[n].bCode     = (Flags & 0x4) != 0;
			m_pSections[n].bData     = (Flags & 0x4) == 0;
			m_pSections[n].bReadOnly = (Flags & 0x1) == 0;

			unsigned char *p = base + LMMH(m_pLXObjects[n].RelocBaseAddr) - LMMH(m_pLXObjects[0].RelocBaseAddr);
			ifs.seekg(m_pLXHeader->datapagesoffset + (LMMH(m_pLXObjects[n].PageTblIdx) - 1) * LMMH(m_pLXHeader->pagesize));
			ifs.read((char *)p, LMMH(m_pLXHeader->pagesize) * LMMH(m_pLXObjects[n].NumPageTblEntries));
		}

	// TODO: decode entry tables


#if 0
	// you probably don't want this, it's a bunch of symbols I pulled out of a disassmbly of a binary I'm working on.
	dlprocptrs[0x101ac] = "main";
	dlprocptrs[0x10a24] = "vfprintf";
	dlprocptrs[0x12d2c] = "atoi";
	dlprocptrs[0x12d74] = "malloc";
	dlprocptrs[0x12d84] = "__LastFree";
	dlprocptrs[0x12eaa] = "__ExpandDGROUP";
	dlprocptrs[0x130a7] = "free";
	dlprocptrs[0x130b7] = "_nfree";
	dlprocptrs[0x130dc] = "start";
	dlprocptrs[0x132fa] = "__exit_";
	dlprocptrs[0x132fc] = "__exit_with_msg_";
	dlprocptrs[0x1332a] = "__GETDS";
	dlprocptrs[0x13332] = "inp";
	dlprocptrs[0x1333d] = "outp";
	dlprocptrs[0x13349] = "_dos_getvect";
	dlprocptrs[0x13383] = "_dos_setvect";
	dlprocptrs[0x133ba] = "int386";
	dlprocptrs[0x133f9] = "sprintf";
	dlprocptrs[0x13423] = "vsprintf";
	dlprocptrs[0x13430] = "segread";
	dlprocptrs[0x1345d] = "int386x";
	dlprocptrs[0x1347e] = "creat";
	dlprocptrs[0x13493] = "setmode";
	dlprocptrs[0x1355f] = "close";
	dlprocptrs[0x1384a] = "read";
	dlprocptrs[0x13940] = "write";
	dlprocptrs[0x13b2e] = "filelength";
	dlprocptrs[0x13b74] = "printf";
	dlprocptrs[0x13b94] = "__null_int23_exit";
	dlprocptrs[0x13b95] = "exit";
	dlprocptrs[0x13bad] = "_exit";
	dlprocptrs[0x13bc4] = "tell";
	dlprocptrs[0x13cba] = "rewind";
	dlprocptrs[0x13cd3] = "fread";
	dlprocptrs[0x13fe1] = "strcat";
	dlprocptrs[0x1401c] = "__open_flags";
	dlprocptrs[0x141a8] = "fopen";
	dlprocptrs[0x141d0] = "freopen";
	dlprocptrs[0x142c4] = "__MemAllocator";
	dlprocptrs[0x14374] = "__MemFree";
	dlprocptrs[0x1447f] = "__nmemneed";
	dlprocptrs[0x14487] = "sbrk";
	dlprocptrs[0x14524] = "__brk";
	dlprocptrs[0x145d0] = "__CMain";
	dlprocptrs[0x145ff] = "_init_files";
	dlprocptrs[0x1464c] = "__InitRtns";
	dlprocptrs[0x1468b] = "__FiniRtns";
	dlprocptrs[0x146ca] = "__prtf";
	dlprocptrs[0x14f58] = "__int386x_";
	dlprocptrs[0x14fb3] = "_DoINTR_";
	dlprocptrs[0x15330] = "__Init_Argv";
	dlprocptrs[0x15481] = "isatty";
	dlprocptrs[0x154a1] = "_dosret0";
	dlprocptrs[0x154bd] = "_dsretax";
	dlprocptrs[0x154d4] = "_EINVAL";
	dlprocptrs[0x154e5] = "_set_errno";
	dlprocptrs[0x15551] = "__CHK";
	dlprocptrs[0x15561] = "__STK";
	dlprocptrs[0x15578] = "__STKOVERFLOW";
	dlprocptrs[0x15588] = "__GRO";
	dlprocptrs[0x155c2] = "__fprtf";
	dlprocptrs[0x15640] = "__ioalloc";
	dlprocptrs[0x156c3] = "__chktty";
	dlprocptrs[0x156f0] = "__qread";
	dlprocptrs[0x15721] = "fgetc";
	dlprocptrs[0x1578f] = "__filbuf";
	dlprocptrs[0x157b9] = "__fill_buffer";
	dlprocptrs[0x15864] = "fflush";
	dlprocptrs[0x1591c] = "ftell";
	dlprocptrs[0x15957] = "tolower";
	dlprocptrs[0x159b4] = "remove";
	dlprocptrs[0x159e9] = "utoa";
	dlprocptrs[0x15a36] = "itoa";
	dlprocptrs[0x15a97] = "ultoa";
	dlprocptrs[0x15ae4] = "ltoa";
	dlprocptrs[0x15b14] = "toupper";
	dlprocptrs[0x15b29] = "fputc";
	dlprocptrs[0x15bca] = "__full_io_exit";
	dlprocptrs[0x15c0b] = "fcloseall";
	dlprocptrs[0x15c3e] = "flushall";
	dlprocptrs[0x15c49] = "__flushall";
	dlprocptrs[0x15c85] = "getche";
	dlprocptrs[0x15caa] = "__qwrite";
	dlprocptrs[0x15d1f] = "unlink";
	dlprocptrs[0x15d43] = "putch";
#endif

	// fixups
	unsigned int *fixuppagetbl = new unsigned int[npages + 1];
	ifs.seekg(LMMH(m_pLXHeader->fixuppagetbloffset) + lxoff);
	ifs.read((char *)fixuppagetbl, sizeof *fixuppagetbl * (npages + 1));

#if 0
	for (unsigned n = 0; n < npages; n++)
		printf("offset for page %i: %x\n", n + 1, fixuppagetbl[n]);
	printf("offset to end of fixup rec: %x\n", fixuppagetbl[npages]);
#endif

	ifs.seekg(LMMH(m_pLXHeader->fixuprecordtbloffset) + lxoff);
	LXFixup fixup;
	unsigned srcpage = 0;
	do {
		ifs.read((char *)&fixup, sizeof fixup);
		if (fixup.src != 7 || (fixup.flags & ~0x50)) {
			fprintf(stderr, "unknown fixup type %02x %02x\n", fixup.src, fixup.flags);
			return false;
		}
		//printf("srcpage = %i srcoff = %x object = %02x trgoff = %x\n", srcpage + 1, fixup.srcoff, fixup.object, fixup.trgoff);
		unsigned long src = srcpage * LMMH(m_pLXHeader->pagesize) + (short)LMMHw(fixup.srcoff);
		unsigned short object = 0;
		if (fixup.flags & 0x40)
			ifs.read((char *)&object, 2);
		else
			ifs.read((char *)&object, 1);
		unsigned int trgoff = 0;
		if (fixup.flags & 0x10)
			ifs.read((char *)&trgoff, 4);
		else
			ifs.read((char *)&trgoff, 2);
		unsigned long target = LMMH(m_pLXObjects[object - 1].RelocBaseAddr) + LMMHw(trgoff);
		//printf("relocate dword at %x to point to %x\n", src, target);
		*(unsigned int *)(base + src) = target;

		while ((std::streamsize)ifs.tellg() - (LMMH(m_pLXHeader->fixuprecordtbloffset) + lxoff) >= LMMH(fixuppagetbl[srcpage + 1]))
			srcpage++;
	} while (srcpage < npages);

	return true;
}

bool DOS4GWBinaryFile::isDynamicLinkedProc(ADDRESS uNative)
{
	if (dlprocptrs.find(uNative) != dlprocptrs.end()
	 && dlprocptrs[uNative] != "main"
	 && dlprocptrs[uNative] != "_start")
		return true;
	return false;
}

#if 0 // Cruft?
bool DOS4GWBinaryFile::PostLoad(void *handle)
{
	return false;
}
#endif

const char *DOS4GWBinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	std::map<ADDRESS, std::string>::iterator it = dlprocptrs.find(dwAddr);
	if (it == dlprocptrs.end())
		return 0;
	return it->second.c_str();
}

ADDRESS DOS4GWBinaryFile::getAddressByName(const char *pName, bool bNoTypeOK /* = false */)
{
	// This is "looking up the wrong way" and hopefully is uncommon
	// Use linear search
	std::map<ADDRESS, std::string>::iterator it = dlprocptrs.begin();
	while (it != dlprocptrs.end()) {
		// std::cerr << "Symbol: " << it->second.c_str() << " at 0x" << std::hex << it->first << "\n";
		if (it->second == pName)
			return it->first;
		it++;
	}
	return NO_ADDRESS;
}

void DOS4GWBinaryFile::addSymbol(ADDRESS uNative, const char *pName)
{
	dlprocptrs[uNative] = pName;
}

/**
 * \brief Read 2 bytes from native addr.
 */
int DOS4GWBinaryFile::dos4gwRead2(short *ps) const
{
	unsigned char *p = (unsigned char *)ps;
	// Little endian
	int n = (int)(p[0] + (p[1] << 8));
	return n;
}

/**
 * \brief Read 4 bytes from native addr.
 */
int DOS4GWBinaryFile::dos4gwRead4(int *pi) const
{
	short *p = (short *)pi;
	int n1 = dos4gwRead2(p);
	int n2 = dos4gwRead2(p + 1);
	int n = (int)(n1 | (n2 << 16));
	return n;
}

int DOS4GWBinaryFile::readNative1(ADDRESS nat) const
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0)
		si = getSectionInfo(0);
	char *host = (char *)(si->uHostAddr - si->uNativeAddr + nat);
	return *host;
}

int DOS4GWBinaryFile::readNative2(ADDRESS nat) const
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0) return 0;
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	return dos4gwRead2((short *)host);
}

int DOS4GWBinaryFile::readNative4(ADDRESS nat) const
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0) return 0;
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	return dos4gwRead4((int *)host);
}

QWord DOS4GWBinaryFile::readNative8(ADDRESS nat) const
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

float DOS4GWBinaryFile::readNativeFloat4(ADDRESS nat) const
{
	int raw = readNative4(nat);
	// Ugh! gcc says that reinterpreting from int to float is invalid!!
	//return reinterpret_cast<float>(raw);  // Note: cast, not convert!!
	return *(float *)&raw;  // Note: cast, not convert
}

double DOS4GWBinaryFile::readNativeFloat8(ADDRESS nat) const
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

bool DOS4GWBinaryFile::isDynamicLinkedProcPointer(ADDRESS uNative)
{
	if (dlprocptrs.find(uNative) != dlprocptrs.end())
		return true;
	return false;
}

const char *DOS4GWBinaryFile::getDynamicProcName(ADDRESS uNative)
{
	return dlprocptrs[uNative].c_str();
}

bool DOS4GWBinaryFile::isLibrary() const
{
	return false; // TODO
}

ADDRESS DOS4GWBinaryFile::getImageBase() const
{
	return m_pLXObjects[0].RelocBaseAddr;
}

size_t DOS4GWBinaryFile::getImageSize() const
{
	return 0; // TODO
}

std::list<const char *> DOS4GWBinaryFile::getDependencyList()
{
	return std::list<const char *>(); /* FIXME */
}

#if 0 // Cruft?
DWord DOS4GWBinaryFile::getDelta()
{
	// Stupid function anyway: delta depends on section
	// This should work for the header only
	//return (DWord)base - LMMH(m_pPEHeader->Imagebase);
	return (DWord)base - (DWord)m_pLXObjects[0].RelocBaseAddr;
}
#endif

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new BinaryFile
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" BinaryFile *construct()
{
	return new DOS4GWBinaryFile();
}
extern "C" void destruct(BinaryFile *bf)
{
	delete (DOS4GWBinaryFile *)bf;
}
#endif
