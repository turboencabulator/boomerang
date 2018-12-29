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
	uint16_t int20h;           ///< Interrupt 20h
	uint16_t eof;              ///< Segment, end of allocation block
	uint8_t  res1;             ///< Reserved
	uint8_t  dosDisp[5];       ///< Far call to DOS function dispatcher
	uint8_t  int22h[4];        ///< Vector for terminate routine
	uint8_t  int23h[4];        ///< Vector for ctrl+break routine
	uint8_t  int24h[4];        ///< Vector for error routine
	uint8_t  res2[22];         ///< Reserved
	uint16_t segEnv;           ///< Segment address of environment block
	uint8_t  res3[34];         ///< Reserved
	uint8_t  int21h[6];        ///< Opcode for int21h and far return
	uint8_t  res4[6];          ///< Reserved
	uint8_t  fcb1[16];         ///< Default file control block 1
	uint8_t  fcb2[16];         ///< Default file control block 2
	uint8_t  res5[4];          ///< Reserved
	uint8_t  cmdTail[0x80];    ///< Command tail and disk transfer area
} PSP;

/**
 * \brief EXE file header
 */
typedef struct {
	uint8_t  sigLo;            ///< .EXE signature: 0x4D 0x5A
	uint8_t  sigHi;
	uint16_t lastPageSize;     ///< Size of the last page
	uint16_t numPages;         ///< Number of pages in the file
	uint16_t numReloc;         ///< Number of relocation items
	uint16_t numParaHeader;    ///< Number of paragraphs in the header
	uint16_t minAlloc;         ///< Minimum number of paragraphs
	uint16_t maxAlloc;         ///< Maximum number of paragraphs
	uint16_t initSS;           ///< Segment displacement of stack
	uint16_t initSP;           ///< Contents of SP at entry
	uint16_t checkSum;         ///< Complemented checksum
	uint16_t initIP;           ///< Contents of IP at entry
	uint16_t initCS;           ///< Segment displacement of code
	uint16_t relocTabOffset;   ///< Relocation table offset
	uint16_t overlayNum;       ///< Overlay number
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
	virtual    ~ExeBinaryFile();

	LOADFMT     getFormat() const override { return LOADFMT_EXE; }
	MACHINE     getMachine() const override { return MACHINE_PENTIUM; }

	//bool        isLibrary() const override;
	//ADDRESS     getImageBase() const override;
	//size_t      getImageSize() const override;

	/**
	 * \name Analysis functions
	 * \{
	 */
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *handle) override;

private:
	exeHeader  *m_pHeader = nullptr;      ///< Pointer to header.
	uint8_t    *m_pImage = nullptr;       ///< Pointer to image.
	uint32_t   *m_pRelocTable = nullptr;  ///< The relocation table.
	//ADDRESS     m_uInitPC;                ///< Initial program counter.
	//ADDRESS     m_uInitSP;                ///< Initial stack pointer.
};

#endif
