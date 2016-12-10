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

/*==============================================================================
 * Dependencies.
 *============================================================================*/

#include "BinaryFile.h"
#include "SymTab.h"

#include <set>

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

class HpSomBinaryFile : public BinaryFile {
public:
	                    HpSomBinaryFile();
	virtual            ~HpSomBinaryFile();

	virtual bool        Open(const char *sName);  // Open the file for r/w; pv
	virtual void        Close();                  // Close file opened with Open()
	virtual void        UnLoad();                 // Unload the image
	virtual bool        PostLoad(void *handle);   // For archive files only
	virtual LOADFMT     getFormat() const { return LOADFMT_PAR; }
	virtual MACHINE     getMachine() const { return MACHINE_HPRISC; }
	virtual const char *getFilename() const { return m_pFilename; }

	virtual bool        isLibrary() const;
	virtual std::list<const char *> getDependencyList();
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

	// Get a symbol given an address
	virtual const char *SymbolByAddress(ADDRESS dwAddr);
	// Lookup the name, return the address
	virtual ADDRESS     GetAddressByName(const char *pName, bool bNoTypeOK = false);
	// Return true if the address matches the convention for A-line system calls
	virtual bool        IsDynamicLinkedProc(ADDRESS uNative);

	// Specific to BinaryFile objects that implement a "global pointer"
	// Gets a pair of unsigned integers representing the address of %agp (first)
	// and the value for GLOBALOFFSET (unused for pa-risc)
	virtual std::pair<unsigned, unsigned> GetGlobalPointerInfo();

	// Get a map from ADDRESS to const char*. This map contains the native
	// addresses and symbolic names of global data items (if any) which are
	// shared with dynamically linked libraries. Example: __iob (basis for
	// stdout).The ADDRESS is the native address of a pointer to the real dynamic data object.
	virtual std::map<ADDRESS, const char *> *GetDynamicGlobalMap();

//
//  --  --  --  --  --  --  --  --  --  --  --
//

	// Internal information
	// Dump headers, etc
	//virtual bool        DisplayDetails(const char *fileName, FILE *f = stdout);

	// Analysis functions
	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();

	        //bool        IsDynamicLinkedProc(ADDRESS wNative);
	        //ADDRESS     NativeToHostAddress(ADDRESS uNative);

protected:
	virtual bool        RealLoad(const char *sName);  // Load the file; pure virtual

private:
	// Private method to get the start and length of a given subspace
	        std::pair<ADDRESS, int> getSubspaceInfo(const char *ssname);

	        unsigned char *m_pImage;    // Points to loaded image
	        SymTab      symbols;        // Symbol table object
	        //ADDRESS     mainExport;     // Export entry for "main"
	        std::set<ADDRESS> imports;  // Set of imported proc addr's
	        const char *m_pFilename;
};

#endif
