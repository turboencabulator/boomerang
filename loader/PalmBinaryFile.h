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

#include "BinaryFile.h"

#include <string>

/**
 * \brief Loader for Palm Pilot .prc files.
 */
class PalmBinaryFile : public BinaryFile {
public:
	                    PalmBinaryFile();
	virtual            ~PalmBinaryFile();

	virtual LOADFMT     getFormat() const { return LOADFMT_PALM; }
	virtual MACHINE     getMachine() const { return MACHINE_PALM; }
	virtual std::list<const char *> getDependencyList();

	virtual bool        isLibrary() const;
	virtual ADDRESS     getImageBase() const;
	virtual size_t      getImageSize() const;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	virtual const char *getSymbolByAddress(ADDRESS dwAddr);
	/** \} */

	//virtual std::pair<unsigned, unsigned> getGlobalPointerInfo();

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual bool        isDynamicLinkedProc(ADDRESS uNative);
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	/** \} */

protected:
	virtual bool        load(std::istream &);
	//virtual bool        PostLoad(void *handle);

private:
	        int         getAppID() const;
	        void        generateBinFiles(const std::string &path) const;

	        unsigned char *m_pImage = NULL;  ///< Points to loaded image.
	        unsigned char *m_pData = NULL;   ///< Points to data.
	        unsigned int m_SizeBelowA5;      ///< Offset from start of data to where a5 should be initialised to.
};

#endif
