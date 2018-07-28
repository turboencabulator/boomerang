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

#include <cstdio>

ExeBinaryFile::ExeBinaryFile()
{
}

ExeBinaryFile::~ExeBinaryFile()
{
	delete    m_pHeader;
	delete [] m_pImage;
	delete [] m_pRelocTable;
}

bool
ExeBinaryFile::load(std::istream &ifs)
{
	m_pHeader = new exeHeader;

	/* Read in first 2 bytes to check EXE signature */
	ifs.read((char *)m_pHeader, 2);
	if (!ifs.good()) {
		fprintf(stderr, "Cannot read file %s\n", getFilename());
		return false;
	}

	std::streamsize cb;
	unsigned numreloc;

	// Check for the "MZ" exe header
	if (m_pHeader->sigLo == 'M' && m_pHeader->sigHi == 'Z') {
		/* Read rest of m_pHeader */
		ifs.seekg(0);
		ifs.read((char *)m_pHeader, sizeof *m_pHeader);
		if (!ifs.good()) {
			fprintf(stderr, "Cannot read file %s\n", getFilename());
			return false;
		}
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

		/* This is a typical DOS kludge! */
		if (m_pHeader->relocTabOffset == 0x40) {
			fprintf(stderr, "Error - NE format executable\n");
			return false;
		}

		/* Calculate the load module size.
		 * This is the number of pages in the file
		 * less the length of the m_pHeader and reloc table
		 * less the number of bytes unused on last page
		 */
		cb = m_pHeader->numPages * 512
		   - m_pHeader->numParaHeader * 16;
		if (m_pHeader->lastPageSize) {
			cb -= 512 - m_pHeader->lastPageSize;
		}

		/* We quietly ignore minAlloc and maxAlloc since for our
		 * purposes it doesn't really matter where in real memory
		 * the m_am would end up.  EXE m_ams can't really rely on
		 * their load location so setting the PSP segment to 0 is fine.
		 * Certainly m_ams that prod around in DOS or BIOS are going
		 * to have to load DS from a constant so it'll be pretty
		 * obvious.
		 */
		numreloc = m_pHeader->numReloc;

		/* Allocate the relocation table */
		if (numreloc) {
			m_pRelocTable = new uint32_t[numreloc];
			ifs.seekg(m_pHeader->relocTabOffset);

			/* Read in seg:offset pairs and convert to Image ptrs */
			for (int i = 0; i < numreloc; ++i) {
				uint8_t buf[4];
				ifs.read((char *)buf, 4);
				m_pRelocTable[i] = LH16(buf) + ((int)LH16(buf + 2) << 4);
			}
		}

		/* Seek to start of image */
		ifs.seekg(m_pHeader->numParaHeader * 16);

		// Initial PC and SP. Note that we fake the seg:offset by putting
		// the segment in the top half, and offset in the bottom
		//m_uInitPC = (m_pHeader->initCS << 16) + m_pHeader->initIP;
		//m_uInitSP = (m_pHeader->initSS << 16) + m_pHeader->initSP;
	} else {
		/* COM file
		 * In this case the load module size is just the file length
		 */
		ifs.seekg(0, ifs.end);
		cb = ifs.tellg();

		/* COM programs start off with an ORG 100H (to leave room for a PSP)
		 * This is also the implied start address so if we load the image
		 * at offset 100H addresses should all line up properly again.
		 */
		//m_uInitPC = 0x100;
		//m_uInitSP = 0xFFFE;
		numreloc = 0;

		ifs.seekg(0, ifs.beg);
	}

	/* Allocate a block of memory for the image. */
	m_pImage = new uint8_t[cb];

	ifs.read((char *)m_pImage, cb);
	if (!ifs.good()) {
		fprintf(stderr, "Cannot read file %s\n", getFilename());
		return false;
	}

	/* Relocate segment constants */
	for (int i = 0; i < numreloc; ++i) {
		uint8_t *p = &m_pImage[m_pRelocTable[i]];
		uint16_t w = (uint16_t)LH16(p);
		*p++       = (uint8_t)(w & 0x00FF);
		*p         = (uint8_t)((w & 0xFF00) >> 8);
	}

	// Always just 3 sections
	// FIXME:  Should $HEADER and $RELOC be sections?
	//         We've converted them to host endianness.
	sections.reserve(3);

	auto sect0 = SectionInfo();
	sect0.name = "$HEADER";  // Special header section
	//sect0.fSectionFlags = ST_HEADER;
	sect0.uNativeAddr = 0;  // Not applicable
	sect0.uHostAddr = (char *)m_pHeader;
	sect0.uSectionSize = sizeof *m_pHeader;
	sect0.uSectionEntrySize = 1;  // Not applicable
	sections.push_back(sect0);

	auto sect1 = SectionInfo();
	sect1.name = ".text";  // The text and data section
	sect1.bCode = true;
	sect1.bData = true;
	sect1.uNativeAddr = 0;
	sect1.uHostAddr = (char *)m_pImage;
	sect1.uSectionSize = cb;
	sect1.uSectionEntrySize = 1;  // Not applicable
	sections.push_back(sect1);

	auto sect2 = SectionInfo();
	sect2.name = "$RELOC";  // Special relocation section
	//sect2.fSectionFlags = ST_RELOC;  // Give it a special flag
	sect2.uNativeAddr = 0;  // Not applicable
	sect2.uHostAddr = (char *)m_pRelocTable;
	sect2.uSectionSize =  numreloc * sizeof *m_pRelocTable;
	sect2.uSectionEntrySize = sizeof *m_pRelocTable;
	sections.push_back(sect2);

	return true;
}

const char *
ExeBinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	if (dwAddr == getMainEntryPoint())
		return "main";

	// No symbol table handled at present
	return nullptr;
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
