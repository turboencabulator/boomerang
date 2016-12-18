/**
 * \file
 * \brief Contains the definition of the class PalmBinaryFile.
 *
 * \authors
 * Copyright (C) 2000-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef PALMBINARYFILE_H
#define PALMBINARYFILE_H

/*==============================================================================
 * Dependencies.
 *============================================================================*/

#include "BinaryFile.h"

class PalmBinaryFile : public BinaryFile {
public:
	                    PalmBinaryFile();
	virtual            ~PalmBinaryFile();

	virtual bool        PostLoad(void *handle);   // For archive files only
	virtual LOADFMT     getFormat() const { return LOADFMT_PALM; }
	virtual MACHINE     getMachine() const { return MACHINE_PALM; }
	virtual const char *getFilename() const { return m_pFilename; }

	virtual bool        isLibrary() const;
	virtual std::list<const char *> getDependencyList();
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

	// Get a symbol given an address
	virtual const char *getSymbolByAddress(ADDRESS dwAddr);
	// Return true if the address matches the convention for A-line system calls
	virtual bool        isDynamicLinkedProc(ADDRESS uNative);

	// Specific to BinaryFile objects that implement a "global pointer"
	// Gets a pair of unsigned integers representing the address of %agp (first)
	// and the value for GLOBALOFFSET (second)
	virtual std::pair<unsigned, unsigned> getGlobalPointerInfo();

	// Palm specific calls

	// Get the ID number for this application. It's possible that the app uses
	// this number internally, so this needs to be used in the final make
	        int         getAppID() const;

	// Generate binary files for non code and data sections
	        void        generateBinFiles(const std::string &path) const;

//
//  --  --  --  --  --  --  --  --  --  --  --
//

	// Internal information
	// Dump headers, etc
	//virtual bool        DisplayDetails(const char *fileName, FILE *f = stdout);

	// Analysis functions
	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();

	        //bool        isDynamicLinkedProc(ADDRESS wNative);
	        //ADDRESS     NativeToHostAddress(ADDRESS uNative);

protected:
	virtual bool        RealLoad(const char *sName);  // Load the file; pure virtual

private:
	        unsigned char *m_pImage;  // Points to loaded image
	        unsigned char *m_pData;   // Points to data
	// Offset from start of data to where a5 should be initialised to
	        unsigned int m_SizeBelowA5;
	        const char *m_pFilename;
};

#endif
