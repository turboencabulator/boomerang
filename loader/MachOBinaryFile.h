/**
 * \file
 * \brief Contains the definition of the class MachOBinaryFile, and some other
 * definitions specific to the Mac OS-X version of the BinaryFile object.
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

/**
 * \brief Loader for Mach-O executable files.
 *
 * This is my bare bones implementation of a Mac OS-X binary loader.
 */
class MachOBinaryFile : public BinaryFile {
public:
	                    MachOBinaryFile();
	virtual            ~MachOBinaryFile();

	virtual LOADFMT     getFormat() const { return LOADFMT_MACHO; }
	virtual MACHINE     getMachine() const { return machine; }
	virtual std::list<const char *> getDependencyList();

	virtual bool        isLibrary() const;
	virtual ADDRESS     getImageBase() const;
	virtual size_t      getImageSize() const;

private:
	        int         machORead2(const short *ps) const;
	        int         machORead4(const int *pi) const;
	        unsigned int   BMMH(const void *x);
	        uint32_t       BMMH(uint32_t x);
	        unsigned short BMMHW(unsigned short x);
public:
	virtual int         readNative1(ADDRESS a) const;
	virtual int         readNative2(ADDRESS a) const;
	virtual int         readNative4(ADDRESS a) const;
	virtual QWord       readNative8(ADDRESS a) const;
	virtual float       readNativeFloat4(ADDRESS a) const;
	virtual double      readNativeFloat8(ADDRESS a) const;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	virtual void        addSymbol(ADDRESS uNative, const char *pName);
	virtual const char *getSymbolByAddress(ADDRESS dwAddr);
	virtual ADDRESS     getAddressByName(const char *name, bool bNoTypeOK = false);
	virtual std::map<ADDRESS, std::string> &getSymbols() { return m_SymA; }
	virtual std::map<std::string, ObjcModule> &getObjcModules() { return modules; }
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual bool        isDynamicLinkedProc(ADDRESS uNative) { return dlprocs.find(uNative) != dlprocs.end(); }
	virtual const char *getDynamicProcName(ADDRESS uNative);
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	//        DWord       getDelta();
	/** \} */

protected:
	virtual bool        load(std::istream &);
	//virtual bool        PostLoad(void *handle);

private:
	        char       *base;                ///< Beginning of the loaded image
	        ADDRESS     entrypoint, loaded_addr;
	        unsigned    loaded_size;
	        MACHINE     machine;
	        bool        swap_bytes;
	        std::map<ADDRESS, std::string> m_SymA, dlprocs;
	        std::map<std::string, ObjcModule> modules;
};

#endif
