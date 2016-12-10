/**
 * \file
 * \brief Contains the definition of the class Win32BinaryFile.
 *
 * This file contains the definition of the Win32BinaryFile class, and some
 * other definitions specific to the exe version of the BinaryFile object.
 *
 * At present, there is no support for a symbol table.  Win32 files do not use
 * dynamic linking, but it is possible that some files may have debug symbols
 * (in Microsoft Codeview or Borland formats), and these may be implemented in
 * the future.  The debug info may even be exposed as another pseudo section.
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

#include <string>

#define PACKED __attribute__((packed))

/* exe file header, just the signature really */
typedef struct {
	Byte sigLo;  /* .EXE signature: 0x4D 0x5A */
	Byte sigHi;
} Header;

typedef struct PACKED {
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

// The real Win32 name of this struct is IMAGE_SECTION_HEADER
typedef struct PACKED {
	char  ObjectName[8];  // Name
	DWord VirtualSize;
	DWord RVA;            // VirtualAddress
	DWord PhysicalSize;   // SizeOfRawData
	DWord PhysicalOffset; // PointerToRawData
	DWord Reserved1;      // PointerToRelocations
	DWord Reserved2;      // PointerToLinenumbers
	DWord Reserved3;      // WORD NumberOfRelocations; WORD NumberOfLinenumbers;
	DWord Flags;          // Characteristics
} PEObject;

typedef struct PACKED {
	DWord originalFirstThunk; // 0 for end of array; also ptr to hintNameArray
	DWord preSnapDate;    // Time and date the import data was pre-snapped or zero if not pre-snapped
	SWord verMajor;       // Major version number of dll being ref'd
	SWord verMinor;       // Minor "       "
	DWord name;           // RVA of dll name (asciz)
	DWord firstThunk;     // RVA of start of import address table (IAT)
} PEImportDtor;

typedef struct PACKED {
	DWord flags;          // Reserved; 0
	DWord stamp;          // Time/date stamp export data was created
	SWord verMajor;       // Version number can be ...
	SWord verMinor;       //   ... set by user
	DWord name;           // RVA of the ascii string containing the name of the DLL
	DWord base;           // Starting ordinal number for exports in this image. Usually set to 1.
	DWord numEatEntries;  // Number of entries in EAT (Export ADdress Table)
	DWord numNptEntries;  // Number of entries in NPT (Name Pointer Table) (also #entries in the Ordinal Table)
	DWord eatRVA;         // RVA of the EAT
	DWord nptRVA;         // RVA of the NPT
	DWord otRVA;          // RVA of the OT
} PEExportDtor;

class Win32BinaryFile : public BinaryFile {
public:
	                    Win32BinaryFile();
	virtual            ~Win32BinaryFile();

	virtual bool        Open(const char *sName);  // Open the file for r/w; ???
	virtual void        Close();                  // Close file opened with Open()
	virtual void        UnLoad();                 // Unload the image
	virtual LOADFMT     getFormat() const { return LOADFMT_PE; }
	virtual MACHINE     getMachine() const { return MACHINE_PENTIUM; }
	virtual const char *getFilename() const { return m_pFilename; }

	virtual bool        isLibrary() const;
	virtual std::list<const char *> getDependencyList();
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	        DWord       getDelta();

	virtual const char *SymbolByAddress(ADDRESS dwAddr);  // Get sym from addr
	virtual ADDRESS     GetAddressByName(const char *name, bool bNoTypeOK = false);  // Find addr given name
	virtual void        AddSymbol(ADDRESS uNative, const char *pName);
	        void        dumpSymbols();  // For debugging

//
//      --      --      --      --      --      --      --      --      --
//

	// Internal information
	// Dump headers, etc
	virtual bool        DisplayDetails(const char *fileName, FILE *f = stdout);

protected:
	        int         win32Read2(short *ps) const;  // Read 2 bytes from native addr
	        int         win32Read4(int *pi) const;    // Read 4 bytes from native addr

public:
	virtual int         readNative1(ADDRESS a);       // Read 1 bytes from native addr
	virtual int         readNative2(ADDRESS a);       // Read 2 bytes from native addr
	virtual int         readNative4(ADDRESS a);       // Read 4 bytes from native addr
	virtual QWord       readNative8(ADDRESS a);       // Read 8 bytes from native addr
	virtual float       readNativeFloat4(ADDRESS a);  // Read 4 bytes as float
	virtual double      readNativeFloat8(ADDRESS a);  // Read 8 bytes as float

	virtual bool        IsDynamicLinkedProcPointer(ADDRESS uNative);
	virtual bool        IsStaticLinkedLibProc(ADDRESS uNative);
	virtual ADDRESS     IsJumpToAnotherAddr(ADDRESS uNative);
	virtual const char *GetDynamicProcName(ADDRESS uNative);

	        bool        IsMinGWsAllocStack(ADDRESS uNative);
	        bool        IsMinGWsFrameInit(ADDRESS uNative);
	        bool        IsMinGWsFrameEnd(ADDRESS uNative);
	        bool        IsMinGWsCleanupSetup(ADDRESS uNative);
	        bool        IsMinGWsMalloc(ADDRESS uNative);

	virtual std::map<ADDRESS, std::string> &getSymbols() { return dlprocptrs; }

	virtual bool        hasDebugInfo() { return haveDebugInfo; }

protected:
	virtual bool        RealLoad(const char *sName);  // Load the file; pure virtual

private:

	virtual bool        PostLoad(void *handle);  // Called after archive member loaded
	        void        findJumps(ADDRESS curr);  // Find names for jumps to IATs

	        Header     *m_pHeader;      // Pointer to header
	        PEHeader   *m_pPEHeader;    // Pointer to pe header
	        int         m_cbImage;      // Size of image
	        int         m_cReloc;       // Number of relocation entries
	        DWord      *m_pRelocTable;  // The relocation table
	        char       *base;           // Beginning of the loaded image
	// Map from address of dynamic pointers to library procedure names:
	        std::map<ADDRESS, std::string> dlprocptrs;
	        const char *m_pFilename;
	        bool        haveDebugInfo;
	        bool        mingw_main;
};

#endif
