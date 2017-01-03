/**
 * \file
 * \brief Contains the implementation of the class ArchiveFile.
 *
 * \authors
 * Copyright (C) 1998, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

ArchiveFile::ArchiveFile()
{
}

ArchiveFile::~ArchiveFile()
{
}

int ArchiveFile::getNumMembers() const
{
	return m_FileMap.size();
}

const char *ArchiveFile::getMemberFileName(int i) const
{
	return m_FileNames[i];
}

BinaryFile *ArchiveFile::getMemberByProcName(const string &sSym)
{
	// Get the index
	int idx = m_SymMap[sSym];
	// Look it up
	return getMember(idx);
}

BinaryFile *ArchiveFile::getMemberByFileName(const string &sFile)
{
	// Get the index
	int idx = m_FileMap[sFile];
	// Look it up
	return getMember(idx);
}

bool ArchiveFile::PostLoadMember(BinaryFile *pBF, void *handle)
{
	return pBF->PostLoad(handle);
}
