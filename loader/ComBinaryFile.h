/**
 * \file
 * \brief Contains the definition of the class ComBinaryFile.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef COMBINARYFILE_H
#define COMBINARYFILE_H

#include "BinaryFile.h"

/**
 * \brief Loader for COM executable files.
 */
class ComBinaryFile : public BinaryFile {
public:
	            ComBinaryFile();
	           ~ComBinaryFile() override;

	LOADFMT     getFormat() const override { return LOADFMT_COM; }
	MACHINE     getMachine() const override { return MACHINE_PENTIUM; }

	/**
	 * \name Analysis functions
	 * \{
	 */
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	/** \} */

protected:
	bool        load(std::istream &) override;

private:
	uint8_t    *m_pImage = nullptr;       ///< Pointer to image.
	//ADDRESS     m_uInitPC;                ///< Initial program counter.
	//ADDRESS     m_uInitSP;                ///< Initial stack pointer.
};

#endif
