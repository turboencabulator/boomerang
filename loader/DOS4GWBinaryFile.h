/**
 * \file
 * \brief Contains the definition of the class DOS4GWBinaryFile.
 *
 * This file contains the definition of the DOS4GWBinaryFile class, and some
 * other definitions specific to the exe version of the BinaryFile object.
 *
 * At present, this loader supports the OS2 file format (also known as the
 * Linear eXecutable format) as much as I've found necessary to inspect old
 * DOS4GW apps.  This loader could also be used for decompiling Win9x VxD
 * files or, of course, OS2 binaries, but you're probably better off making a
 * specific loader for each of these.
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

#ifndef DOS4GWBINARYFILE_H
#define DOS4GWBINARYFILE_H

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
	Byte  byteord;
	Byte  wordord;
	DWord formatlvl;
	SWord cputype;
	SWord ostype;
	DWord modulever;
	DWord moduleflags;
	DWord modulenumpages;
	DWord eipobjectnum;
	DWord eip;
	DWord espobjectnum;
	DWord esp;
	DWord pagesize;
	DWord pageoffsetshift;
	DWord fixupsectionsize;
	DWord fixupsectionchksum;
	DWord loadersectionsize;
	DWord loadersectionchksum;
	DWord objtbloffset;
	DWord numobjsinmodule;
	DWord objpagetbloffset;
	DWord objiterpagesoffset;
	DWord resourcetbloffset;
	DWord numresourcetblentries;
	DWord residentnametbloffset;
	DWord entrytbloffset;
	DWord moduledirectivesoffset;
	DWord nummoduledirectives;
	DWord fixuppagetbloffset;
	DWord fixuprecordtbloffset;
	DWord importtbloffset;
	DWord numimportmoduleentries;
	DWord importproctbloffset;
	DWord perpagechksumoffset;
	DWord datapagesoffset;
	DWord numpreloadpages;
	DWord nonresnametbloffset;
	DWord nonresnametbllen;
	DWord nonresnametblchksum;
	DWord autodsobjectnum;
	DWord debuginfooffset;
	DWord debuginfolen;
	DWord numinstancepreload;
	DWord numinstancedemand;
	DWord heapsize;
} LXHeader;

typedef struct PACKED {
	DWord VirtualSize;
	DWord RelocBaseAddr;
	DWord ObjectFlags;
	DWord PageTblIdx;
	DWord NumPageTblEntries;
	DWord Reserved1;
} LXObject;

typedef struct PACKED {
	DWord pagedataoffset;
	SWord datasize;
	SWord flags;
} LXPage;

// this is correct for internal fixups only
typedef struct PACKED {
	unsigned char src;
	unsigned char flags;
	short srcoff;
	//unsigned char object;  // these are now variable length
	//unsigned short trgoff;
} LXFixup;

class DOS4GWBinaryFile : public BinaryFile {
public:
	                    DOS4GWBinaryFile();
	virtual            ~DOS4GWBinaryFile();

	virtual bool        Open(const char *sName);  // Open the file for r/w; ???
	virtual void        Close();                  // Close file opened with Open()
	virtual void        UnLoad();                 // Unload the image
	virtual LOADFMT     getFormat() const { return LOADFMT_LX; }
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

//
//      --      --      --      --      --      --      --      --      --
//

	// Internal information
	// Dump headers, etc
	virtual bool        DisplayDetails(const char *fileName, FILE *f = stdout);

protected:
	        int         dos4gwRead2(short *ps) const;  // Read 2 bytes from native addr
	        int         dos4gwRead4(int *pi) const;    // Read 4 bytes from native addr

public:
	virtual int         readNative1(ADDRESS a);       // Read 1 bytes from native addr
	virtual int         readNative2(ADDRESS a);       // Read 2 bytes from native addr
	virtual int         readNative4(ADDRESS a);       // Read 4 bytes from native addr
	virtual QWord       readNative8(ADDRESS a);       // Read 8 bytes from native addr
	virtual float       readNativeFloat4(ADDRESS a);  // Read 4 bytes as float
	virtual double      readNativeFloat8(ADDRESS a);  // Read 8 bytes as float

	virtual bool        isDynamicLinkedProcPointer(ADDRESS uNative);
	virtual bool        isDynamicLinkedProc(ADDRESS uNative);
	virtual const char *getDynamicProcName(ADDRESS uNative);

	virtual std::map<ADDRESS, std::string> &getSymbols() { return dlprocptrs; }

protected:
	virtual bool        RealLoad(const char *sName);  // Load the file; pure virtual

private:
	virtual bool        PostLoad(void *handle);  // Called after archive member loaded

	        Header     *m_pHeader;      // Pointer to header
	        LXHeader   *m_pLXHeader;    // Pointer to lx header
	        LXObject   *m_pLXObjects;   // Pointer to lx objects
	        LXPage     *m_pLXPages;     // Pointer to lx pages
	        int         m_cbImage;      // Size of image
	        //int         m_cReloc;       // Number of relocation entries
	        //DWord      *m_pRelocTable;  // The relocation table
	        char       *base;           // Beginning of the loaded image
	// Map from address of dynamic pointers to library procedure names:
	        std::map<ADDRESS, std::string> dlprocptrs;
	        const char *m_pFilename;
};

#endif
