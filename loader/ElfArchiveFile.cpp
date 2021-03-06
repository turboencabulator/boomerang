/**
 * \file
 * \brief Contains the implementation of the class ElfArchiveFile.
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

#include <iostream>

ElfArchiveFile::ElfArchiveFile()
{
}

ElfArchiveFile::~ElfArchiveFile()
{
	for (unsigned u = 0; u < m_Members.size(); ++u)
		delete m_Members[u];
}

bool
ElfArchiveFile::Load(const char *pName)
{
	// Load the elf file
	m_filedes = open(pName, O_RDONLY);
	if (m_filedes == -1) {
		std::cout << "Could not open " << pName << "\n";
		return false;
	}

	elf_version(EV_CURRENT);
	m_arf = elf_begin(m_filedes, ELF_C_READ, (Elf *)0);
	if (elf_kind(m_arf) != ELF_K_AR) {
		std::cout << "Error - " << pName << " is not an archive (.a) file\n";
		return false;
	}

	// Load the symbol table. We assume that each member has at
	// least one symbol.
	// We want a map from symbol to index; to do this, we need to know
	// the current index and last offset seen
	int iLastOffset = 0;
	int iOffset = 0;
	unsigned int uNumSyms;
	int iIndex = -1;  // 0,1,2... for 1st,2nd,3rd... member

	Elf_Arsym *asym = elf_getarsym(m_arf, &uNumSyms);
	--uNumSyms;
	if (!asym) {
		std::cout << "Get archive symbol table failed\n";
		return false;
	}

	for (unsigned u = 0; u < uNumSyms; ++u) {
		iOffset = asym[u].as_off;
		// Last entry is null, but should never see it
		if (iOffset == 0) break;
		if (iOffset != iLastOffset) {
			// This is a new member. Use a new index
			++iIndex;
			iLastOffset = iOffset;

			// Seek to that member
			if (elf_rand(m_arf, iOffset) == 0) {
				std::cout << "Could not seek to offset " << iOffset << "\n";
				return false;
			}
			Elf *elf = elf_begin(m_filedes, ELF_C_READ, m_arf);
			if (!elf) {
				std::cout << "Could not begin member at offset " << iOffset << "\n";
				return false;
			}
			Elf_Arhdr *ahdr = elf_getarhdr(elf);
			if (!ahdr) {
				std::cout << "Could not get header information for member at offset " << iOffset << "\n";
				return false;
			}
			// Add the name to the map
			m_FileMap[ahdr->ar_name] = iIndex;
			// And to the vector of pointers to file names
			m_FileNames.push_back(ahdr->ar_name);
			// Also add the offset. These are supposed to be relatively
			// implementation independant
			m_Offsets.push_back(iOffset);
		}
		// Add an entry to the symbol->offset map
		m_SymMap[asym[u].as_name] = iIndex;
	}

	// Now we know the correct size for the vector of members.
	// Ugh - can't call constructor any more
	//m_Members.vector(getNumMembers(), (BinaryFile *)0);
	m_Members.reserve(getNumMembers());

	return true;
}

BinaryFile *
ElfArchiveFile::getMember(int i)
{
	// Sanity checks on the index
	if (i < 0) return nullptr;
	if (i >= m_FileMap.size()) return nullptr;

	// Lazy creation. Check to see if already created
	if (i >= m_Members.size() || (m_Members[i] == 0)) {
		// Now we have to create one. We set the constructor argument
		// bArchive to true, so it knows it's an archive member
		BinaryFile *pBF = new ElfBinaryFile(true);
		// Load the file for the user. First find the offset
		int iOffset = m_Offsets[i];
		if (iOffset == 0) return nullptr;
		if (elf_rand(m_arf, iOffset) != iOffset) return nullptr;
		Elf *elf = elf_begin(m_filedes, ELF_C_READ, m_arf);  // Elf handle for the new member
		if (!elf) return nullptr;
		// We have to get our father to load the file, since he is a
		// friend of class BinaryFile, but we aren't
		if (!PostLoadMember(pBF, elf)) return nullptr;
		m_Members[i] = pBF;
		return pBF;
	}
	// Else already seen
	return m_Members[i];
}
