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

#include <istream>
#include <list>
#include <map>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdint>

// Little-endian to host-endian conversions
#define LH16(p) \
  ( ((unsigned)((uint8_t *)(p))[0]     ) \
  + ((unsigned)((uint8_t *)(p))[1] << 8) )

#define LH32(p) \
  ( ((unsigned)((uint8_t *)(p))[0]      ) \
  + ((unsigned)((uint8_t *)(p))[1] <<  8) \
  + ((unsigned)((uint8_t *)(p))[2] << 16) \
  + ((unsigned)((uint8_t *)(p))[3] << 24) )

// Big-endian to host-endian conversions
#define BH16(p) \
  ( ((unsigned)((uint8_t *)(p))[1]     ) \
  + ((unsigned)((uint8_t *)(p))[0] << 8) )

#define BH32(p) \
  ( ((unsigned)((uint8_t *)(p))[3]      ) \
  + ((unsigned)((uint8_t *)(p))[2] <<  8) \
  + ((unsigned)((uint8_t *)(p))[1] << 16) \
  + ((unsigned)((uint8_t *)(p))[0] << 24) )


/**
 * BinaryFile::getSectionInfo returns a pointer to an array of these structs.
 * All information about the sections is contained in these structures.
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

	std::string name;                   ///< Name of section.
	char       *uHostAddr = nullptr;    ///< Host or actual address of data.
	ADDRESS     uNativeAddr = 0;        ///< Logical or native load address.
	ADDRESS     uSectionSize = 0;       ///< Size of section in bytes.
	ADDRESS     uSectionEntrySize = 0;  ///< Size of one section entry (if applic).
	unsigned    uType = 0;              ///< Type of section (format dependent).
	unsigned    bCode     : 1;          ///< Set if section contains instructions.
	unsigned    bData     : 1;          ///< Set if section contains data.
	unsigned    bBss      : 1;          ///< Set if section is BSS (allocated only).
	unsigned    bReadOnly : 1;          ///< Set if this is a read only section.
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
 * compiler-specific features.
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
	        void       *dlHandle = nullptr;
	        destructFcn destruct = nullptr;
#endif

public:
	/// Get the format (e.g. LOADFMT_ELF).
	virtual LOADFMT     getFormat() const = 0;
	/// Get the expected machine (e.g. MACHINE_PENTIUM).
	virtual MACHINE     getMachine() const = 0;
	        const char *getFilename() const { return m_pFilename; }
	/// Return a list of library names which the binary file depends on.
	//virtual std::list<const char *> getDependencyList() const { return std::list<const char *>(); }

#if 0 // Cruft?
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
	virtual ADDRESS     getImageBase() const = 0;
	/**
	 * Return the total size of the loaded image.
	 */
	virtual size_t      getImageSize() const = 0;
#endif

	        ADDRESS     getLimitTextLow() const { return limitTextLow; }
	        ADDRESS     getLimitTextHigh() const { return limitTextHigh; }

	        int         readNative1(ADDRESS a) const;
	        int         readNative2(ADDRESS a) const;
	        int         readNative4(ADDRESS a) const;
	        uint64_t    readNative8(ADDRESS a) const;
	        float       readNativeFloat4(ADDRESS a) const;
	        double      readNativeFloat8(ADDRESS a) const;

	/**
	 * \name Section functions
	 * \{
	 */
protected:
	        char       *getSectionData(ADDRESS a, ADDRESS range) const;
public:
	        size_t      getNumSections() const;
	        const SectionInfo *getSectionInfo(size_t) const;
	        const SectionInfo *getSectionInfoByName(const std::string &) const;
	        const SectionInfo *getSectionInfoByAddr(ADDRESS uEntry) const;
	        bool        isReadOnly(ADDRESS uEntry) const;
	/** \} */

	/**
	 * \name Symbol table functions
	 * \{
	 */
	virtual void        addSymbol(ADDRESS, const std::string &);
	virtual const char *getSymbolByAddress(ADDRESS uNative);
	virtual ADDRESS     getAddressByName(const std::string &, bool = false) const;
	virtual int         getSizeByName(const std::string &, bool = false) const;
	virtual const char *getFilenameSymbolFor(const std::string &sym) { return nullptr; }
	//virtual ADDRESS    *getImportStubs(int &numImports);
	//virtual std::vector<ADDRESS> getExportedAddresses(bool funcsOnly = true) { return std::vector<ADDRESS>(); }
	//virtual std::map<ADDRESS, const char *> *getDynamicGlobalMap();
	//virtual const std::map<ADDRESS, std::string> &getFuncSymbols() const { return *new std::map<ADDRESS, std::string>(); }
	virtual const std::map<ADDRESS, std::string> &getSymbols() const { return *new std::map<ADDRESS, std::string>(); }
	virtual const std::map<std::string, ObjcModule> &getObjcModules() const { return *new std::map<std::string, ObjcModule>(); }
	/** \} */

	/**
	 * \name Relocation table functions
	 * \{
	 */
	//virtual bool        isAddressRelocatable(ADDRESS uNative);
	//virtual ADDRESS     getRelocatedAddress(ADDRESS uNative);
	//virtual ADDRESS     applyRelocation(ADDRESS uNative, ADDRESS uWord);
	// Get symbol associated with relocation at address, if any
	//virtual const char *getRelocSym(ADDRESS uNative, ADDRESS *a = nullptr, unsigned int *sz = nullptr) { return nullptr; }
	virtual bool        isRelocationAt(ADDRESS uNative) const { return false; }
	/** \} */

	//virtual std::pair<unsigned, unsigned> getGlobalPointerInfo();

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual ADDRESS     isJumpToAnotherAddr(ADDRESS uNative) const;
	virtual bool        isStaticLinkedLibProc(ADDRESS uNative) const;
	virtual bool        isDynamicLinkedProc(ADDRESS uNative) const;
	virtual bool        isDynamicLinkedProcPointer(ADDRESS uNative) const;
	virtual const char *getDynamicProcName(ADDRESS uNative) const;

	/**
	 * Returns the entry point to main.
	 */
	virtual ADDRESS     getMainEntryPoint() = 0;
	/**
	 * Return the "real" entry point, i.e. where execution of the program
	 * begins.
	 */
	virtual ADDRESS     getEntryPoint() const = 0;
	/** \} */

protected:
	/**
	 * \brief Load the file.
	 *
	 * \returns true for a good load.
	 */
	virtual bool        load(std::istream &) = 0;
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

	        const char *m_pFilename = nullptr;   ///< Input file name.
	        bool        m_bArchive;              ///< True if archive member.
	        bool        bigendian = false;       ///< Section data in big-endian byte order?
	        std::vector<SectionInfo> sections;   ///< The section info.

	        ADDRESS     limitTextLow;            ///< Lowest used native address (inclusive) in the text segment.
	        ADDRESS     limitTextHigh;           ///< Highest used native address (not inclusive) in the text segment.
};

#endif
