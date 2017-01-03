/**
 * \file
 * \brief Contains the definition of the class IntelCoffFile.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef INTELCOFFFILE_H
#define INTELCOFFFILE_H

#include "BinaryFile.h"
#include "SymTab.h"

#include <stdint.h>

#define PACKED __attribute__((packed))

struct PACKED coff_header {
	uint16_t coff_magic;
	uint16_t coff_sections;
	uint32_t coff_timestamp;
	uint32_t coff_symtab_ofs;
	uint32_t coff_num_syment;
	uint16_t coff_opthead_size;
	uint16_t coff_flags;
};


/**
 * \brief Loader for COFF executable files.
 */
class IntelCoffFile : public BinaryFile {
public:
	                    IntelCoffFile();
	virtual            ~IntelCoffFile();

	virtual LOADFMT     getFormat() const { return LOADFMT_COFF; }
	virtual MACHINE     getMachine() const { return MACHINE_PENTIUM; }
	virtual const char *getFilename() const { return m_pFilename; }
	virtual std::list<const char *> getDependencyList();

	virtual bool        isLibrary() const;
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

private:
	        int         readNative(ADDRESS a, unsigned short n);
public:
	virtual int         readNative1(ADDRESS a);
	virtual int         readNative2(ADDRESS a);
	virtual int         readNative4(ADDRESS a);

	/**
	 * \name Symbol table functions
	 * \{
	 */
	virtual const char *getSymbolByAddress(ADDRESS uNative);
	virtual std::map<ADDRESS, std::string> &getSymbols();
	/** \} */

	/**
	 * \name Relocation table functions
	 * \{
	 */
	virtual bool        isRelocationAt(ADDRESS uNative);
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual bool        isDynamicLinkedProc(ADDRESS uNative);
	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	/** \} */

protected:
	virtual bool        RealLoad(const char *);
	//virtual bool        PostLoad(void *);

private:
	const char *m_pFilename;
	FILE *m_fd;
	std::list<SectionInfo *> m_EntryPoints;
	std::list<ADDRESS> m_Relocations;
	struct coff_header m_Header;

	SectionInfo *AddSection(SectionInfo *);
	unsigned char *getAddrPtr(ADDRESS a, ADDRESS range);

	SymTab m_Symbols;
};

#endif
