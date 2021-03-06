/**
 * \file
 * \brief Contains the definition of the class HpSomBinaryFile.
 *
 * \authors
 * Copyright (C) 2000-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef HPSOMBINARYFILE_H
#define HPSOMBINARYFILE_H

#include "BinaryFile.h"
#include "SymTab.h"

#include <set>
#include <utility>

struct import_entry {
	int         name;
	short       reserved2;
	uint8_t     type;
	uint8_t     reserved1;
};

struct export_entry {
	int         next;
	int         name;
	int         value;
	int         size;       // Also misc_info
	uint8_t     type;
	char        reserved1;
	short       module_index;
};

struct space_dictionary_record {
	unsigned    name;
	unsigned    flags;
	int         space_number;
	int         subspace_index;
	unsigned    subspace_quantity;
	int         loader_fix_index;
	unsigned    loader_fix_quantity;
	int         init_pointer_index;
	unsigned    init_pointer_quantity;
};

struct subspace_dictionary_record {
	int         space_index;
	unsigned    flags;
	int         file_loc_init_value;
	unsigned    initialization_length;
	unsigned    subspace_start;
	unsigned    subspace_length;
	unsigned    alignment;
	unsigned    name;
	int         fixup_request_index;
	int         fixup_request_quantity;
};

struct plt_record {
	ADDRESS     value;                      // Address in the library
	ADDRESS     r19value;                   // r19 value needed
};

/**
 * \brief Loader for PA/RISC SOM executable files.
 */
class HpSomBinaryFile : public BinaryFile {
public:
	            HpSomBinaryFile();
	           ~HpSomBinaryFile() override;

	LOADFMT     getFormat() const override { return LOADFMT_PAR; }
	MACHINE     getMachine() const override { return MACHINE_HPRISC; }

	//bool        isLibrary() const override;
	//ADDRESS     getImageBase() const override;
	//size_t      getImageSize() const override;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	const char *getSymbolByAddress(ADDRESS dwAddr) override;
	ADDRESS     getAddressByName(const std::string &, bool = false) const override;
	//std::map<ADDRESS, const char *> *getDynamicGlobalMap() override;
	/** \} */

	//std::pair<unsigned, unsigned> getGlobalPointerInfo() override;

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
	//bool        PostLoad(void *handle) override;

private:
	std::pair<ADDRESS, int> getSubspaceInfo(const char *ssname) const;

	unsigned char *m_pImage = nullptr;  ///< Points to loaded image.
	SymTab      symbols;                ///< Symbol table object.
	//ADDRESS     mainExport;             ///< Export entry for "main".
	std::set<ADDRESS> imports;          ///< Set of imported proc addr's.
};

#endif
