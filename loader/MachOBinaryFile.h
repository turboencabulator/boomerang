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
	virtual    ~MachOBinaryFile();

	LOADFMT     getFormat() const override { return LOADFMT_MACHO; }
	MACHINE     getMachine() const override { return machine; }

	bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

private:
	int         machORead2(const short *ps) const;
	int         machORead4(const int *pi) const;
	unsigned int   BMMH(const void *x) const;
	uint32_t       BMMH(uint32_t x) const;
	unsigned short BMMHW(unsigned short x) const;
public:
	int         readNative1(ADDRESS a) const override;
	int         readNative2(ADDRESS a) const override;
	int         readNative4(ADDRESS a) const override;
	QWord       readNative8(ADDRESS a) const override;
	float       readNativeFloat4(ADDRESS a) const override;
	double      readNativeFloat8(ADDRESS a) const override;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	void        addSymbol(ADDRESS uNative, const char *pName) override;
	const char *getSymbolByAddress(ADDRESS dwAddr) override;
	ADDRESS     getAddressByName(const char *name, bool bNoTypeOK = false) const override;
	const std::map<ADDRESS, std::string> &getSymbols() const override { return m_SymA; }
	const std::map<std::string, ObjcModule> &getObjcModules() const override { return modules; }
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	bool        isDynamicLinkedProc(ADDRESS uNative) const override { return dlprocs.find(uNative) != dlprocs.end(); }
	const char *getDynamicProcName(ADDRESS uNative) override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	//DWord       getDelta();
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *handle) override;

private:
	char       *base = nullptr;      ///< Beginning of the loaded image
	ADDRESS     entrypoint, loaded_addr;
	unsigned    loaded_size;
	MACHINE     machine = MACHINE_PPC;
	bool        swap_bytes = false;
	std::map<ADDRESS, std::string> m_SymA, dlprocs;
	std::map<std::string, ObjcModule> modules;
};

#endif
