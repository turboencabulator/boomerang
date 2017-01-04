/**
 * \file
 * \ingroup UnitTestStub
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "BinaryFile.h"

class BinaryFileStub : public BinaryFile {
public:
	                    BinaryFileStub();
	virtual            ~BinaryFileStub();

	virtual LOADFMT     getFormat() const;
	virtual MACHINE     getMachine() const;
	virtual std::list<const char *> getDependencyList();

	virtual bool        isLibrary() const;
	virtual ADDRESS     getImageBase() const;
	virtual size_t      getImageSize() const;

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	/** \} */

protected:
	virtual bool        RealLoad(const char *sName);
	//virtual bool        PostLoad(void *handle);
};
