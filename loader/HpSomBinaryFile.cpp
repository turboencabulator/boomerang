/**
 * \file
 * \brief Contains the implementation of the class HpSomBinaryFile,
 *
 * \authors
 * Copyright (C) 2000-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "HpSomBinaryFile.h"

#include <cassert>
#include <cstdio>
#include <cstring>

HpSomBinaryFile::HpSomBinaryFile()
{
}

HpSomBinaryFile::~HpSomBinaryFile()
{
	delete [] m_pImage;
}

// Functions to recognise various instruction patterns
// Note: these are not presently used. May be needed again if it turns out
// that addresses in the PLT do not always point to the BOR (Bind On Reference,
// a kind of stub)
#if 0
bool
isLDW(unsigned instr, int &offset, unsigned dest)
{
	if (((instr >> 26) == 0x12)                 // Opcode
	 && (instr & 1)                             // Offset is neg
	 && (((instr >> 21) & 0x1f) == 27)          // register b
	 && (((instr >> 16) & 0x1f) == dest)) {     // register t
		offset = (((int)instr << 31) >> 18)
		       | ((instr & 0x3ffe) >> 1);
		return true;
	}
	return false;
}

bool
isLDSID(unsigned instr)
{
	// Looking for LDSID       (%s0,%r21),%r1
	return (instr == 0x02a010a1);
}

bool
isMSTP(unsigned instr)
{
	// Looking for MTSP        %r1,%s0
	return (instr == 0x00011820);
}

bool
isBE(unsigned instr)
{
	// Looking for BE          0(%s0,%r21)
	return (instr == 0xe2a00000);
}

bool
isSTW(unsigned instr)
{
	// Looking for STW         %r2,-24(%s0,%r30)
	return (instr == 0x6bc23fd1);
}

bool
isStub(const char *hostAddr, int &offset)
{
	// Looking for this pattern:
	// 2600: 4b753f91  LDW         -56(%s0,%r27),%r21
	// 2604: 4b733f99  LDW         -52(%s0,%r27),%r19
	// 2608: 02a010a1  LDSID       (%s0,%r21),%r1
	// 260c: 00011820  MTSP        %r1,%s0
	// 2610: e2a00000  BE          0(%s0,%r21)
	// 2614: 6bc23fd1  STW         %r2,-24(%s0,%r30)
	// Where the only things that vary are the first two offsets (here -56 and -52)
	unsigned instr;
	int offset1, offset2;
	instr = *((unsigned *)hostAddr); hostAddr += 4;
	if (!isLDW(instr, offset1, 21)) return false;
	instr = *((unsigned *)hostAddr); hostAddr += 4;
	if (!isLDW(instr, offset2, 19)) return false;
	instr = *((unsigned *)hostAddr); hostAddr += 4;
	if (!isLDSID(instr)) return false;
	instr = *((unsigned *)hostAddr); hostAddr += 4;
	if (!isMSTP(instr)) return false;
	instr = *((unsigned *)hostAddr); hostAddr += 4;
	if (!isBE(instr)) return false;
	instr = *((unsigned *)hostAddr);
	if (!isSTW(instr)) return false;
	if ((offset2 - offset1) != 4) return false;
	offset = offset1;
	return true;
}
#endif


bool
HpSomBinaryFile::load(std::istream &ifs)
{
	ifs.seekg(0, ifs.end);
	std::streamsize size = ifs.tellg();

	// Allocate a buffer for the image
	m_pImage = new unsigned char[size];

	ifs.seekg(0, ifs.beg);
	ifs.read((char *)m_pImage, size);
	if (!ifs.good()) {
		fprintf(stderr, "Error reading binary file %s\n", getFilename());
		return false;
	}

	// Check type at offset 0x0; should be 0x0210 or 0x20B then
	// 0107, 0108, or 010B
	unsigned magic = BH32(m_pImage);
	unsigned system_id = magic >> 16;
	unsigned a_magic = magic & 0xFFFF;
	if (((system_id != 0x210) && (system_id != 0x20B))
	 || ((a_magic != 0x107) && (a_magic != 0x108) && (a_magic != 0x10B))) {
		fprintf(stderr, "%s is not a standard PA/RISC executable file, with "
		        "system ID %X and magic number %X\n", getFilename(), system_id, a_magic);
		return false;
	}

	// Find the array of aux headers
	unsigned auxHeaders = BH32(m_pImage + 0x1c);
	// Get the size of the aux headers
	unsigned sizeAux = BH32(m_pImage + 0x20);
	if (!auxHeaders || !sizeAux) {
		fprintf(stderr, "Error: auxiliary header array is not present\n");
		return false;
	}
	// Search through the auxiliary headers.
	// There should be one of type 4 ("Exec Auxiliary Header").
	bool found = false;
	unsigned maxAux = auxHeaders + sizeAux;
	while (auxHeaders < maxAux) {
		if ((BH32(m_pImage + auxHeaders) & 0xFFFF) == 0x0004) {
			found = true;
			break;
		}
		// Skip this one; length is at the second word and does NOT include
		// the 2-word header.  Round the length up to the next word boundary.
		auxHeaders += (BH32(m_pImage + auxHeaders + 4) + 11) & ~3;
	}
	if (!found) {
		fprintf(stderr, "Error: Exec auxiliary header not found\n");
		return false;
	}

	// Find the main symbol table, if it exists
	const char *symPtr = (const char *)m_pImage + BH32(m_pImage + 0x5C);
	unsigned numSym = BH32(m_pImage + 0x60);

	// Find the DL Table, if it exists
	// The DL table (Dynamic Link info?) is supposed to be at the start of
	// the $TEXT$ space, but the only way I can presently find that is to
	// assume that the first subspace entry points to it
	char *subspace_location = (char *)m_pImage + BH32(m_pImage + 0x34);
	ADDRESS first_subspace_fileloc = BH32(subspace_location + 8);
	char *DLTable = (char *)m_pImage + first_subspace_fileloc;
	const char *pDlStrings = DLTable + BH32(DLTable + 0x28);
	unsigned numImports = BH32(DLTable + 0x14);    // Number of import strings
	import_entry *import_list = (import_entry *)(DLTable + BH32(DLTable + 0x10));
	unsigned numExports = BH32(DLTable + 0x24);    // Number of export strings
	export_entry *export_list = (export_entry *)(DLTable + BH32(DLTable + 0x20));

// A convenient macro for accessing the fields (0-11) of the auxiliary header
// Fields 0, 1 are the header (flags, aux header type, and size)
#define AUXHDR(idx) (BH32(m_pImage + auxHeaders + 4*idx))

	// Allocate the section information. There will be just four entries:
	// one for the header, one for text, one for initialised data, one for BSS
	sections.reserve(4);

	// Section 0: header
	auto sect0 = SectionInfo();
	sect0.name = "$HEADER$";
	sect0.uNativeAddr = 0;         // Not applicable
	sect0.uHostAddr = (char *)m_pImage;
	//sect0.uSectionSize = AUXHDR(4);
	// There is nothing that appears in memory space here; to give this a size
	// is to invite getSectionInfoByAddr to return this section!
	sect0.uSectionSize = 0;

	sect0.uSectionEntrySize = 1;   // Not applicable
	sect0.bCode = false;
	sect0.bData = false;
	sect0.bBss = false;
	sect0.bReadOnly = false;
	sections.push_back(sect0);

	// Section 1: text (code)
	auto sect1 = SectionInfo();
	sect1.name = "$TEXT$";
	sect1.uNativeAddr = AUXHDR(3);
	sect1.uHostAddr = (char *)m_pImage + AUXHDR(4);
	sect1.uSectionSize = AUXHDR(2);
	sect1.uSectionEntrySize = 1;   // Not applicable
	sect1.bCode = true;
	sect1.bData = false;
	sect1.bBss = false;
	sect1.bReadOnly = true;
	sections.push_back(sect1);

	// Section 2: initialised data
	auto sect2 = SectionInfo();
	sect2.name = "$DATA$";
	sect2.uNativeAddr = AUXHDR(6);
	sect2.uHostAddr = (char *)m_pImage + AUXHDR(7);
	sect2.uSectionSize = AUXHDR(5);
	sect2.uSectionEntrySize = 1;   // Not applicable
	sect2.bCode = false;
	sect2.bData = true;
	sect2.bBss = false;
	sect2.bReadOnly = false;
	sections.push_back(sect2);

	// Section 3: BSS
	auto sect3 = SectionInfo();
	sect3.name = "$BSS$";
	// For now, assume that BSS starts at the end of the initialised data
	sect3.uNativeAddr = AUXHDR(6) + AUXHDR(5);
	sect3.uHostAddr = nullptr;     // Not applicable
	sect3.uSectionSize = AUXHDR(8);
	sect3.uSectionEntrySize = 1;   // Not applicable
	sect3.bCode = false;
	sect3.bData = false;
	sect3.bBss = true;
	sect3.bReadOnly = false;
	sections.push_back(sect3);

	// Work through the imports, and find those for which there are stubs using that import entry.
	// Add the addresses of any such stubs.
	// The "end of data" where r27 points is not necessarily the same as
	// the end of the $DATA$ space. So we have to call getSubSpaceInfo
	std::pair<unsigned, int> pr = getSubspaceInfo("$GLOBAL$");
	//ADDRESS endData = pr.first + pr.second;
	pr = getSubspaceInfo("$PLT$");
	//int minPLT = pr.first - endData;
	//int maxPLT = minPLT + pr.second;
	ADDRESS pltStart = pr.first;
	//cout << "Offset limits are " << dec << minPLT << " and " << maxPLT << endl;
	// Note: DLT entries come before PLT entries in the import array, but
	// the $DLT$ subsection is not necessarily just before the $PLT$
	// subsection in memory.
	int numDLT = BH32(DLTable + 0x40);

	// This code was for pattern patching the BOR (Bind On Reference, or library call stub) routines. It appears to be
	// unnecessary, since as they appear in the file, the PLT entries point to the BORs
#if 0
	const char *startText = sections[1].uHostAddr;
	const char *endText = startText + sections[1].uSectionSize - 0x10;
	for (const char *host = startText; host != endText; host += 4) {
		// Test this location for a BOR (library stub)
		int offset;
		if (isStub(host, offset)) {
			cout << "Found a stub with offset " << dec << offset << endl;
			if ((offset >= minPLT) && (offset < maxPLT)) {
				// This stub corresponds with an import entry
				u = (offset - minPLT) / sizeof(plt_record);
				// Add an offset for the DLT entries
				u += numDLT;
				symbols[import_list[u].name + pDlStrings] = sections[1].uNativeAddr + (host - startText);
				cout << "Added sym " << (import_list[u].name + pDlStrings) << ", value " << hex << (sections[1].uNativeAddr + (host - startText)) << endl;
			}
		}
	}
#endif

	// For each PLT import table entry, add a symbol
	// u runs through import table; v through $PLT$ subspace
	// There should be a one to one correspondance between (DLT + PLT) entries and import table entries.
	// The DLT entries always come first in the import table
	unsigned u = (unsigned)numDLT, v = 0;
	plt_record *PLTs = (plt_record *)&sections[2].uHostAddr[pltStart - sections[2].uNativeAddr];
	for (; u < numImports; ++u, ++v) {
		//cout << "Importing " << (pDlStrings+import_list[u].name) << endl;
		symbols.Add(PLTs[v].value, pDlStrings + BH32(&import_list[u].name));
		// Add it to the set of imports; needed by isDynamicLinkedProc()
		imports.insert(PLTs[v].value);
		//cout << "Added import sym " << (import_list[u].name + pDlStrings) << ", value " << hex << PLTs[v].value << endl;
	}
	// Work through the exports, and find main. This isn't main itself,
	// but in fact a call to main.
	for (u = 0; u < numExports; ++u) {
		//cout << "Exporting " << (pDlStrings+BH32(&export_list[u].name)) << " value " << hex << BH32(&export_list[u].value) << endl;
		if (strncmp(pDlStrings + BH32(&export_list[u].name), "main", 4) == 0) {
			// Enter the symbol "_callmain" for this address
			symbols.Add(BH32(&export_list[u].value), "_callmain");
			// Found call to main. Extract the offset. See assemble_17
			// in pa-risc 1.1 manual page 5-9
			// +--------+--------+--------+----+------------+-+-+
			// | 3A (6) |  t (5) | w1 (5) |0(3)|   w2 (11)  |n|w|  BL
			// +--------+--------+--------+----+------------+-+-+
			//  31    26|25    21|20    16|1513|12         2|1|0
			// +----------------------+--------+-----+----------+
			// |wwww...              w| w1 (5) |w2lsb| w2 msb's | offset
			// +----------------------+--------+-----+----------+
			//  31                  16|15    11| 10  |9        0

			unsigned bincall = *(unsigned *)&sections[1].uHostAddr[BH32(&export_list[u].value) - sections[1].uNativeAddr];
			int offset = (((bincall & 1) << 31) >> 15)     // w
			           | ((bincall & 0x1f0000) >> 5)       // w1
			           | ((bincall &        4) << 8)       // w2@10
			           | ((bincall &   0x1ff8) >> 3);      // w2@0..9
			// Address of main is st + 8 + offset << 2
			symbols.Add(BH32(&export_list[u].value) + 8 + (offset << 2), "main");
			break;
		}
	}

	// Read the main symbol table, if any
	if (numSym) {
		const char *pNames = (const char *)(m_pImage + (int)BH32(m_pImage + 0x6C));
#define SYMSIZE 20              // 5 4-byte words per symbol entry
#define SYMBOLNM(idx)  (BH32(symPtr + idx*SYMSIZE + 4))
#define SYMBOLAUX(idx) (BH32(symPtr + idx*SYMSIZE + 8))
#define SYMBOLVAL(idx) (BH32(symPtr + idx*SYMSIZE + 16))
#define SYMBOLTY(idx)  ((BH32(symPtr + idx*SYMSIZE) >> 24) & 0x3f)
		for (u = 0; u < numSym; ++u) {
			// cout << "Symbol " << pNames+SYMBOLNM(u) << ", type " << SYMBOLTY(u) << ", value " << hex << SYMBOLVAL(u) << ", aux " << SYMBOLAUX(u) << endl;
			unsigned symbol_type = SYMBOLTY(u);
			// Only interested in type 3 (code), 8 (stub), and 12 (millicode)
			if ((symbol_type != 3) && (symbol_type != 8) && (symbol_type != 12))
				continue;
#if 0
			if ((symbol_type == 10) || (symbol_type == 11))
				// These are extension entries; not interested
				continue;
#endif
			const char *pSymName = pNames + SYMBOLNM(u);
			// Ignore symbols starting with one $; for example, there are many
			// $CODE$ (but we want to see helper functions like $$remU)
			if ((pSymName[0] == '$') && (pSymName[1] != '$')) continue;
#if 0
			if ((symbol_type == 6) && (strcmp("main", pSymName) == 0))
				// Entry point for main. Make sure to ignore this entry, else it
				// ends up being the main entry point
				continue;
#endif
			ADDRESS value = SYMBOLVAL(u);
			//if ((symbol_type >= 3) && (symbol_type <= 8))
				// Addresses of code; remove the privilege bits
				value &= ~3;
#if 0
			if (strcmp("main", pNames + SYMBOLNM(u)) == 0) {    // HACK!
				cout << "main at " << hex << value << " has type " << SYMBOLTY(u) << endl;
			}
#endif
			// HP's symbol table is crazy. It seems that imports like printf have entries of type 3 with the wrong
			// value. So we have to check whether the symbol has already been entered (assume first one is correct).
			if (symbols.find(pSymName) == NO_ADDRESS)
				symbols.Add(value, pSymName);
			//cout << "Symbol " << pNames+SYMBOLNM(u) << ", type " << SYMBOLTY(u) << ", value " << hex << value << ", aux " << SYMBOLAUX(u) << endl;  // HACK!
		}
	}  // if (numSym)

	return true;
}

ADDRESS
HpSomBinaryFile::getEntryPoint() const
{
	assert(0); /* FIXME: Someone who understands this file please implement */
	return NO_ADDRESS;
}

#if 0 // Cruft?
bool
HpSomBinaryFile::PostLoad(void *handle)
{
	// Not needed: for archives only
	return false;
}
#endif

#if 0 // Cruft?
bool
HpSomBinaryFile::isLibrary() const
{
	int type =  BH32(m_pImage) & 0xFFFF;
	return type == 0x0104 || type == 0x010D
	    || type == 0x010E || type == 0x0619;
}

ADDRESS
HpSomBinaryFile::getImageBase() const
{
	return 0; /* FIXME */
}

size_t
HpSomBinaryFile::getImageSize() const
{
	return BH32(m_pImage + 0x24);
}
#endif

/**
 * We at least need to be able to name the main function and system calls.
 */
const char *
HpSomBinaryFile::getSymbolByAddress(ADDRESS a)
{
	return symbols.find(a);
}

ADDRESS
HpSomBinaryFile::getAddressByName(const std::string &name, bool bNoTypeOK) const
{
	// For now, we ignore the symbol table and do a linear search of our
	// SymTab table
	return symbols.find(name);
}

/**
 * \returns true if the address matches the convention for A-line system
 * calls.
 */
bool
HpSomBinaryFile::isDynamicLinkedProc(ADDRESS uNative) const
{
	// Look up the address in the set of imports
	return !!imports.count(uNative);
}

/**
 * Get the start and length of a given subspace.
 */
std::pair<ADDRESS, int>
HpSomBinaryFile::getSubspaceInfo(const char *ssname) const
{
	std::pair<ADDRESS, int> ret(0, 0);
	// Get the start and length of the subspace with the given name
	struct subspace_dictionary_record *subSpaces = (struct subspace_dictionary_record *)(m_pImage + BH32(m_pImage + 0x34));
	unsigned numSubSpaces = BH32(m_pImage + 0x38);
	const char *spaceStrings = (const char *)(m_pImage + BH32(m_pImage + 0x44));
	for (unsigned u = 0; u < numSubSpaces; ++u) {
		const char *thisName = spaceStrings + BH32(&subSpaces[u].name);
		unsigned thisNameSize = BH32(spaceStrings + BH32(&subSpaces[u].name) - 4);
		//cout << "Subspace " << thisName << " starts " << hex << subSpaces[u].subspace_start << " length " << subSpaces[u].subspace_length << endl;
		if (thisNameSize == strlen(ssname)
		 && strcmp(thisName, ssname) == 0) {
			ret.first = BH32(&subSpaces[u].subspace_start);
			ret.second = BH32(&subSpaces[u].subspace_length);
			return ret;
		}
	}
	// Failed. Return the zeroes
	return ret;
}

#if 0 // Cruft?
/**
 * Specific to BinaryFile objects that implement a "global pointer".  Gets a
 * pair of unsigned integers representing the address of %agp (first) and the
 * value for GLOBALOFFSET (unused for pa-risc).
 *
 * The understanding at present is that the global data pointer (%r27 for
 * pa-risc) points just past the end of the $GLOBAL$ subspace.
 */
std::pair<unsigned, unsigned>
HpSomBinaryFile::getGlobalPointerInfo()
{
	std::pair<unsigned, unsigned> ret(0, 0);
	// Search the subspace names for "$GLOBAL$
	std::pair<ADDRESS, int> info = getSubspaceInfo("$GLOBAL$");
	// We want the end of the $GLOBAL$ section, which is the sum of the start
	// address and the size
	ret.first = info.first + info.second;
	return ret;
}
#endif

#if 0 // Cruft?
std::map<ADDRESS, const char *> *
HpSomBinaryFile::getDynamicGlobalMap()
{
	// Find the DL Table, if it exists
	// The DL table (Dynamic Link info) is supposed to be at the start of
	// the $TEXT$ space, but the only way I can presently find that is to
	// assume that the first subspace entry points to it
	const char *subspace_location = (const char *)m_pImage + BH32(m_pImage + 0x34);
	ADDRESS first_subspace_fileloc = BH32(subspace_location + 8);
	const char *DLTable = (const char *)m_pImage + first_subspace_fileloc;

	unsigned numDLT = BH32(DLTable + 0x40);
	// Offset 0x38 in the DL table has the offset relative to $DATA$ (section 2)
	unsigned *p = (unsigned *)&sections[2].uHostAddr[BH32(DLTable + 0x38)];

	// The DLT is paralelled by the first <numDLT> entries in the import table;
	// the import table has the symbolic names
	const import_entry *import_list = (import_entry *)(DLTable + BH32(DLTable + 0x10));
	// Those names are in the DLT string table
	const char *pDlStrings = DLTable + BH32(DLTable + 0x28);

	auto ret = new std::map<ADDRESS, const char *>;
	for (unsigned u = 0; u < numDLT; ++u) {
		// ? Sometimes the names are just -1
		if (import_list[u].name == -1)
			continue;
		const char *str = pDlStrings + import_list[u].name;
		(*ret)[*p++] = str;
	}
	return ret;
}
#endif

ADDRESS
HpSomBinaryFile::getMainEntryPoint()
{
	return symbols.find("main");
#if 0
	if (mainExport == 0) {
		// This means we didn't find an export table entry for main
		// didn't load the file
		fprintf(stderr, "Did not find export entry for `main'\n");
		return 0;
	}
	// Expect a bl <main>, rp instruction
	unsigned instr = BH32(&sections[1].uHostAddr[mainExport - sections[1].uNativeAddr]);
	int disp;
	// Standard form: sub-opcode 0, target register = 2
	if (instr >> 26 == 0x3A
	 && ((instr >> 21) & 0x1F) == 2
	 && ((instr >> 13) & 0x7) == 0) {
		disp = (instr & 1) << 16            // w
		     | ((instr >> 16) & 0x1F) << 11 // w1
		     | ((instr >> 2) & 1) << 10     // w2{10}
		     | ((instr >> 3) & 0x3FF);      // w2{0..9}
		// Sign extend
		disp <<= 15; disp >>= 15;
	}
	// Alternate (v2 only?) form: sub-opcode 5, t field becomes w3
	// (extra 5 bits of address range)
	else if (instr >> 26 == 0x3A
	      && ((instr >> 13) & 0x7) == 5) {
		disp = (instr & 1) << 21            // w
		     | ((instr >> 21) & 0x1F) << 16 // w3
		     | ((instr >> 16) & 0x1F) << 11 // w1
		     | ((instr >> 2) & 1) << 10     // w2{10}
		     | ((instr >> 3) & 0x3FF);      // w2{0..9}
		// Sign extend
		disp <<= 10; disp >>= 10;
	} else {
		fprintf(stderr, "Error: expected BL instruction at %X, found %X\n",
		        mainExport, instr);
		return 0;
	}
	// Return the effective destination address
	return mainExport + (disp << 2) + 8;
#endif
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
	return new HpSomBinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (HpSomBinaryFile *)bf;
}
#endif
