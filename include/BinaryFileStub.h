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
	virtual    ~BinaryFileStub();

	LOADFMT     getFormat() const override;
	MACHINE     getMachine() const override;
	std::list<const char *> getDependencyList() override;

	bool        isLibrary() const override;
	ADDRESS     getImageBase() const override;
	size_t      getImageSize() const override;

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
};
