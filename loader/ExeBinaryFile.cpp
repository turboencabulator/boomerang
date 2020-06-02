/**
 * \file
 * \brief Contains the implementation of the class ExeBinaryFile.
 *
 * \authors
 * Copyright (C) 1997,2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ExeBinaryFile.h"

#include <cassert>
#include <cstdio>

ExeBinaryFile::ExeBinaryFile()
{
}

ExeBinaryFile::~ExeBinaryFile()
{
	delete    m_pHeader;
	delete [] m_pImage;
}

bool
ExeBinaryFile::load(std::istream &ifs)
{
	m_pHeader = new exeHeader;
	ifs.read((char *)m_pHeader, sizeof *m_pHeader);
	if (!ifs.good()) {
		fprintf(stderr, "Cannot read file %s\n", getFilename());
		return false;
	}

	/* Check for the "MZ" exe header */
	assert(m_pHeader->sigLo == 'M' && m_pHeader->sigHi == 'Z');

	m_pHeader->lastPageSize   = LH16(&m_pHeader->lastPageSize);
	m_pHeader->numPages       = LH16(&m_pHeader->numPages);
	m_pHeader->numReloc       = LH16(&m_pHeader->numReloc);
	m_pHeader->numParaHeader  = LH16(&m_pHeader->numParaHeader);
	m_pHeader->minAlloc       = LH16(&m_pHeader->minAlloc);
	m_pHeader->maxAlloc       = LH16(&m_pHeader->maxAlloc);
	m_pHeader->initSS         = LH16(&m_pHeader->initSS);
	m_pHeader->initSP         = LH16(&m_pHeader->initSP);
	m_pHeader->checkSum       = LH16(&m_pHeader->checkSum);
	m_pHeader->initIP         = LH16(&m_pHeader->initIP);
	m_pHeader->initCS         = LH16(&m_pHeader->initCS);
	m_pHeader->relocTabOffset = LH16(&m_pHeader->relocTabOffset);
	m_pHeader->overlayNum     = LH16(&m_pHeader->overlayNum);

	/* Calculate the load module size.
	 * This is the number of pages in the file
	 * less the length of the m_pHeader and reloc table
	 * less the number of bytes unused on last page. */
	std::streamsize cb = m_pHeader->numPages * 512
	                   - m_pHeader->numParaHeader * 16;
	if (m_pHeader->lastPageSize) {
		cb -= 512 - m_pHeader->lastPageSize;
	}
	m_pImage = new uint8_t[cb];

	ifs.seekg(m_pHeader->numParaHeader * 16);
	ifs.read((char *)m_pImage, cb);
	if (!ifs.good()) {
		fprintf(stderr, "Cannot read file %s\n", getFilename());
		return false;
	}

	/* We quietly ignore minAlloc and maxAlloc since for our
	 * purposes it doesn't really matter where in real memory
	 * the m_am would end up.  EXE m_ams can't really rely on
	 * their load location so setting the PSP segment to 0 is fine.
	 * Certainly m_ams that prod around in DOS or BIOS are going
	 * to have to load DS from a constant so it'll be pretty
	 * obvious. */

	/* Relocate segment constants */
#if 0  // This code is a no-op
	ifs.seekg(m_pHeader->relocTabOffset);
	for (size_t i = 0; i < m_pHeader->numReloc; ++i) {
		/* Read in seg:offset pairs and convert to Image ptrs */
		uint8_t buf[4];
		ifs.read((char *)buf, 4);
		uint32_t addr = LH16(buf) + ((int)LH16(buf + 2) << 4);

		uint8_t *p = &m_pImage[addr];
		uint16_t w = (uint16_t)LH16(p);
		*p++       = (uint8_t)(w & 0x00FF);
		*p         = (uint8_t)((w & 0xFF00) >> 8);
	}
#endif

	// Initial PC and SP. Note that we fake the seg:offset by putting
	// the segment in the top half, and offset in the bottom
	//m_uInitPC = (m_pHeader->initCS << 16) + m_pHeader->initIP;
	//m_uInitSP = (m_pHeader->initSS << 16) + m_pHeader->initSP;

	sections.reserve(1);

	auto sect = SectionInfo();
	sect.name = ".text";  // The text and data section
	sect.bCode = true;
	sect.bData = true;
	sect.uNativeAddr = 0;
	sect.uHostAddr = (char *)m_pImage;
	sect.uSectionSize = cb;
	sect.uSectionEntrySize = 1;  // Not applicable
	sections.push_back(sect);

	return true;
}

#if 0 // Cruft?
bool
ExeBinaryFile::PostLoad(void *handle)
{
	// Not needed: for archives only
	return false;
}
#endif

#if 0 // Cruft?
bool
ExeBinaryFile::isLibrary() const
{
	return false;
}

ADDRESS
ExeBinaryFile::getImageBase() const
{
	return 0; /* FIXME */
}

size_t
ExeBinaryFile::getImageSize() const
{
	return 0; /* FIXME */
}
#endif

/**
 * Should be doing a search for this.
 */
ADDRESS
ExeBinaryFile::getMainEntryPoint()
{
	return NO_ADDRESS;
}

ADDRESS
ExeBinaryFile::getEntryPoint() const
{
	// Check this...
	return (ADDRESS)((m_pHeader->initCS << 4) + m_pHeader->initIP);
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
	return new ExeBinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (ExeBinaryFile *)bf;
}
#endif
