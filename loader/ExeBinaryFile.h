/**
 * \file
 * \brief Contains the definition of the class ExeBinaryFile, and some other
 * definitions specific to the exe version of the BinaryFile object.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef EXEBINARYFILE_H
#define EXEBINARYFILE_H

#include "BinaryFile.h"

/**
 * \brief PSP structure
 */
typedef struct {
	SWord int20h;           ///< Interrupt 20h
	SWord eof;              ///< Segment, end of allocation block
	Byte  res1;             ///< Reserved
	Byte  dosDisp[5];       ///< Far call to DOS function dispatcher
	Byte  int22h[4];        ///< Vector for terminate routine
	Byte  int23h[4];        ///< Vector for ctrl+break routine
	Byte  int24h[4];        ///< Vector for error routine
	Byte  res2[22];         ///< Reserved
	SWord segEnv;           ///< Segment address of environment block
	Byte  res3[34];         ///< Reserved
	Byte  int21h[6];        ///< Opcode for int21h and far return
	Byte  res4[6];          ///< Reserved
	Byte  fcb1[16];         ///< Default file control block 1
	Byte  fcb2[16];         ///< Default file control block 2
	Byte  res5[4];          ///< Reserved
	Byte  cmdTail[0x80];    ///< Command tail and disk transfer area
} PSP;

/**
 * \brief EXE file header
 */
typedef struct {
	Byte  sigLo;            ///< .EXE signature: 0x4D 0x5A
	Byte  sigHi;
	SWord lastPageSize;     ///< Size of the last page
	SWord numPages;         ///< Number of pages in the file
	SWord numReloc;         ///< Number of relocation items
	SWord numParaHeader;    ///< Number of paragraphs in the header
	SWord minAlloc;         ///< Minimum number of paragraphs
	SWord maxAlloc;         ///< Maximum number of paragraphs
	SWord initSS;           ///< Segment displacement of stack
	SWord initSP;           ///< Contents of SP at entry
	SWord checkSum;         ///< Complemented checksum
	SWord initIP;           ///< Contents of IP at entry
	SWord initCS;           ///< Segment displacement of code
	SWord relocTabOffset;   ///< Relocation table offset
	SWord overlayNum;       ///< Overlay number
} exeHeader;


/**
 * \brief Loader for EXE executable files.
 *
 * At present, there is no support for a symbol table.  Exe files do not use
 * dynamic linking, but it is possible that some files may have debug symbols
 * (in Microsoft Codeview or Borland formats), and these may be implemented in
 * the future.  The debug info may even be exposed as another pseudo section.
 */
class ExeBinaryFile : public BinaryFile {
public:
	                    ExeBinaryFile();
	virtual            ~ExeBinaryFile();

	virtual LOADFMT     getFormat() const { return LOADFMT_EXE; }
	virtual MACHINE     getMachine() const { return MACHINE_PENTIUM; }
	virtual const char *getFilename() const { return m_pFilename; }
	virtual std::list<const char *> getDependencyList();

	virtual bool        isLibrary() const;
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

	/**
	 * \name Symbol table functions
	 * \{
	 */
	virtual const char *getSymbolByAddress(ADDRESS a);
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	/** \} */

protected:
	virtual bool        RealLoad(const char *sName);
	//virtual bool        PostLoad(void *handle);

private:
	        exeHeader  *m_pHeader;      ///< Pointer to header.
	        Byte       *m_pImage;       ///< Pointer to image.
	        int         m_cbImage;      ///< Size of image.
	        int         m_cReloc;       ///< Number of relocation entries.
	        DWord      *m_pRelocTable;  ///< The relocation table.
	        const char *m_pFilename;
};

#endif
