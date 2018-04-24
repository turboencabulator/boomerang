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
	uint8_t sigLo;  ///< .EXE signature: 0x4D 0x5A
	uint8_t sigHi;
} Header;

typedef struct __attribute__((packed)) {
	uint8_t  sigLo;
	uint8_t  sigHi;
	uint16_t sigver;
	uint16_t cputype;
	uint16_t numObjects;
	uint32_t TimeDate;
	uint32_t Reserved1;
	uint32_t Reserved2;
	uint16_t NtHdrSize;
	uint16_t Flags;
	uint16_t Reserved3;
	uint8_t  LMajor;
	uint8_t  LMinor;
	uint32_t Reserved4;
	uint32_t Reserved5;
	uint32_t Reserved6;
	uint32_t EntrypointRVA;
	uint32_t Reserved7;
	uint32_t Reserved8;
	uint32_t Imagebase;
	uint32_t ObjectAlign;
	uint32_t FileAlign;
	uint16_t OSMajor;
	uint16_t OSMinor;
	uint16_t UserMajor;
	uint16_t UserMinor;
	uint16_t SubsysMajor;
	uint16_t SubsysMinor;
	uint32_t Reserved9;
	uint32_t ImageSize;
	uint32_t HeaderSize;
	uint32_t FileChecksum;
	uint16_t Subsystem;
	uint16_t DLLFlags;
	uint32_t StackReserveSize;
	uint32_t StackCommitSize;
	uint32_t HeapReserveSize;
	uint32_t HeapCommitSize;
	uint32_t Reserved10;
	uint32_t nInterestingRVASizes;
	uint32_t ExportTableRVA;
	uint32_t TotalExportDataSize;
	uint32_t ImportTableRVA;
	uint32_t TotalImportDataSize;
	uint32_t ResourceTableRVA;
	uint32_t TotalResourceDataSize;
	uint32_t ExceptionTableRVA;
	uint32_t TotalExceptionDataSize;
	uint32_t SecurityTableRVA;
	uint32_t TotalSecurityDataSize;
	uint32_t FixupTableRVA;
	uint32_t TotalFixupDataSize;
	uint32_t DebugTableRVA;
	uint32_t TotalDebugDirectories;
	uint32_t ImageDescriptionRVA;
	uint32_t TotalDescriptionSize;
	uint32_t MachineSpecificRVA;
	uint32_t MachineSpecificSize;
	uint32_t ThreadLocalStorageRVA;
	uint32_t TotalTLSSize;
} PEHeader;

/**
 * The real Win32 name of this struct is IMAGE_SECTION_HEADER.
 */
typedef struct __attribute__((packed)) {
	char  ObjectName[8];  ///< Name
	uint32_t VirtualSize;
	uint32_t RVA;            ///< VirtualAddress
	uint32_t PhysicalSize;   ///< SizeOfRawData
	uint32_t PhysicalOffset; ///< PointerToRawData
	uint32_t Reserved1;      ///< PointerToRelocations
	uint32_t Reserved2;      ///< PointerToLinenumbers
	uint32_t Reserved3;      ///< WORD NumberOfRelocations; WORD NumberOfLinenumbers;
	uint32_t Flags;          ///< Characteristics
} PEObject;

typedef struct __attribute__((packed)) {
	uint32_t originalFirstThunk; ///< 0 for end of array; also ptr to hintNameArray
	uint32_t preSnapDate;    ///< Time and date the import data was pre-snapped or zero if not pre-snapped
	uint16_t verMajor;       ///< Major version number of dll being ref'd
	uint16_t verMinor;       ///< Minor "       "
	uint32_t name;           ///< RVA of dll name (asciz)
	uint32_t firstThunk;     ///< RVA of start of import address table (IAT)
} PEImportDtor;

typedef struct __attribute__((packed)) {
	uint32_t flags;          ///< Reserved; 0
	uint32_t stamp;          ///< Time/date stamp export data was created
	uint16_t verMajor;       ///< Version number can be ...
	uint16_t verMinor;       ///<   ... set by user
	uint32_t name;           ///< RVA of the ascii string containing the name of the DLL
	uint32_t base;           ///< Starting ordinal number for exports in this image. Usually set to 1.
	uint32_t numEatEntries;  ///< Number of entries in EAT (Export ADdress Table)
	uint32_t numNptEntries;  ///< Number of entries in NPT (Name Pointer Table) (also #entries in the Ordinal Table)
	uint32_t eatRVA;         ///< RVA of the EAT
	uint32_t nptRVA;         ///< RVA of the NPT
	uint32_t otRVA;          ///< RVA of the OT
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

	//bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	void        addSymbol(ADDRESS uNative, const char *pName) override;
	//void        dumpSymbols() const;
	const char *getSymbolByAddress(ADDRESS dwAddr) override;
	ADDRESS     getAddressByName(const char *name, bool bNoTypeOK = false) const override;
	const std::map<ADDRESS, std::string> &getSymbols() const override { return dlprocptrs; }
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	ADDRESS     isJumpToAnotherAddr(ADDRESS uNative) const override;
	bool        isStaticLinkedLibProc(ADDRESS uNative) const override;
	bool        isDynamicLinkedProcPointer(ADDRESS uNative) const override;
	const char *getDynamicProcName(ADDRESS uNative) const override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
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
	uint32_t   *m_pRelocTable;      ///< The relocation table.
	unsigned char *base = nullptr;  ///< Beginning of the loaded image.
	std::map<ADDRESS, std::string> dlprocptrs;  ///< Map from address of dynamic pointers to library procedure names.
	bool        mingw_main = false;
};

#endif
