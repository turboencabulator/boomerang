/**
 * \file
 * \brief Contains the implementation of the class ExeBinaryFile.
 *
 * This file implements the class ExeBinaryFile, derived from class
 * BinaryFile.  See ExeBinaryFile.h and BinaryFile.h for details.
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

ExeBinaryFile::ExeBinaryFile() :
	m_pHeader(NULL),
	m_pImage(NULL),
	m_pRelocTable(NULL)
{
}

ExeBinaryFile::~ExeBinaryFile()
{
	delete m_pHeader;
	delete [] m_pImage;
	delete [] m_pRelocTable;
}

bool ExeBinaryFile::RealLoad(const char *sName)
{
	FILE *fp;
	int cb;

	m_pFilename = sName;

	// Always just 3 sections
	m_iNumSections = 3;
	m_pSections = new SectionInfo[m_iNumSections];
	if (m_pSections == 0) {
		fprintf(stderr, "Could not allocate section information\n");
		return false;
	}
	m_pHeader = new exeHeader;
	if (m_pHeader == 0) {
		fprintf(stderr, "Could not allocate header memory\n");
		return false;
	}

	/* Open the input file */
	if ((fp = fopen(sName, "rb")) == NULL) {
		fprintf(stderr, "Could not open file %s\n", sName);
		return false;
	}

	/* Read in first 2 bytes to check EXE signature */
	if (fread(m_pHeader, 1, 2, fp) != 2) {
		fprintf(stderr, "Cannot read file %s\n", sName);
		return false;
	}

	// Check for the "MZ" exe header
	if (m_pHeader->sigLo == 'M' && m_pHeader->sigHi == 'Z') {
		/* Read rest of m_pHeader */
		fseek(fp, 0, SEEK_SET);
		if (fread(m_pHeader, sizeof *m_pHeader, 1, fp) != 1) {
			fprintf(stderr, "Cannot read file %s\n", sName);
			return false;
		}

		/* This is a typical DOS kludge! */
		if (LH(&m_pHeader->relocTabOffset) == 0x40) {
			fprintf(stderr, "Error - NE format executable\n");
			return false;
		}

		/* Calculate the load module size.
		 * This is the number of pages in the file
		 * less the length of the m_pHeader and reloc table
		 * less the number of bytes unused on last page
		 */
		cb = (dword)LH(&m_pHeader->numPages) * 512
		   - (dword)LH(&m_pHeader->numParaHeader) * 16;
		if (m_pHeader->lastPageSize) {
			cb -= 512 - LH(&m_pHeader->lastPageSize);
		}

		/* We quietly ignore minAlloc and maxAlloc since for our
		 * purposes it doesn't really matter where in real memory
		 * the m_am would end up.  EXE m_ams can't really rely on
		 * their load location so setting the PSP segment to 0 is fine.
		 * Certainly m_ams that prod around in DOS or BIOS are going
		 * to have to load DS from a constant so it'll be pretty
		 * obvious.
		 */
		m_cReloc = (SWord)LH(&m_pHeader->numReloc);

		/* Allocate the relocation table */
		if (m_cReloc) {
			m_pRelocTable = new dword[m_cReloc];
			if (m_pRelocTable == 0) {
				fprintf(stderr, "Could not allocate relocation table (%d entries)\n", m_cReloc);
				return false;
			}
			fseek(fp, LH(&m_pHeader->relocTabOffset), SEEK_SET);

			/* Read in seg:offset pairs and convert to Image ptrs */
			for (int i = 0; i < m_cReloc; i++) {
				Byte buf[4];
				fread(buf, sizeof *buf, 4, fp);
				m_pRelocTable[i] = LH(buf) + (((int)LH(buf + 2)) << 4);
			}
		}

		/* Seek to start of image */
		fseek(fp, (int)LH(&m_pHeader->numParaHeader) * 16, SEEK_SET);

		// Initial PC and SP. Note that we fake the seg:offset by putting
		// the segment in the top half, and offset in the bottom
		m_uInitPC = ((LH(&m_pHeader->initCS)) << 16) + LH(&m_pHeader->initIP);
		m_uInitSP = ((LH(&m_pHeader->initSS)) << 16) + LH(&m_pHeader->initSP);
	} else {
		/* COM file
		 * In this case the load module size is just the file length
		 */
		fseek(fp, 0, SEEK_END);
		cb = ftell(fp);

		/* COM programs start off with an ORG 100H (to leave room for a PSP)
		 * This is also the implied start address so if we load the image
		 * at offset 100H addresses should all line up properly again.
		 */
		m_uInitPC = 0x100;
		m_uInitSP = 0xFFFE;
		m_cReloc = 0;

		fseek(fp, 0, SEEK_SET);
	}

	/* Allocate a block of memory for the image. */
	m_cbImage = cb;
	m_pImage  = new Byte[cb];

	if (cb != (int)fread(m_pImage, sizeof *m_pImage, cb, fp)) {
		fprintf(stderr, "Cannot read file %s\n", sName);
		return false;
	}

	fclose(fp);

	/* Relocate segment constants */
	for (int i = 0; i < m_cReloc; i++) {
		Byte *p = &m_pImage[m_pRelocTable[i]];
		SWord w = (SWord)LH(p);
		*p++    = (Byte)(w & 0x00FF);
		*p      = (Byte)((w & 0xFF00) >> 8);
	}

	m_pSections[0].pSectionName = "$HEADER";  // Special header section
	//m_pSections[0].fSectionFlags = ST_HEADER;
	m_pSections[0].uNativeAddr = 0;  // Not applicable
	m_pSections[0].uHostAddr = (DWord)m_pHeader;
	m_pSections[0].uSectionSize = sizeof *m_pHeader;
	m_pSections[0].uSectionEntrySize = 1;  // Not applicable

	m_pSections[1].pSectionName = ".text";  // The text and data section
	m_pSections[1].bCode = true;
	m_pSections[1].bData = true;
	m_pSections[1].uNativeAddr = 0;
	m_pSections[1].uHostAddr = (DWord)m_pImage;
	m_pSections[1].uSectionSize = m_cbImage;
	m_pSections[1].uSectionEntrySize = 1;  // Not applicable

	m_pSections[2].pSectionName = "$RELOC";  // Special relocation section
	//m_pSections[2].fSectionFlags = ST_RELOC;  // Give it a special flag
	m_pSections[2].uNativeAddr = 0;  // Not applicable
	m_pSections[2].uHostAddr = (DWord)m_pRelocTable;
	m_pSections[2].uSectionSize =  m_cReloc * sizeof *m_pRelocTable;
	m_pSections[2].uSectionEntrySize = sizeof *m_pRelocTable;

	return true;
}

const char *ExeBinaryFile::SymbolByAddr(ADDRESS dwAddr)
{
	if (dwAddr == getMainEntryPoint())
		return "main";

	// No symbol table handled at present
	return 0;
}

bool ExeBinaryFile::DisplayDetails(const char *fileName, FILE *f /* = stdout */)
{
	return false;
}

bool ExeBinaryFile::PostLoad(void *handle)
{
	// Not needed: for archives only
	return false;
}

bool ExeBinaryFile::isLibrary() const
{
	return false;
}

std::list<const char *> ExeBinaryFile::getDependencyList()
{
	return std::list<const char *>(); /* for now */
}

ADDRESS ExeBinaryFile::getImageBase()
{
	return 0; /* FIXME */
}

size_t ExeBinaryFile::getImageSize()
{
	return 0; /* FIXME */
}

// Should be doing a search for this
ADDRESS ExeBinaryFile::getMainEntryPoint()
{
	return NO_ADDRESS;
}

ADDRESS ExeBinaryFile::getEntryPoint()
{
	// Check this...
	return (ADDRESS)((LH(&m_pHeader->initCS) << 4) + LH(&m_pHeader->initIP));
}

// This is provided for completeness only...
std::list<SectionInfo *> &ExeBinaryFile::getEntryPoints(const char *pEntry /* = "main"*/)
{
	std::list<SectionInfo *> *ret = new std::list<SectionInfo *>;
#if 0  // Copied from PalmBinaryFile.cc
	SectionInfo *pSect = getSectionInfoByName("code1");
	if (pSect == 0)
		return *ret;  // Failed
	ret->push_back(pSect);
#endif
	return *ret;
}

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new BinaryFile
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" BinaryFile *construct()
{
	return new ExeBinaryFile();
}
extern "C" void destruct(BinaryFile *bf)
{
	delete bf;
}
#endif
