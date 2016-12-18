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

class IntelCoffFile : public BinaryFile {
public:
	//
	// Interface
	//
	IntelCoffFile();
	virtual ~IntelCoffFile();

	virtual bool RealLoad(const char *);
	virtual bool PostLoad(void *);

	virtual LOADFMT     getFormat() const { return LOADFMT_COFF; }
	virtual MACHINE     getMachine() const { return MACHINE_PENTIUM; }
	virtual const char *getFilename() const { return m_pFilename; }

	virtual bool        isLibrary() const;
	virtual std::list<const char *> getDependencyList();
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");

	virtual const char *getSymbolByAddress(ADDRESS uNative);
	// Lookup the name, return the address. If not found, return NO_ADDRESS
	//virtual ADDRESS     getAddressByName(const char *pName, bool bNoTypeOK = false);
	//virtual void        addSymbol(ADDRESS uNative, const char *pName) { }
	virtual bool isDynamicLinkedProc(ADDRESS uNative);
	virtual bool isRelocationAt(ADDRESS uNative);
	virtual std::map<ADDRESS, std::string> &getSymbols();

	virtual int readNative4(ADDRESS a);
	virtual int readNative2(ADDRESS a);
	virtual int readNative1(ADDRESS a);

private:
	//
	// Internal stuff
	//
	const char *m_pFilename;
	FILE *m_fd;
	std::list<SectionInfo *> m_EntryPoints;
	std::list<ADDRESS> m_Relocations;
	struct coff_header m_Header;

	SectionInfo *AddSection(SectionInfo *);
	unsigned char *getAddrPtr(ADDRESS a, ADDRESS range);
	int readNative(ADDRESS a, unsigned short n);

	SymTab m_Symbols;
};

#endif
