/**
 * \file
 * \brief Contains the implementation of the class ComBinaryFile.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ComBinaryFile.h"

#include <cstdio>

ComBinaryFile::ComBinaryFile()
{
}

ComBinaryFile::~ComBinaryFile()
{
	delete [] m_pImage;
}

bool
ComBinaryFile::load(std::istream &ifs)
{
	/* The load module size is just the file length. */
	ifs.seekg(0, ifs.end);
	std::streamsize cb = ifs.tellg();
	m_pImage = new uint8_t[cb];

	ifs.seekg(0, ifs.beg);
	ifs.read((char *)m_pImage, cb);
	if (!ifs.good()) {
		fprintf(stderr, "Cannot read file %s\n", getFilename());
		return false;
	}

	/* COM programs start off with an ORG 100H (to leave room for a PSP)
	 * This is also the implied start address so if we load the image
	 * at offset 100H addresses should all line up properly again. */
	//m_uInitPC = 0x100;
	//m_uInitSP = 0xFFFE;

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

ADDRESS
ComBinaryFile::getMainEntryPoint()
{
	return NO_ADDRESS;
}

ADDRESS
ComBinaryFile::getEntryPoint() const
{
	return NO_ADDRESS;  // TODO
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
	return new ComBinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (ComBinaryFile *)bf;
}
#endif
