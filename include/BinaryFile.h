/**
 * \file
 * \brief Contains the definition of the abstract class BinaryFile.
 *
 * \authors
 * Copyright (C) 1997-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef BINARYFILE_H
#define BINARYFILE_H

#include "types.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#include <cstdio>  // For FILE

// Given a pointer p, returns the 16 bits (halfword) in the two bytes
// starting at p.
#define LH(p) \
  ( ((int)((Byte *)(p))[0]     ) \
  + ((int)((Byte *)(p))[1] << 8) )

// Given a little endian value x, load its value assuming little endian order
// Note: must be able to take address of x
// Note: Unlike the LH macro, the parameter is not a pointer
#define LMMH(x) \
  ( ((unsigned)((Byte *)(&x))[0]      ) \
  + ((unsigned)((Byte *)(&x))[1] <<  8) \
  + ((unsigned)((Byte *)(&x))[2] << 16) \
  + ((unsigned)((Byte *)(&x))[3] << 24) )

// With this one, x is a pointer to unsigned
#define LMMH2(x) \
  ( ((unsigned)((Byte *)(x))[0]      ) \
  + ((unsigned)((Byte *)(x))[1] <<  8) \
  + ((unsigned)((Byte *)(x))[2] << 16) \
  + ((unsigned)((Byte *)(x))[3] << 24) )

#define LMMHw(x) \
  ( ((unsigned)((Byte *)(&x))[0]     ) \
  + ((unsigned)((Byte *)(&x))[1] << 8) )

// Given a little endian value x, load its value assuming big endian order
// Note: must be able to take address of x
// Note: Unlike the LH macro, the parameter is not a pointer
#define _BMMH(x) \
  ( ((unsigned)((Byte *)(&x))[3]      ) \
  + ((unsigned)((Byte *)(&x))[2] <<  8) \
  + ((unsigned)((Byte *)(&x))[1] << 16) \
  + ((unsigned)((Byte *)(&x))[0] << 24) )

// With this one, x is a pointer to unsigned
#define _BMMH2(x) \
  ( ((unsigned)((Byte *)(x))[3]      ) \
  + ((unsigned)((Byte *)(x))[2] <<  8) \
  + ((unsigned)((Byte *)(x))[1] << 16) \
  + ((unsigned)((Byte *)(x))[0] << 24) )

#define _BMMHW(x) \
  ( ((unsigned)((Byte *)(&x))[1]     ) \
  + ((unsigned)((Byte *)(&x))[0] << 8) )


/**
 * SectionInfo structure.  BinaryFile::getSectionInfo returns a pointer to an
 * array of these structs.  All information about the sections is contained in
 * these structures.
 */
struct SectionInfo {
	            SectionInfo();
	virtual    ~SectionInfo();      // Quell a warning in gcc

	/**
	 * Windows's PE file sections can contain any combination of code,
	 * data and bss.  As such, it can't be correctly described by
	 * SectionInfo, why we need to override the behaviour of (at least)
	 * the question "Is this address in BSS".
	 */
	virtual bool isAddressBss(ADDRESS a) const { return bBss != 0; }

	const char *pSectionName;       ///< Name of section.
	ADDRESS     uNativeAddr;        ///< Logical or native load address.
	ADDRESS     uHostAddr;          ///< Host or actual address of data.
	ADDRESS     uSectionSize;       ///< Size of section in bytes.
	ADDRESS     uSectionEntrySize;  ///< Size of one section entry (if applic).
	unsigned    uType;              ///< Type of section (format dependent).
	unsigned    bCode     : 1;      ///< Set if section contains instructions.
	unsigned    bData     : 1;      ///< Set if section contains data.
	unsigned    bBss      : 1;      ///< Set if section is BSS (allocated only).
	unsigned    bReadOnly : 1;      ///< Set if this is a read only section.
};


// Objective-C stuff
class ObjcIvar {
public:
	std::string name, type;
	unsigned offset;
};

class ObjcMethod {
public:
	std::string name, types;
	ADDRESS addr;
};

class ObjcClass {
public:
	std::string name;
	std::map<std::string, ObjcIvar> ivars;
	std::map<std::string, ObjcMethod> methods;
};

class ObjcModule {
public:
	std::string name;
	std::map<std::string, ObjcClass> classes;
};


/**
 * This enum allows a sort of run time type identification, without using
 * compiler specific features.
 */
enum LOADFMT {
	LOADFMT_ELF,
	LOADFMT_PE,
	LOADFMT_PALM,
	LOADFMT_PAR,
	LOADFMT_EXE,
	LOADFMT_MACHO,
	LOADFMT_LX,
	LOADFMT_COFF,

	LOADFMT_UNKNOWN,
};

enum MACHINE {
	MACHINE_PENTIUM,
	MACHINE_SPARC,
	MACHINE_HPRISC,
	MACHINE_PALM,
	MACHINE_PPC,
	MACHINE_ST20,
	MACHINE_MIPS,
};


/**
 * \brief Base class for loaders.
 */
class BinaryFile {
	friend class ArchiveFile;

protected:
	                    BinaryFile(bool bArchive = false);
	virtual            ~BinaryFile();
public:
	// Creates and returns an instance of the appropriate subclass.
	static  BinaryFile *open(const char *name);
	// Destroys an instance created by open() or new.
	static  void        close(BinaryFile *bf);

#ifdef DYNAMIC
private:
	// Needed by BinaryFile::close to destroy an instance and unload its library.
	typedef BinaryFile *(*constructFcn)();
	typedef void        (*destructFcn)(BinaryFile *bf);
	        void       *dlHandle;
	        destructFcn destruct;
#endif

public:
	/// Get the format (e.g. LOADFMT_ELF).
	virtual LOADFMT     getFormat() const = 0;
	/// Get the expected machine (e.g. MACHINE_PENTIUM).
	virtual MACHINE     getMachine() const = 0;
	        const char *getFilename() const { return m_pFilename; }
	/// Return a list of library names which the binary file depends on.
	virtual std::list<const char *> getDependencyList() = 0;

	/**
	 * Return whether or not the object is a library file.
	 */
	virtual bool        isLibrary() const = 0;
	/**
	 * Return whether the object can be relocated if necessary (i.e. if it
	 * is not tied to a particular base address).  If not, the object must
	 * be loaded at the address given by getImageBase().
	 */
	virtual bool        isRelocatable() const { return isLibrary(); }
	/**
	 * Return the virtual address at which the binary expects to be
	 * loaded.  For position independent / relocatable code this should be
	 * NO_ADDRESS.
	 */
	virtual ADDRESS     getImageBase() = 0;
	/**
	 * Return the total size of the loaded image.
	 */
	virtual size_t      getImageSize() = 0;
	        ADDRESS     getLimitTextLow() { return limitTextLow; }
	        ADDRESS     getLimitTextHigh() { return limitTextHigh; }
	        int         getTextDelta() { return textDelta; }


	virtual int         readNative1(ADDRESS a);
	virtual int         readNative2(ADDRESS a);
	virtual int         readNative4(ADDRESS a);
	virtual QWord       readNative8(ADDRESS a);
	virtual float       readNativeFloat4(ADDRESS a);
	virtual double      readNativeFloat8(ADDRESS a);

	/**
	 * \name Section functions
	 * \{
	 */
	        int         getNumSections() const;
	        int         getSectionIndexByName(const char *sName);
	        SectionInfo *getSectionInfo(int idx) const;
	        SectionInfo *getSectionInfoByName(const char *sName);
	        SectionInfo *getSectionInfoByAddr(ADDRESS uEntry) const;
	        bool        isReadOnly(ADDRESS uEntry);
	/** \} */

	/**
	 * \name Symbol table functions
	 * \{
	 */
	virtual void        addSymbol(ADDRESS uNative, const char *pName);
	virtual const char *getSymbolByAddress(ADDRESS uNative);
	virtual ADDRESS     getAddressByName(const char *pName, bool bNoTypeOK = false);
	virtual int         getSizeByName(const char *pName, bool bTypeOK = false);
	virtual const char *getFilenameSymbolFor(const char *sym) { return NULL; }
	virtual ADDRESS    *getImportStubs(int &numImports);
	virtual std::vector<ADDRESS> getExportedAddresses(bool funcsOnly = true) { return std::vector<ADDRESS>(); }
	virtual std::map<ADDRESS, const char *> *getDynamicGlobalMap();
	virtual std::map<ADDRESS, std::string> &getFuncSymbols() { return *new std::map<ADDRESS, std::string>(); }
	virtual std::map<ADDRESS, std::string> &getSymbols() { return *new std::map<ADDRESS, std::string>(); }
	virtual std::map<std::string, ObjcModule> &getObjcModules() { return *new std::map<std::string, ObjcModule>(); }
	/** \} */

	/**
	 * \name Relocation table functions
	 * \{
	 */
	//virtual bool        isAddressRelocatable(ADDRESS uNative);
	//virtual ADDRESS     getRelocatedAddress(ADDRESS uNative);
	//virtual ADDRESS     applyRelocation(ADDRESS uNative, ADDRESS uWord);
	// Get symbol associated with relocation at address, if any
	//virtual const char *getRelocSym(ADDRESS uNative, ADDRESS *a = NULL, unsigned int *sz = NULL) { return NULL; }
	virtual bool        isRelocationAt(ADDRESS uNative) { return false; }
	/** \} */

	//virtual std::pair<unsigned, unsigned> getGlobalPointerInfo();

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual ADDRESS     isJumpToAnotherAddr(ADDRESS uNative);
	virtual bool        isStaticLinkedLibProc(ADDRESS uNative);
	virtual bool        isDynamicLinkedProc(ADDRESS uNative);
	virtual bool        isDynamicLinkedProcPointer(ADDRESS uNative);
	virtual const char *getDynamicProcName(ADDRESS uNative);

	/**
	 * Returns a list of pointers to SectionInfo structs representing
	 * entry points to the program.
	 */
	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main") = 0;
	/**
	 * Returns the entry point to main.
	 */
	virtual ADDRESS     getMainEntryPoint() = 0;
	/**
	 * Return the "real" entry point, i.e. where execution of the program
	 * begins.
	 */
	virtual ADDRESS     getEntryPoint() = 0;
	/** \} */

	//virtual bool        hasDebugInfo() { return false; }

	virtual bool        DisplayDetails(const char *fileName, FILE *f = stdout);

protected:
	/**
	 * \brief Load the file.
	 *
	 * \returns true for a good load.
	 */
	virtual bool        RealLoad(const char *sName) = 0;
#if 0 // Cruft?
	/**
	 * \brief Special load function for archive members.
	 *
	 * For archive files only.
	 * Called after archive member loaded.
	 */
	virtual bool        PostLoad(void *handle) = 0;
#endif

	        void        getTextLimits();

	        const char *m_pFilename;     ///< Input file name.
	        bool        m_bArchive;      ///< True if archive member.
	        int         m_iNumSections;  ///< Number of sections.
	        SectionInfo *m_pSections;    ///< The section info.
	        ADDRESS     m_uInitPC;       ///< Initial program counter.
	        ADDRESS     m_uInitSP;       ///< Initial stack pointer.

	        ADDRESS     limitTextLow;    ///< Lowest used native address (inclusive) in the text segment.
	        ADDRESS     limitTextHigh;   ///< Highest used native address (not inclusive) in the text segment.

	/**
	 * Difference between the host and native addresses (host - native).
	 * At this stage, we are assuming that the difference is the same for
	 * all text sections of the BinaryFile image.
	 */
	        int         textDelta;
};

#endif
