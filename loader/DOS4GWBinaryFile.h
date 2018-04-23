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
	uint8_t sigLo;  /* .EXE signature: 0x4D 0x5A */
	uint8_t sigHi;
} Header;
#endif

typedef struct __attribute__((packed)) {
	uint8_t  sigLo;
	uint8_t  sigHi;
	uint8_t  byteord;
	uint8_t  wordord;
	uint32_t formatlvl;
	uint16_t cputype;
	uint16_t ostype;
	uint32_t modulever;
	uint32_t moduleflags;
	uint32_t modulenumpages;
	uint32_t eipobjectnum;
	uint32_t eip;
	uint32_t espobjectnum;
	uint32_t esp;
	uint32_t pagesize;
	uint32_t pageoffsetshift;
	uint32_t fixupsectionsize;
	uint32_t fixupsectionchksum;
	uint32_t loadersectionsize;
	uint32_t loadersectionchksum;
	uint32_t objtbloffset;
	uint32_t numobjsinmodule;
	uint32_t objpagetbloffset;
	uint32_t objiterpagesoffset;
	uint32_t resourcetbloffset;
	uint32_t numresourcetblentries;
	uint32_t residentnametbloffset;
	uint32_t entrytbloffset;
	uint32_t moduledirectivesoffset;
	uint32_t nummoduledirectives;
	uint32_t fixuppagetbloffset;
	uint32_t fixuprecordtbloffset;
	uint32_t importtbloffset;
	uint32_t numimportmoduleentries;
	uint32_t importproctbloffset;
	uint32_t perpagechksumoffset;
	uint32_t datapagesoffset;
	uint32_t numpreloadpages;
	uint32_t nonresnametbloffset;
	uint32_t nonresnametbllen;
	uint32_t nonresnametblchksum;
	uint32_t autodsobjectnum;
	uint32_t debuginfooffset;
	uint32_t debuginfolen;
	uint32_t numinstancepreload;
	uint32_t numinstancedemand;
	uint32_t heapsize;
} LXHeader;

typedef struct __attribute__((packed)) {
	uint32_t VirtualSize;
	uint32_t RelocBaseAddr;
	uint32_t ObjectFlags;
	uint32_t PageTblIdx;
	uint32_t NumPageTblEntries;
	uint32_t Reserved1;
} LXObject;

typedef struct __attribute__((packed)) {
	uint32_t pagedataoffset;
	uint16_t datasize;
	uint16_t flags;
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

	//bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	void        addSymbol(ADDRESS uNative, const char *pName) override;
	const char *getSymbolByAddress(ADDRESS dwAddr) override;
	ADDRESS     getAddressByName(const char *name, bool bNoTypeOK = false) const override;
	const std::map<ADDRESS, std::string> &getSymbols() const override { return dlprocptrs; }
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	bool        isDynamicLinkedProc(ADDRESS uNative) const override;
	bool        isDynamicLinkedProcPointer(ADDRESS uNative) const override;
	const char *getDynamicProcName(ADDRESS uNative) const override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	//uint32_t    getDelta();
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
	//uint32_t   *m_pRelocTable;           ///< The relocation table.
	unsigned char *base = nullptr;       ///< Beginning of the loaded image.

	/// Map from address of dynamic pointers to library procedure names.
	std::map<ADDRESS, std::string> dlprocptrs;
};

#endif
