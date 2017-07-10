/**
 * \file
 * \brief Contains the definition of the class DOS4GWBinaryFile, and some
 * other definitions specific to the exe version of the BinaryFile object.
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

#include <map>
#include <string>

#if 0
/* exe file header, just the signature really */
typedef struct {
	Byte sigLo;  /* .EXE signature: 0x4D 0x5A */
	Byte sigHi;
} Header;
#endif

typedef struct __attribute__((packed)) {
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

typedef struct __attribute__((packed)) {
	DWord VirtualSize;
	DWord RelocBaseAddr;
	DWord ObjectFlags;
	DWord PageTblIdx;
	DWord NumPageTblEntries;
	DWord Reserved1;
} LXObject;

typedef struct __attribute__((packed)) {
	DWord pagedataoffset;
	SWord datasize;
	SWord flags;
} LXPage;

// this is correct for internal fixups only
typedef struct __attribute__((packed)) {
	unsigned char src;
	unsigned char flags;
	short srcoff;
	//unsigned char object;  // these are now variable length
	//unsigned short trgoff;
} LXFixup;


/**
 * \brief Loader for OS2 LX (DOS4GW) executable files.
 *
 * At present, this loader supports the OS2 file format (also known as the
 * Linear eXecutable format) as much as I've found necessary to inspect old
 * DOS4GW apps.  This loader could also be used for decompiling Win9x VxD
 * files or, of course, OS2 binaries, but you're probably better off making a
 * specific loader for each of these.
 */
class DOS4GWBinaryFile : public BinaryFile {
public:
	            DOS4GWBinaryFile();
	virtual    ~DOS4GWBinaryFile();

	LOADFMT     getFormat() const override { return LOADFMT_LX; }
	MACHINE     getMachine() const override { return MACHINE_PENTIUM; }
	std::list<const char *> getDependencyList() override;

	bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

private:
	int         dos4gwRead2(const short *ps) const;
	int         dos4gwRead4(const int *pi) const;
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
	ADDRESS     getAddressByName(const char *name, bool bNoTypeOK = false) override;
	std::map<ADDRESS, std::string> &getSymbols() override { return dlprocptrs; }
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	bool        isDynamicLinkedProc(ADDRESS uNative) override;
	bool        isDynamicLinkedProcPointer(ADDRESS uNative) override;
	const char *getDynamicProcName(ADDRESS uNative) override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() override;
	//DWord       getDelta();
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *handle) override;

private:
	//Header     *m_pHeader;               ///< Pointer to header.
	LXHeader   *m_pLXHeader = nullptr;   ///< Pointer to lx header.
	LXObject   *m_pLXObjects = nullptr;  ///< Pointer to lx objects.
	LXPage     *m_pLXPages = nullptr;    ///< Pointer to lx pages.
	int         m_cbImage;               ///< Size of image.
	//int         m_cReloc;                ///< Number of relocation entries.
	//DWord      *m_pRelocTable;           ///< The relocation table.
	unsigned char *base = nullptr;       ///< Beginning of the loaded image.

	/// Map from address of dynamic pointers to library procedure names.
	std::map<ADDRESS, std::string> dlprocptrs;
};

#endif
