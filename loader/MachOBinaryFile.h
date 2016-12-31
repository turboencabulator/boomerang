/**
 * \file
 * \brief Contains the definition of the class MachOBinaryFile.
 *
 * This file contains the definition of the MachOBinaryFile class, and some
 * other definitions specific to the Mac OS-X version of the BinaryFile
 * object.
 *
 * This is my bare bones implementation of a Mac OS-X binary loader.
 *
 * \authors
 * Copyright (C) 2000, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef MACHOBINARYFILE_H
#define MACHOBINARYFILE_H

#include "BinaryFile.h"

#ifndef _MACH_MACHINE_H_                // On OS X, this is already defined
typedef unsigned long cpu_type_t;       // I guessed
typedef unsigned long cpu_subtype_t;    // I guessed
typedef unsigned long vm_prot_t;        // I guessed
#endif

struct mach_header;

class MachOBinaryFile : public BinaryFile {
public:
	                    MachOBinaryFile();
	virtual            ~MachOBinaryFile();

	virtual LOADFMT     getFormat() const { return LOADFMT_MACHO; }
	virtual MACHINE     getMachine() const { return machine; }
	virtual const char *getFilename() const { return m_pFilename; }

	virtual bool        isLibrary() const;
	virtual std::list<const char *> getDependencyList();
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	        DWord       getDelta();

	virtual const char *getSymbolByAddress(ADDRESS dwAddr);  // Get sym from addr
	virtual ADDRESS     getAddressByName(const char *name, bool bNoTypeOK = false);  // Find addr given name
	virtual void        addSymbol(ADDRESS uNative, const char *pName);

//
//      --      --      --      --      --      --      --      --      --
//

	// Internal information
	// Dump headers, etc
	virtual bool        DisplayDetails(const char *fileName, FILE *f = stdout);

protected:
	        int         machORead2(short *ps) const;  // Read 2 bytes from native addr
	        int         machORead4(int *pi) const;    // Read 4 bytes from native addr

	        //void          *BMMH(void *x);
	        char          *BMMH(char *x);
	        const char    *BMMH(const char *x);
	        unsigned int   BMMH(long int &x);
	        unsigned int   BMMH(void *x);
	        unsigned int   BMMH(unsigned long x);
	          signed int   BMMH(signed int x);
	        unsigned int   BMMH(unsigned int x);
	        unsigned short BMMHW(unsigned short x);

public:
	virtual int         readNative1(ADDRESS a);       // Read 1 bytes from native addr
	virtual int         readNative2(ADDRESS a);       // Read 2 bytes from native addr
	virtual int         readNative4(ADDRESS a);       // Read 4 bytes from native addr
	virtual QWord       readNative8(ADDRESS a);       // Read 8 bytes from native addr
	virtual float       readNativeFloat4(ADDRESS a);  // Read 4 bytes as float
	virtual double      readNativeFloat8(ADDRESS a);  // Read 8 bytes as float

	virtual bool        isDynamicLinkedProc(ADDRESS uNative) { return dlprocs.find(uNative) != dlprocs.end(); }
	virtual const char *getDynamicProcName(ADDRESS uNative);

	virtual std::map<ADDRESS, std::string> &getSymbols() { return m_SymA; }
	virtual std::map<std::string, ObjcModule> &getObjcModules() { return modules; }

protected:
	virtual bool        RealLoad(const char *sName); // Load the file; pure virtual

private:
	virtual bool        PostLoad(void *handle);  // Called after archive member loaded
	        void        findJumps(ADDRESS curr);  // Find names for jumps to IATs

	        struct mach_header *header;      // The Mach-O header
	        char       *base;                // Beginning of the loaded image
	        const char *m_pFilename;
	        ADDRESS     entrypoint, loaded_addr;
	        unsigned    loaded_size;
	        MACHINE     machine;
	        bool        swap_bytes;
	        std::map<ADDRESS, std::string> m_SymA, dlprocs;
	        std::map<std::string, ObjcModule> modules;
};

#endif
