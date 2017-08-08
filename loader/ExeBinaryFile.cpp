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
	delete [] m_pSections;
	delete    m_pHeader;
	delete [] m_pImage;
	delete [] m_pRelocTable;
}

bool
ExeBinaryFile::load(std::istream &ifs)
{
	std::streamsize cb;

	// Always just 3 sections
	// FIXME:  Should $HEADER and $RELOC be sections?
	//         We've converted them to host endianness.
	m_iNumSections = 3;
	m_pSections = new SectionInfo[m_iNumSections];
	m_pHeader = new exeHeader;

	/* Read in first 2 bytes to check EXE signature */
	ifs.read((char *)m_pHeader, 2);
	if (!ifs.good()) {
		fprintf(stderr, "Cannot read file %s\n", getFilename());
		return false;
	}

	// Check for the "MZ" exe header
	if (m_pHeader->sigLo == 'M' && m_pHeader->sigHi == 'Z') {
		/* Read rest of m_pHeader */
		ifs.seekg(0);
		ifs.read((char *)m_pHeader, sizeof *m_pHeader);
		if (!ifs.good()) {
			fprintf(stderr, "Cannot read file %s\n", getFilename());
			return false;
		}
		m_pHeader->lastPageSize   = LH(&m_pHeader->lastPageSize);
		m_pHeader->numPages       = LH(&m_pHeader->numPages);
		m_pHeader->numReloc       = LH(&m_pHeader->numReloc);
		m_pHeader->numParaHeader  = LH(&m_pHeader->numParaHeader);
		m_pHeader->minAlloc       = LH(&m_pHeader->minAlloc);
		m_pHeader->maxAlloc       = LH(&m_pHeader->maxAlloc);
		m_pHeader->initSS         = LH(&m_pHeader->initSS);
		m_pHeader->initSP         = LH(&m_pHeader->initSP);
		m_pHeader->checkSum       = LH(&m_pHeader->checkSum);
		m_pHeader->initIP         = LH(&m_pHeader->initIP);
		m_pHeader->initCS         = LH(&m_pHeader->initCS);
		m_pHeader->relocTabOffset = LH(&m_pHeader->relocTabOffset);
		m_pHeader->overlayNum     = LH(&m_pHeader->overlayNum);

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
		m_cReloc = m_pHeader->numReloc;

		/* Allocate the relocation table */
		if (m_cReloc) {
			m_pRelocTable = new dword[m_cReloc];
			ifs.seekg(m_pHeader->relocTabOffset);

			/* Read in seg:offset pairs and convert to Image ptrs */
			for (int i = 0; i < m_cReloc; ++i) {
				Byte buf[4];
				ifs.read((char *)buf, 4);
				m_pRelocTable[i] = LH(buf) + ((int)LH(buf + 2) << 4);
			}
		}

		/* Seek to start of image */
		ifs.seekg(m_pHeader->numParaHeader * 16);

		// Initial PC and SP. Note that we fake the seg:offset by putting
		// the segment in the top half, and offset in the bottom
		m_uInitPC = (m_pHeader->initCS << 16) + m_pHeader->initIP;
		m_uInitSP = (m_pHeader->initSS << 16) + m_pHeader->initSP;
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
		m_uInitPC = 0x100;
		m_uInitSP = 0xFFFE;
		m_cReloc = 0;

		ifs.seekg(0, ifs.beg);
	}

	/* Allocate a block of memory for the image. */
	m_pImage  = new Byte[cb];

	ifs.read((char *)m_pImage, cb);
	if (!ifs.good()) {
		fprintf(stderr, "Cannot read file %s\n", getFilename());
		return false;
	}

	/* Relocate segment constants */
	for (int i = 0; i < m_cReloc; ++i) {
		Byte *p = &m_pImage[m_pRelocTable[i]];
		SWord w = (SWord)LH(p);
		*p++    = (Byte)(w & 0x00FF);
		*p      = (Byte)((w & 0xFF00) >> 8);
	}

	m_pSections[0].pSectionName = "$HEADER";  // Special header section
	//m_pSections[0].fSectionFlags = ST_HEADER;
	m_pSections[0].uNativeAddr = 0;  // Not applicable
	m_pSections[0].uHostAddr = (char *)m_pHeader;
	m_pSections[0].uSectionSize = sizeof *m_pHeader;
	m_pSections[0].uSectionEntrySize = 1;  // Not applicable

	m_pSections[1].pSectionName = ".text";  // The text and data section
	m_pSections[1].bCode = true;
	m_pSections[1].bData = true;
	m_pSections[1].uNativeAddr = 0;
	m_pSections[1].uHostAddr = (char *)m_pImage;
	m_pSections[1].uSectionSize = cb;
	m_pSections[1].uSectionEntrySize = 1;  // Not applicable

	m_pSections[2].pSectionName = "$RELOC";  // Special relocation section
	//m_pSections[2].fSectionFlags = ST_RELOC;  // Give it a special flag
	m_pSections[2].uNativeAddr = 0;  // Not applicable
	m_pSections[2].uHostAddr = (char *)m_pRelocTable;
	m_pSections[2].uSectionSize =  m_cReloc * sizeof *m_pRelocTable;
	m_pSections[2].uSectionEntrySize = sizeof *m_pRelocTable;

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

bool
ExeBinaryFile::isLibrary() const
{
	return false;
}

std::list<const char *>
ExeBinaryFile::getDependencyList()
{
	return std::list<const char *>(); /* for now */
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
