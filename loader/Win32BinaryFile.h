/**
 * \file
 * \brief Contains the definition of the class Win32BinaryFile, and some other
 * definitions specific to the exe version of the BinaryFile object.
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

#ifndef WIN32BINARYFILE_H
#define WIN32BINARYFILE_H

#include "BinaryFile.h"

/**
 * \brief EXE file header, just the signature really.
 */
typedef struct {
	Byte sigLo;  ///< .EXE signature: 0x4D 0x5A
	Byte sigHi;
} Header;

typedef struct __attribute__((packed)) {
	Byte  sigLo;
	Byte  sigHi;
	SWord sigver;
	SWord cputype;
	SWord numObjects;
	DWord TimeDate;
	DWord Reserved1;
	DWord Reserved2;
	SWord NtHdrSize;
	SWord Flags;
	SWord Reserved3;
	Byte  LMajor;
	Byte  LMinor;
	DWord Reserved4;
	DWord Reserved5;
	DWord Reserved6;
	DWord EntrypointRVA;
	DWord Reserved7;
	DWord Reserved8;
	DWord Imagebase;
	DWord ObjectAlign;
	DWord FileAlign;
	SWord OSMajor;
	SWord OSMinor;
	SWord UserMajor;
	SWord UserMinor;
	SWord SubsysMajor;
	SWord SubsysMinor;
	DWord Reserved9;
	DWord ImageSize;
	DWord HeaderSize;
	DWord FileChecksum;
	SWord Subsystem;
	SWord DLLFlags;
	DWord StackReserveSize;
	DWord StackCommitSize;
	DWord HeapReserveSize;
	DWord HeapCommitSize;
	DWord Reserved10;
	DWord nInterestingRVASizes;
	DWord ExportTableRVA;
	DWord TotalExportDataSize;
	DWord ImportTableRVA;
	DWord TotalImportDataSize;
	DWord ResourceTableRVA;
	DWord TotalResourceDataSize;
	DWord ExceptionTableRVA;
	DWord TotalExceptionDataSize;
	DWord SecurityTableRVA;
	DWord TotalSecurityDataSize;
	DWord FixupTableRVA;
	DWord TotalFixupDataSize;
	DWord DebugTableRVA;
	DWord TotalDebugDirectories;
	DWord ImageDescriptionRVA;
	DWord TotalDescriptionSize;
	DWord MachineSpecificRVA;
	DWord MachineSpecificSize;
	DWord ThreadLocalStorageRVA;
	DWord TotalTLSSize;
} PEHeader;

/**
 * The real Win32 name of this struct is IMAGE_SECTION_HEADER.
 */
typedef struct __attribute__((packed)) {
	char  ObjectName[8];  ///< Name
	DWord VirtualSize;
	DWord RVA;            ///< VirtualAddress
	DWord PhysicalSize;   ///< SizeOfRawData
	DWord PhysicalOffset; ///< PointerToRawData
	DWord Reserved1;      ///< PointerToRelocations
	DWord Reserved2;      ///< PointerToLinenumbers
	DWord Reserved3;      ///< WORD NumberOfRelocations; WORD NumberOfLinenumbers;
	DWord Flags;          ///< Characteristics
} PEObject;

typedef struct __attribute__((packed)) {
	DWord originalFirstThunk; ///< 0 for end of array; also ptr to hintNameArray
	DWord preSnapDate;    ///< Time and date the import data was pre-snapped or zero if not pre-snapped
	SWord verMajor;       ///< Major version number of dll being ref'd
	SWord verMinor;       ///< Minor "       "
	DWord name;           ///< RVA of dll name (asciz)
	DWord firstThunk;     ///< RVA of start of import address table (IAT)
} PEImportDtor;

typedef struct __attribute__((packed)) {
	DWord flags;          ///< Reserved; 0
	DWord stamp;          ///< Time/date stamp export data was created
	SWord verMajor;       ///< Version number can be ...
	SWord verMinor;       ///<   ... set by user
	DWord name;           ///< RVA of the ascii string containing the name of the DLL
	DWord base;           ///< Starting ordinal number for exports in this image. Usually set to 1.
	DWord numEatEntries;  ///< Number of entries in EAT (Export ADdress Table)
	DWord numNptEntries;  ///< Number of entries in NPT (Name Pointer Table) (also #entries in the Ordinal Table)
	DWord eatRVA;         ///< RVA of the EAT
	DWord nptRVA;         ///< RVA of the NPT
	DWord otRVA;          ///< RVA of the OT
} PEExportDtor;


/**
 * \brief Loader for Win32 PE executable files.
 *
 * At present, there is no support for a symbol table.  Win32 files do not use
 * dynamic linking, but it is possible that some files may have debug symbols
 * (in Microsoft Codeview or Borland formats), and these may be implemented in
 * the future.  The debug info may even be exposed as another pseudo section.
 */
class Win32BinaryFile : public BinaryFile {
public:
	            Win32BinaryFile();
	virtual    ~Win32BinaryFile();

	LOADFMT     getFormat() const override { return LOADFMT_PE; }
	MACHINE     getMachine() const override { return MACHINE_PENTIUM; }
	std::list<const char *> getDependencyList() override;

	bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

protected:
	int         win32Read2(const short *ps) const;
	int         win32Read4(const int *pi) const;
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
	void        dumpSymbols();
	const char *getSymbolByAddress(ADDRESS dwAddr) override;
	ADDRESS     getAddressByName(const char *name, bool bNoTypeOK = false) override;
	const std::map<ADDRESS, std::string> &getSymbols() const override { return dlprocptrs; }
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	ADDRESS     isJumpToAnotherAddr(ADDRESS uNative) const override;
	bool        isStaticLinkedLibProc(ADDRESS uNative) const override;
	bool        isDynamicLinkedProcPointer(ADDRESS uNative) override;
	const char *getDynamicProcName(ADDRESS uNative) override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	//DWord       getDelta();
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *handle) override;

private:
	bool        isMinGWsAllocStack(ADDRESS uNative) const;
	bool        isMinGWsFrameInit(ADDRESS uNative) const;
	bool        isMinGWsFrameEnd(ADDRESS uNative) const;
	bool        isMinGWsCleanupSetup(ADDRESS uNative) const;
	bool        isMinGWsMalloc(ADDRESS uNative) const;

	void        findJumps(ADDRESS curr);

	Header     *m_pHeader;          ///< Pointer to header.
	PEHeader   *m_pPEHeader;        ///< Pointer to pe header.
	int         m_cbImage;          ///< Size of image.
	int         m_cReloc;           ///< Number of relocation entries.
	DWord      *m_pRelocTable;      ///< The relocation table.
	unsigned char *base = nullptr;  ///< Beginning of the loaded image.
	std::map<ADDRESS, std::string> dlprocptrs;  ///< Map from address of dynamic pointers to library procedure names.
	bool        mingw_main = false;
};

#endif
