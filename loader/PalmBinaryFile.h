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
	virtual    ~PalmBinaryFile();

	LOADFMT     getFormat() const override { return LOADFMT_PALM; }
	MACHINE     getMachine() const override { return MACHINE_PALM; }

	//bool        isLibrary() const override;
	//ADDRESS     getImageBase() const override;
	//size_t      getImageSize() const override;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	const char *getSymbolByAddress(ADDRESS dwAddr) override;
	/** \} */

	//std::pair<unsigned, unsigned> getGlobalPointerInfo() override;

	/**
	 * \name Analysis functions
	 * \{
	 */
	bool        isDynamicLinkedProc(ADDRESS uNative) const override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *handle) override;

private:
	int         getAppID() const;
	void        generateBinFiles(const std::string &path) const;

	unsigned char *m_pImage = nullptr;  ///< Points to loaded image.
	unsigned char *m_pData = nullptr;   ///< Points to data.
	unsigned int m_SizeBelowA5;         ///< Offset from start of data to where a5 should be initialised to.
};

#endif
