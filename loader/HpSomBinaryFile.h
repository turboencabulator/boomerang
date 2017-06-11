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
	Byte        type;
	Byte        reserved1;
};

struct export_entry {
	int         next;
	int         name;
	int         value;
	int         size;       // Also misc_info
	Byte        type;
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

struct symElem {
	const char *name;                       // Simple symbol table entry
	ADDRESS     value;
};

/**
 * \brief Loader for PA/RISC SOM executable files.
 */
class HpSomBinaryFile : public BinaryFile {
public:
	            HpSomBinaryFile();
	virtual    ~HpSomBinaryFile();

	LOADFMT     getFormat() const override { return LOADFMT_PAR; }
	MACHINE     getMachine() const override { return MACHINE_HPRISC; }
	std::list<const char *> getDependencyList() override;

	bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	const char *getSymbolByAddress(ADDRESS dwAddr) override;
	ADDRESS     getAddressByName(const char *pName, bool bNoTypeOK = false) override;
	//std::map<ADDRESS, const char *> *getDynamicGlobalMap() override;
	/** \} */

	//std::pair<unsigned, unsigned> getGlobalPointerInfo() override;

	/**
	 * \name Analysis functions
	 * \{
	 */
	bool        isDynamicLinkedProc(ADDRESS uNative) override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() override;
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *handle) override;

private:
	std::pair<ADDRESS, int> getSubspaceInfo(const char *ssname);

	unsigned char *m_pImage = NULL;  ///< Points to loaded image.
	SymTab      symbols;             ///< Symbol table object.
	//ADDRESS     mainExport;          ///< Export entry for "main".
	std::set<ADDRESS> imports;       ///< Set of imported proc addr's.
};

#endif
