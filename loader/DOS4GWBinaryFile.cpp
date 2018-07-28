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
	bigendian = false;
}

DOS4GWBinaryFile::~DOS4GWBinaryFile()
{
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
	const SectionInfo *si = getSectionInfoByName("seg0");     // Assume the first section is text
	if (!si) si = getSectionInfoByName(".text");
	if (!si) si = getSectionInfoByName("CODE");
	assert(si);
	ADDRESS nativeOrigin = si->uNativeAddr;
	unsigned textSize = si->uSectionSize;

	unsigned p = m_pLXHeader->eip;
	unsigned lim = p + 0x300;
	if (textSize < 0x300)
		lim = p + textSize;

	bool gotSubEbp = false;         // True if see sub ebp, ebp
	bool lastWasCall = false;       // True if the last instruction was a call
	while (p < lim) {
		unsigned char op1 = *(p + base);
		unsigned char op2 = *(p + base + 1);
		//std::cerr << std::hex << "At " << p << ", ops " << (unsigned)op1 << ", " << (unsigned)op2 << std::dec << "\n";
		switch (op1) {
		case 0xE8:  // An ordinary call
			if (gotSubEbp) {
				// This is the call we want. Get the offset from the call instruction
				unsigned addr = nativeOrigin + p + 5 + LH32(p + base + 1);
				// std::cerr << "__CMain at " << std::hex << addr << "\n";
				return addr;
			}
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
	uint32_t lxoff;
	ifs.seekg(0x3c);
	ifs.read((char *)&lxoff, sizeof lxoff);
	lxoff = LH32(&lxoff);

	m_pLXHeader = new LXHeader;
	ifs.seekg(lxoff);
	ifs.read((char *)m_pLXHeader, sizeof *m_pLXHeader);
	m_pLXHeader->formatlvl               = LH32(&m_pLXHeader->formatlvl);
	m_pLXHeader->cputype                 = LH16(&m_pLXHeader->cputype);
	m_pLXHeader->ostype                  = LH16(&m_pLXHeader->ostype);
	m_pLXHeader->modulever               = LH32(&m_pLXHeader->modulever);
	m_pLXHeader->moduleflags             = LH32(&m_pLXHeader->moduleflags);
	m_pLXHeader->modulenumpages          = LH32(&m_pLXHeader->modulenumpages);
	m_pLXHeader->eipobjectnum            = LH32(&m_pLXHeader->eipobjectnum);
	m_pLXHeader->eip                     = LH32(&m_pLXHeader->eip);
	m_pLXHeader->espobjectnum            = LH32(&m_pLXHeader->espobjectnum);
	m_pLXHeader->esp                     = LH32(&m_pLXHeader->esp);
	m_pLXHeader->pagesize                = LH32(&m_pLXHeader->pagesize);
	m_pLXHeader->pageoffsetshift         = LH32(&m_pLXHeader->pageoffsetshift);
	m_pLXHeader->fixupsectionsize        = LH32(&m_pLXHeader->fixupsectionsize);
	m_pLXHeader->fixupsectionchksum      = LH32(&m_pLXHeader->fixupsectionchksum);
	m_pLXHeader->loadersectionsize       = LH32(&m_pLXHeader->loadersectionsize);
	m_pLXHeader->loadersectionchksum     = LH32(&m_pLXHeader->loadersectionchksum);
	m_pLXHeader->objtbloffset            = LH32(&m_pLXHeader->objtbloffset);
	m_pLXHeader->numobjsinmodule         = LH32(&m_pLXHeader->numobjsinmodule);
	m_pLXHeader->objpagetbloffset        = LH32(&m_pLXHeader->objpagetbloffset);
	m_pLXHeader->objiterpagesoffset      = LH32(&m_pLXHeader->objiterpagesoffset);
	m_pLXHeader->resourcetbloffset       = LH32(&m_pLXHeader->resourcetbloffset);
	m_pLXHeader->numresourcetblentries   = LH32(&m_pLXHeader->numresourcetblentries);
	m_pLXHeader->residentnametbloffset   = LH32(&m_pLXHeader->residentnametbloffset);
	m_pLXHeader->entrytbloffset          = LH32(&m_pLXHeader->entrytbloffset);
	m_pLXHeader->moduledirectivesoffset  = LH32(&m_pLXHeader->moduledirectivesoffset);
	m_pLXHeader->nummoduledirectives     = LH32(&m_pLXHeader->nummoduledirectives);
	m_pLXHeader->fixuppagetbloffset      = LH32(&m_pLXHeader->fixuppagetbloffset);
	m_pLXHeader->fixuprecordtbloffset    = LH32(&m_pLXHeader->fixuprecordtbloffset);
	m_pLXHeader->importtbloffset         = LH32(&m_pLXHeader->importtbloffset);
	m_pLXHeader->numimportmoduleentries  = LH32(&m_pLXHeader->numimportmoduleentries);
	m_pLXHeader->importproctbloffset     = LH32(&m_pLXHeader->importproctbloffset);
	m_pLXHeader->perpagechksumoffset     = LH32(&m_pLXHeader->perpagechksumoffset);
	m_pLXHeader->datapagesoffset         = LH32(&m_pLXHeader->datapagesoffset);
	m_pLXHeader->numpreloadpages         = LH32(&m_pLXHeader->numpreloadpages);
	m_pLXHeader->nonresnametbloffset     = LH32(&m_pLXHeader->nonresnametbloffset);
	m_pLXHeader->nonresnametbllen        = LH32(&m_pLXHeader->nonresnametbllen);
	m_pLXHeader->nonresnametblchksum     = LH32(&m_pLXHeader->nonresnametblchksum);
	m_pLXHeader->autodsobjectnum         = LH32(&m_pLXHeader->autodsobjectnum);
	m_pLXHeader->debuginfooffset         = LH32(&m_pLXHeader->debuginfooffset);
	m_pLXHeader->debuginfolen            = LH32(&m_pLXHeader->debuginfolen);
	m_pLXHeader->numinstancepreload      = LH32(&m_pLXHeader->numinstancepreload);
	m_pLXHeader->numinstancedemand       = LH32(&m_pLXHeader->numinstancedemand);
	m_pLXHeader->heapsize                = LH32(&m_pLXHeader->heapsize);

	if (m_pLXHeader->sigLo != 'L' || (m_pLXHeader->sigHi != 'X' && m_pLXHeader->sigHi != 'E')) {
		fprintf(stderr, "error loading file %s, bad LE/LX magic\n", getFilename());
		return false;
	}

	m_pLXObjects = new LXObject[m_pLXHeader->numobjsinmodule];
	ifs.seekg(lxoff + m_pLXHeader->objtbloffset);
	ifs.read((char *)m_pLXObjects, sizeof *m_pLXObjects * m_pLXHeader->numobjsinmodule);
	for (unsigned n = 0; n < m_pLXHeader->numobjsinmodule; ++n) {
		auto &obj = m_pLXObjects[n];
		obj.VirtualSize       = LH32(&obj.VirtualSize);
		obj.RelocBaseAddr     = LH32(&obj.RelocBaseAddr);
		obj.ObjectFlags       = LH32(&obj.ObjectFlags);
		obj.PageTblIdx        = LH32(&obj.PageTblIdx);
		obj.NumPageTblEntries = LH32(&obj.NumPageTblEntries);
		obj.Reserved1         = LH32(&obj.Reserved1);
	}

	// at this point we're supposed to read in the page table and fuss around with it
	// but I'm just going to assume the file is flat.
#if 0
	unsigned npagetblentries = 0;
	unsigned imagesize = 0;
	for (unsigned n = 0; n < m_pLXHeader->numobjsinmodule; ++n) {
		const auto &obj = m_pLXObjects[n];
		if (npagetblentries < obj.PageTblIdx + obj.NumPageTblEntries - 1)
			npagetblentries = obj.PageTblIdx + obj.NumPageTblEntries - 1;
		if (obj.ObjectFlags & 0x40)
			if (imagesize < obj.RelocBaseAddr + obj.VirtualSize)
				imagesize = obj.RelocBaseAddr + obj.VirtualSize;
	}
	imagesize -= m_pLXObjects[0].RelocBaseAddr;

	m_pLXPages = new LXPage[npagetblentries];
	ifs.seekg(lxoff + m_pLXHeader->objpagetbloffset);
	ifs.read((char *)m_pLXPages, sizeof *m_pLXPages * npagetblentries);
#endif

	unsigned npages = 0;
	unsigned imagesize = 0;
	for (unsigned n = 0; n < m_pLXHeader->numobjsinmodule; ++n) {
		const auto &obj = m_pLXObjects[n];
		if (obj.ObjectFlags & 0x40) {
			if (npages < obj.PageTblIdx + obj.NumPageTblEntries - 1)
				npages = obj.PageTblIdx + obj.NumPageTblEntries - 1;
			imagesize = obj.RelocBaseAddr + obj.VirtualSize;
		}
	}

	imagesize -= m_pLXObjects[0].RelocBaseAddr;

	base = new unsigned char[imagesize];

	sections.reserve(m_pLXHeader->numobjsinmodule);
	for (unsigned n = 0; n < m_pLXHeader->numobjsinmodule; ++n) {
		const auto &obj = m_pLXObjects[n];
		if (obj.ObjectFlags & 0x40) {
			printf("vsize %x reloc %x flags %x page %i npage %i\n",
			       obj.VirtualSize,
			       obj.RelocBaseAddr,
			       obj.ObjectFlags,
			       obj.PageTblIdx,
			       obj.NumPageTblEntries);

			auto sect = SectionInfo();
			sect.name = "seg" + std::to_string(n);  // no section names in LX
			sect.uNativeAddr = (ADDRESS)obj.RelocBaseAddr;
			sect.uHostAddr = (char *)(obj.RelocBaseAddr - m_pLXObjects[0].RelocBaseAddr + base);
			sect.uSectionSize = obj.VirtualSize;
			auto Flags = obj.ObjectFlags;
			sect.bBss      = false; // TODO
			sect.bCode     = (Flags & 0x4) != 0;
			sect.bData     = (Flags & 0x4) == 0;
			sect.bReadOnly = (Flags & 0x1) == 0;
			sections.push_back(sect);

			ifs.seekg(m_pLXHeader->datapagesoffset + (obj.PageTblIdx - 1) * m_pLXHeader->pagesize);
			ifs.read(sect.uHostAddr, m_pLXHeader->pagesize * obj.NumPageTblEntries);
		}
	}

	// TODO: decode entry tables

	// fixups
	auto fixuppagetbl = new uint32_t[npages + 1];
	ifs.seekg(m_pLXHeader->fixuppagetbloffset + lxoff);
	ifs.read((char *)fixuppagetbl, sizeof *fixuppagetbl * (npages + 1));
	for (unsigned n = 0; n < npages + 1; ++n) {
		fixuppagetbl[n] = LH32(&fixuppagetbl[n]);
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
		fixup.srcoff = LH16(&fixup.srcoff);

		if (fixup.src != 7 || (fixup.flags & ~0x50)) {
			fprintf(stderr, "unknown fixup type %02x %02x\n", fixup.src, fixup.flags);
			return false;
		}
		//printf("srcpage = %i srcoff = %x object = %02x trgoff = %x\n", srcpage + 1, fixup.srcoff, fixup.object, fixup.trgoff);

		uint16_t object = 0;
		ifs.read((char *)&object, fixup.flags & 0x40 ? 2 : 1);
		object = LH16(&object);
		uint32_t trgoff = 0;
		ifs.read((char *)&trgoff, fixup.flags & 0x10 ? 4 : 2);
		trgoff = LH32(&trgoff);

		unsigned long src = srcpage * m_pLXHeader->pagesize + fixup.srcoff;
		unsigned long target = m_pLXObjects[object - 1].RelocBaseAddr + trgoff;
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
	// This is "looking up the wrong way" and hopefully is uncommon.  Use linear search
	for (const auto &sym : dlprocptrs) {
		// std::cerr << "Symbol: " << sym.second << " at 0x" << std::hex << sym.first << "\n";
		if (sym.second == pName)
			return sym.first;
	}
	return NO_ADDRESS;
}

void
DOS4GWBinaryFile::addSymbol(ADDRESS uNative, const char *pName)
{
	dlprocptrs[uNative] = pName;
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
