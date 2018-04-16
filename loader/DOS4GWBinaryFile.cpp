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

DOS4GWBinaryFile::DOS4GWBinaryFile()
{
}

DOS4GWBinaryFile::~DOS4GWBinaryFile()
{
	delete [] m_pSections;
	delete    m_pLXHeader;
	delete [] m_pLXObjects;
	delete [] m_pLXPages;
	delete [] base;
}

ADDRESS
DOS4GWBinaryFile::getEntryPoint() const
{
	return (ADDRESS)(m_pLXObjects[m_pLXHeader->eipobjectnum].RelocBaseAddr + m_pLXHeader->eip);
}

ADDRESS
DOS4GWBinaryFile::getMainEntryPoint()
{
	ADDRESS aMain = getAddressByName("main", true);
	if (aMain != NO_ADDRESS)
		return aMain;
	aMain = getAddressByName("__CMain", true);
	if (aMain != NO_ADDRESS)
		return aMain;

	// Search with this crude pattern: call, sub ebp, ebp, call __Cmain in the first 0x300 bytes
	// Start at program entry point
	unsigned p = m_pLXHeader->eip;
	unsigned lim = p + 0x300;
	unsigned lastOrdCall = 0;
	bool gotSubEbp = false;         // True if see sub ebp, ebp
	bool lastWasCall = false;       // True if the last instruction was a call

	const SectionInfo *si = getSectionInfoByName("seg0");     // Assume the first section is text
	if (!si) si = getSectionInfoByName(".text");
	if (!si) si = getSectionInfoByName("CODE");
	assert(si);
	ADDRESS nativeOrigin = si->uNativeAddr;
	unsigned textSize = si->uSectionSize;
	if (textSize < 0x300)
		lim = p + textSize;

	while (p < lim) {
		unsigned char op1 = *(p + base);
		unsigned char op2 = *(p + base + 1);
		//std::cerr << std::hex << "At " << p << ", ops " << (unsigned)op1 << ", " << (unsigned)op2 << std::dec << "\n";
		switch (op1) {
		case 0xE8:  // An ordinary call
			if (gotSubEbp) {
				// This is the call we want. Get the offset from the call instruction
				unsigned addr = nativeOrigin + p + 5 + LMMH(*(p + base + 1));
				// std::cerr << "__CMain at " << std::hex << addr << "\n";
				return addr;
			}
			lastOrdCall = p;
			lastWasCall = true;
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

bool
DOS4GWBinaryFile::load(std::istream &ifs)
{
	DWord lxoff;
	ifs.seekg(0x3c);
	ifs.read((char *)&lxoff, sizeof lxoff);
	lxoff = LMMH(lxoff);

	m_pLXHeader = new LXHeader;
	ifs.seekg(lxoff);
	ifs.read((char *)m_pLXHeader, sizeof *m_pLXHeader);
	m_pLXHeader->formatlvl               = LMMH(m_pLXHeader->formatlvl);
	m_pLXHeader->cputype                 = LMMH(m_pLXHeader->cputype);
	m_pLXHeader->ostype                  = LMMH(m_pLXHeader->ostype);
	m_pLXHeader->modulever               = LMMH(m_pLXHeader->modulever);
	m_pLXHeader->moduleflags             = LMMH(m_pLXHeader->moduleflags);
	m_pLXHeader->modulenumpages          = LMMH(m_pLXHeader->modulenumpages);
	m_pLXHeader->eipobjectnum            = LMMH(m_pLXHeader->eipobjectnum);
	m_pLXHeader->eip                     = LMMH(m_pLXHeader->eip);
	m_pLXHeader->espobjectnum            = LMMH(m_pLXHeader->espobjectnum);
	m_pLXHeader->esp                     = LMMH(m_pLXHeader->esp);
	m_pLXHeader->pagesize                = LMMH(m_pLXHeader->pagesize);
	m_pLXHeader->pageoffsetshift         = LMMH(m_pLXHeader->pageoffsetshift);
	m_pLXHeader->fixupsectionsize        = LMMH(m_pLXHeader->fixupsectionsize);
	m_pLXHeader->fixupsectionchksum      = LMMH(m_pLXHeader->fixupsectionchksum);
	m_pLXHeader->loadersectionsize       = LMMH(m_pLXHeader->loadersectionsize);
	m_pLXHeader->loadersectionchksum     = LMMH(m_pLXHeader->loadersectionchksum);
	m_pLXHeader->objtbloffset            = LMMH(m_pLXHeader->objtbloffset);
	m_pLXHeader->numobjsinmodule         = LMMH(m_pLXHeader->numobjsinmodule);
	m_pLXHeader->objpagetbloffset        = LMMH(m_pLXHeader->objpagetbloffset);
	m_pLXHeader->objiterpagesoffset      = LMMH(m_pLXHeader->objiterpagesoffset);
	m_pLXHeader->resourcetbloffset       = LMMH(m_pLXHeader->resourcetbloffset);
	m_pLXHeader->numresourcetblentries   = LMMH(m_pLXHeader->numresourcetblentries);
	m_pLXHeader->residentnametbloffset   = LMMH(m_pLXHeader->residentnametbloffset);
	m_pLXHeader->entrytbloffset          = LMMH(m_pLXHeader->entrytbloffset);
	m_pLXHeader->moduledirectivesoffset  = LMMH(m_pLXHeader->moduledirectivesoffset);
	m_pLXHeader->nummoduledirectives     = LMMH(m_pLXHeader->nummoduledirectives);
	m_pLXHeader->fixuppagetbloffset      = LMMH(m_pLXHeader->fixuppagetbloffset);
	m_pLXHeader->fixuprecordtbloffset    = LMMH(m_pLXHeader->fixuprecordtbloffset);
	m_pLXHeader->importtbloffset         = LMMH(m_pLXHeader->importtbloffset);
	m_pLXHeader->numimportmoduleentries  = LMMH(m_pLXHeader->numimportmoduleentries);
	m_pLXHeader->importproctbloffset     = LMMH(m_pLXHeader->importproctbloffset);
	m_pLXHeader->perpagechksumoffset     = LMMH(m_pLXHeader->perpagechksumoffset);
	m_pLXHeader->datapagesoffset         = LMMH(m_pLXHeader->datapagesoffset);
	m_pLXHeader->numpreloadpages         = LMMH(m_pLXHeader->numpreloadpages);
	m_pLXHeader->nonresnametbloffset     = LMMH(m_pLXHeader->nonresnametbloffset);
	m_pLXHeader->nonresnametbllen        = LMMH(m_pLXHeader->nonresnametbllen);
	m_pLXHeader->nonresnametblchksum     = LMMH(m_pLXHeader->nonresnametblchksum);
	m_pLXHeader->autodsobjectnum         = LMMH(m_pLXHeader->autodsobjectnum);
	m_pLXHeader->debuginfooffset         = LMMH(m_pLXHeader->debuginfooffset);
	m_pLXHeader->debuginfolen            = LMMH(m_pLXHeader->debuginfolen);
	m_pLXHeader->numinstancepreload      = LMMH(m_pLXHeader->numinstancepreload);
	m_pLXHeader->numinstancedemand       = LMMH(m_pLXHeader->numinstancedemand);
	m_pLXHeader->heapsize                = LMMH(m_pLXHeader->heapsize);

	if (m_pLXHeader->sigLo != 'L' || (m_pLXHeader->sigHi != 'X' && m_pLXHeader->sigHi != 'E')) {
		fprintf(stderr, "error loading file %s, bad LE/LX magic\n", getFilename());
		return false;
	}

	m_pLXObjects = new LXObject[m_pLXHeader->numobjsinmodule];
	ifs.seekg(lxoff + m_pLXHeader->objtbloffset);
	ifs.read((char *)m_pLXObjects, sizeof *m_pLXObjects * m_pLXHeader->numobjsinmodule);
	for (unsigned n = 0; n < m_pLXHeader->numobjsinmodule; ++n) {
		auto &obj = m_pLXObjects[n];
		obj.VirtualSize       = LMMH(obj.VirtualSize);
		obj.RelocBaseAddr     = LMMH(obj.RelocBaseAddr);
		obj.ObjectFlags       = LMMH(obj.ObjectFlags);
		obj.PageTblIdx        = LMMH(obj.PageTblIdx);
		obj.NumPageTblEntries = LMMH(obj.NumPageTblEntries);
		obj.Reserved1         = LMMH(obj.Reserved1);
	}

	// at this point we're supposed to read in the page table and fuss around with it
	// but I'm just going to assume the file is flat.
#if 0
	unsigned npagetblentries = 0;
	m_cbImage = 0;
	for (unsigned n = 0; n < m_pLXHeader->numobjsinmodule; ++n) {
		auto &obj = m_pLXObjects[n];
		if (npagetblentries < obj.PageTblIdx + obj.NumPageTblEntries - 1)
			npagetblentries = obj.PageTblIdx + obj.NumPageTblEntries - 1;
		if (obj.ObjectFlags & 0x40)
			if (m_cbImage < obj.RelocBaseAddr + obj.VirtualSize)
				m_cbImage = obj.RelocBaseAddr + obj.VirtualSize;
	}
	m_cbImage -= m_pLXObjects[0].RelocBaseAddr;

	m_pLXPages = new LXPage[npagetblentries];
	ifs.seekg(lxoff + m_pLXHeader->objpagetbloffset);
	ifs.read((char *)m_pLXPages, sizeof *m_pLXPages * npagetblentries);
#endif

	unsigned npages = 0;
	m_cbImage = 0;
	for (unsigned n = 0; n < m_pLXHeader->numobjsinmodule; ++n) {
		auto &obj = m_pLXObjects[n];
		if (obj.ObjectFlags & 0x40) {
			if (npages < obj.PageTblIdx + obj.NumPageTblEntries - 1)
				npages = obj.PageTblIdx + obj.NumPageTblEntries - 1;
			m_cbImage = obj.RelocBaseAddr + obj.VirtualSize;
		}
	}

	m_cbImage -= m_pLXObjects[0].RelocBaseAddr;

	base = new unsigned char[m_cbImage];

	m_iNumSections = m_pLXHeader->numobjsinmodule;
	m_pSections = new SectionInfo[m_iNumSections];
	for (unsigned n = 0; n < m_iNumSections; ++n) {
		auto &obj = m_pLXObjects[n];
		auto &sect = m_pSections[n];
		if (obj.ObjectFlags & 0x40) {
			printf("vsize %x reloc %x flags %x page %i npage %i\n",
			       obj.VirtualSize,
			       obj.RelocBaseAddr,
			       obj.ObjectFlags,
			       obj.PageTblIdx,
			       obj.NumPageTblEntries);

			sect.name = "seg" + std::to_string(n);  // no section names in LX
			sect.uNativeAddr = (ADDRESS)obj.RelocBaseAddr;
			sect.uHostAddr = (char *)(obj.RelocBaseAddr - m_pLXObjects[0].RelocBaseAddr + base);
			sect.uSectionSize = obj.VirtualSize;
			DWord Flags = obj.ObjectFlags;
			sect.bBss      = false; // TODO
			sect.bCode     = (Flags & 0x4) != 0;
			sect.bData     = (Flags & 0x4) == 0;
			sect.bReadOnly = (Flags & 0x1) == 0;

			ifs.seekg(m_pLXHeader->datapagesoffset + (obj.PageTblIdx - 1) * m_pLXHeader->pagesize);
			ifs.read(sect.uHostAddr, m_pLXHeader->pagesize * obj.NumPageTblEntries);
		}
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
	auto fixuppagetbl = new unsigned int[npages + 1];
	ifs.seekg(m_pLXHeader->fixuppagetbloffset + lxoff);
	ifs.read((char *)fixuppagetbl, sizeof *fixuppagetbl * (npages + 1));
	for (unsigned n = 0; n < npages + 1; ++n) {
		fixuppagetbl[n] = LMMH(fixuppagetbl[n]);
	}

#if 0
	for (unsigned n = 0; n < npages; ++n)
		printf("offset for page %i: %x\n", n + 1, fixuppagetbl[n]);
	printf("offset to end of fixup rec: %x\n", fixuppagetbl[npages]);
#endif

	ifs.seekg(m_pLXHeader->fixuprecordtbloffset + lxoff);
	unsigned srcpage = 0;
	do {
		LXFixup fixup;
		ifs.read((char *)&fixup, sizeof fixup);
		fixup.srcoff = LMMHw(fixup.srcoff);

		if (fixup.src != 7 || (fixup.flags & ~0x50)) {
			fprintf(stderr, "unknown fixup type %02x %02x\n", fixup.src, fixup.flags);
			return false;
		}
		//printf("srcpage = %i srcoff = %x object = %02x trgoff = %x\n", srcpage + 1, fixup.srcoff, fixup.object, fixup.trgoff);
		unsigned long src = srcpage * m_pLXHeader->pagesize + fixup.srcoff;
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
		unsigned long target = m_pLXObjects[object - 1].RelocBaseAddr + LMMHw(trgoff);
		//printf("relocate dword at %x to point to %x\n", src, target);
		*(unsigned int *)(base + src) = target;

		while ((std::streamsize)ifs.tellg() - (m_pLXHeader->fixuprecordtbloffset + lxoff) >= fixuppagetbl[srcpage + 1])
			++srcpage;
	} while (srcpage < npages);

	return true;
}

#if 0 // Cruft?
bool
DOS4GWBinaryFile::PostLoad(void *handle)
{
	return false;
}
#endif

const char *
DOS4GWBinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	auto it = dlprocptrs.find(dwAddr);
	if (it != dlprocptrs.end())
		return it->second.c_str();
	return nullptr;
}

ADDRESS
DOS4GWBinaryFile::getAddressByName(const char *pName, bool bNoTypeOK /* = false */) const
{
	// This is "looking up the wrong way" and hopefully is uncommon
	// Use linear search
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
DOS4GWBinaryFile::addSymbol(ADDRESS uNative, const char *pName)
{
	dlprocptrs[uNative] = pName;
}

/**
 * \brief Read 2 bytes from native addr.
 */
int
DOS4GWBinaryFile::dos4gwRead2(const short *ps) const
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
DOS4GWBinaryFile::dos4gwRead4(const int *pi) const
{
	const short *p = (const short *)pi;
	int n1 = dos4gwRead2(p);
	int n2 = dos4gwRead2(p + 1);
	int n = (int)(n1 | (n2 << 16));
	return n;
}

int
DOS4GWBinaryFile::readNative1(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) si = getSectionInfo(0);
	return si->uHostAddr[nat - si->uNativeAddr];
}

int
DOS4GWBinaryFile::readNative2(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return 0;
	return dos4gwRead2((const short *)&si->uHostAddr[nat - si->uNativeAddr]);
}

int
DOS4GWBinaryFile::readNative4(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return 0;
	return dos4gwRead4((const int *)&si->uHostAddr[nat - si->uNativeAddr]);
}

QWord
DOS4GWBinaryFile::readNative8(ADDRESS nat) const
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
DOS4GWBinaryFile::readNativeFloat4(ADDRESS nat) const
{
	int raw = readNative4(nat);
	// Ugh! gcc says that reinterpreting from int to float is invalid!!
	//return reinterpret_cast<float>(raw);  // Note: cast, not convert!!
	return *(float *)&raw;  // Note: cast, not convert
}

double
DOS4GWBinaryFile::readNativeFloat8(ADDRESS nat) const
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
DOS4GWBinaryFile::isDynamicLinkedProc(ADDRESS uNative) const
{
	auto it = dlprocptrs.find(uNative);
	return it != dlprocptrs.end()
	    && it->second != "main"
	    && it->second != "_start";
}

bool
DOS4GWBinaryFile::isDynamicLinkedProcPointer(ADDRESS uNative) const
{
	return !!dlprocptrs.count(uNative);
}

const char *
DOS4GWBinaryFile::getDynamicProcName(ADDRESS uNative) const
{
	auto it = dlprocptrs.find(uNative);
	if (it != dlprocptrs.end())
		return it->second.c_str();
	return nullptr;
}

#if 0 // Cruft?
bool
DOS4GWBinaryFile::isLibrary() const
{
	return false; // TODO
}
#endif

ADDRESS
DOS4GWBinaryFile::getImageBase() const
{
	return m_pLXObjects[0].RelocBaseAddr;
}

size_t
DOS4GWBinaryFile::getImageSize() const
{
	return 0; // TODO
}

#if 0 // Cruft?
DWord
DOS4GWBinaryFile::getDelta()
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
extern "C" BinaryFile *
construct()
{
	return new DOS4GWBinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (DOS4GWBinaryFile *)bf;
}
#endif
