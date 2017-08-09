/**
 * \file
 * \brief Contains the definition of the class IntelCoffFile.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef INTELCOFFFILE_H
#define INTELCOFFFILE_H

#include "BinaryFile.h"
#include "SymTab.h"

#include <stdint.h>

struct __attribute__((packed)) coff_header {
	uint16_t coff_magic;
	uint16_t coff_sections;
	uint32_t coff_timestamp;
	uint32_t coff_symtab_ofs;
	uint32_t coff_num_syment;
	uint16_t coff_opthead_size;
	uint16_t coff_flags;
};


/**
 * \brief Loader for COFF executable files.
 */
class IntelCoffFile : public BinaryFile {
public:
	            IntelCoffFile();
	virtual    ~IntelCoffFile();

	LOADFMT     getFormat() const override { return LOADFMT_COFF; }
	MACHINE     getMachine() const override { return MACHINE_PENTIUM; }
	std::list<const char *> getDependencyList() const override;

	bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

private:
	unsigned char *getAddrPtr(ADDRESS a, ADDRESS range) const;
	int         readNative(ADDRESS a, unsigned short n) const;
public:
	int         readNative1(ADDRESS a) const override;
	int         readNative2(ADDRESS a) const override;
	int         readNative4(ADDRESS a) const override;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	const char *getSymbolByAddress(ADDRESS uNative) override;
	const std::map<ADDRESS, std::string> &getSymbols() const override;
	/** \} */

	/**
	 * \name Relocation table functions
	 * \{
	 */
	bool        isRelocationAt(ADDRESS uNative) const override;
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	bool        isDynamicLinkedProc(ADDRESS uNative) const override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *) override;

private:
	std::list<ADDRESS> m_EntryPoints;
	std::list<ADDRESS> m_Relocations;
	struct coff_header m_Header;

	SectionInfo *AddSection(SectionInfo *);

	SymTab m_Symbols;
};

#endif
