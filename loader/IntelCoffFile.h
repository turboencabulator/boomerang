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

#include <cstdint>

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

	//bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

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

	SymTab m_Symbols;
};

#endif
