/**
 * \file
 * \brief Contains the definition of the class ElfBinaryFile.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef ELFBINARYFILE_H
#define ELFBINARYFILE_H

#include "BinaryFile.h"

#include <map>
#include <string>

typedef struct {
	ADDRESS uSymAddr;  // Symbol native address
	int     iSymSize;  // Size associated with symbol
} SymValue;

/**
 * \brief Internal elf info
 */
typedef struct {
	char  e_ident[4];
	char  e_class;
	char  endianness;
	char  version;
	char  osAbi;
	char  pad[8];
	short e_type;
	short e_machine;
	int   e_version;
	int   e_entry;
	int   e_phoff;
	int   e_shoff;
	int   e_flags;
	short e_ehsize;
	short e_phentsize;
	short e_phnum;
	short e_shentsize;
	short e_shnum;
	short e_shstrndx;
} Elf32_Ehdr;

/** \{ */
#define EM_SPARC           2  ///< Sun SPARC
#define EM_386             3  ///< Intel 80386 or higher
#define EM_68K             4  ///< Motorola 68000
#define EM_MIPS            8  ///< MIPS
#define EM_PA_RISC        15  ///< HP PA-RISC
#define EM_SPARC32PLUS    18  ///< Sun SPARC 32+
#define EM_PPC            20  ///< PowerPC
#define EM_X86_64         62
#define EM_ST20         0xa8  ///< ST20 (made up... there is no official value?)
/** \} */

#define ET_DYN      3  ///< Elf type (dynamic library)

#define R_386_32    1
#define R_386_PC32  2

/**
 * \brief Program header
 */
typedef struct {
	int p_type;     ///< Entry type
	int p_offset;   ///< File offset
	int p_vaddr;    ///< Virtual address
	int p_paddr;    ///< Physical address
	int p_filesz;   ///< File size
	int p_memsz;    ///< Memory size
	int p_flags;    ///< Entry flags
	int p_align;    ///< Memory/file alignment
} Elf32_Phdr;

/**
 * \brief Section header
 */
typedef struct {
	int sh_name;
	int sh_type;
	int sh_flags;
	int sh_addr;
	int sh_offset;
	int sh_size;
	int sh_link;
	int sh_info;
	int sh_addralign;
	int sh_entsize;
} Elf32_Shdr;

/** \{ */
#define SHF_WRITE       1  ///< Writeable
#define SHF_ALLOC       2  ///< Consumes memory in exe
#define SHF_EXECINSTR   4  ///< Executable
/** \} */

/** \{ */
#define SHT_SYMTAB      2  ///< Symbol table
#define SHT_STRTAB      3  ///< String table
#define SHT_RELA        4  ///< Relocation table (with addend, e.g. RISC)
#define SHT_NOBITS      8  ///< Bss
#define SHT_REL         9  ///< Relocation table (no addend)
#define SHT_DYNSYM     11  ///< Dynamic symbol table
/** \} */

typedef struct {
	int           st_name;
	unsigned      st_value;
	int           st_size;
	unsigned char st_info;
	unsigned char st_other;
	short         st_shndx;
} Elf32_Sym;

typedef struct {
	unsigned r_offset;
	int      r_info;
} Elf32_Rel;

#define ELF32_R_SYM(info)       ((info)>>8)
#define ELF32_ST_BIND(i)        ((i) >> 4)
#define ELF32_ST_TYPE(i)        ((i) & 0xf)
#define ELF32_ST_INFO(b, t)     (((b)<<4)+((t)&0xf))

/**
 * \name Symbol table type
 * \{
 */
#define STT_NOTYPE  0  ///< None
#define STT_FUNC    2  ///< Function
#define STT_SECTION 3
#define STT_FILE    4
/** \} */

/** \{ */
#define STB_GLOBAL  1
#define STB_WEAK    2
/** \} */

typedef struct {
	short d_tag;  /* how to interpret value */
	union {
		int d_val;
		int d_ptr;
		int d_off;
	} d_un;
} Elf32_Dyn;

/**
 * \name Tag values
 * \{
 */
#define DT_NULL     0  ///< Last entry in list
#define DT_STRTAB   5  ///< String table
#define DT_NEEDED   1  ///< A needed link-type object
/** \} */

#define E_REL       1  ///< Relocatable file type


/**
 * \brief Loader for ELF executable files.
 */
class ElfBinaryFile : public BinaryFile {
public:
	            ElfBinaryFile(bool bArchive = false);
	virtual    ~ElfBinaryFile();

	LOADFMT     getFormat() const override { return LOADFMT_ELF; }
	MACHINE     getMachine() const override;
	//std::list<const char *> getDependencyList() const override;

	//bool        isLibrary() const override;
	//ADDRESS     getImageBase() const override;
	//size_t      getImageSize() const override;

private:
	int         elfRead2(const short *ps) const;
	int         elfRead4(const int *pi) const;
	void        elfWrite4(int *pi, int val);
	//const char *NativeToHostAddress(ADDRESS uNative) const;
public:

	/**
	 * \name Symbol table functions
	 * \{
	 */
	void        addSymbol(ADDRESS, const std::string &) override;
	//void        dumpSymbols() const;
	const char *getSymbolByAddress(ADDRESS uAddr) override;
	ADDRESS     getAddressByName(const std::string &, bool = false) const override;
	int         getSizeByName(const std::string &, bool = false) const override;
	const char *getFilenameSymbolFor(const std::string &) override;
	//int         getDistanceByName(const std::string &, const std::string &);
	//int         getDistanceByName(const std::string &);
	//ADDRESS    *getImportStubs(int &numImports) override;
	//std::vector<ADDRESS> getExportedAddresses(bool funcsOnly = true) override;
	//std::map<ADDRESS, const char *> *getDynamicGlobalMap() override;
	const std::map<ADDRESS, std::string> &getSymbols() const override { return m_SymTab; }
	/** \} */

	/**
	 * \name Relocation table functions
	 * \{
	 */
	//bool        isAddressRelocatable(ADDRESS uNative) override;
	//ADDRESS     getRelocatedAddress(ADDRESS uNative) override;
	//ADDRESS     applyRelocation(ADDRESS uNative, ADDRESS uWord) override;
	// Get symbol associated with relocation at address, if any
	//const char *getRelocSym(ADDRESS uNative, ADDRESS *a = nullptr, unsigned int *sz = nullptr) override;
	bool        isRelocationAt(ADDRESS uNative) const override;
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	bool        isDynamicLinkedProc(ADDRESS wNative) const override;
	ADDRESS     getMainEntryPoint() override;
	ADDRESS     getEntryPoint() const override;
	/** \} */

protected:
	bool        load(std::istream &) override;
	//bool        PostLoad(void *handle) override;

private:
	// Not meant to be used externally, but sometimes you just have to have it.
	const char *getStrPtr(int, int) const;
	const char *getStrPtr(const SectionInfo *, int) const;

#if 0 // Cruft?
	// Similarly here; sometimes you just need to change a section's link
	// and info fields.  idx is the section index; link and info are
	// indices to other sections that will be idx's sh_link and sh_info
	// respectively.
	void        setLinkAndInfo(int idx, int link, int info);

	// Header functions
	ADDRESS     getFirstHeaderAddress();  // Get ADDRESS of main header
	ADDRESS     getNextHeaderAddress();   // Get any other headers

	// Write an ELF object file for a given procedure
	void        writeObjectFile(std::string &path, const char *name, void *ptxt, int txtsz, std::map<ADDRESS, std::string> &reloc);
	void        writeNative4(ADDRESS nat, unsigned int n);

	int         ProcessElfFile();         // Does most of the work
	bool        getNextMember();          // Load next member of archive
	void        AddRelocsAsSyms(size_t);
	void        SetRelocInfo(SectionInfo *pSect);
#endif
	void        AddSyms(size_t);
	void        applyRelocations();
	bool        ValueByName(const std::string &, SymValue *, bool = false) const;
	bool        SearchValueByName(const std::string &, SymValue *) const;
	bool        SearchValueByName(const std::string &, SymValue *, const std::string &, const std::string &) const;
	ADDRESS     findRelPltOffset(int i, const char *addrRelPlt, int sizeRelPlt, int numRelPlt, ADDRESS addrPlt) const;

	char       *m_pImage = nullptr;         ///< Pointer to the loaded image.
	const Elf32_Phdr *m_pPhdrs = nullptr;   ///< Pointer to program headers.
	const Elf32_Shdr *m_pShdrs = nullptr;   ///< Array of section header structs.
	const char *m_pStrings = nullptr;       ///< Pointer to the string section.

	/**
	 * \brief Map from address to symbol name.
	 *
	 * Contains symbols from the various elf symbol tables, and possibly
	 * some symbols with fake addresses.
	 */
	std::map<ADDRESS, std::string> m_SymTab;

	const Elf32_Rel *m_pReloc = nullptr;    ///< Pointer to the relocation section.
	const Elf32_Sym *m_pSym = nullptr;      ///< Pointer to loaded symbol section.
	bool        m_bAddend;                  ///< true if reloc table has addend.
	ADDRESS     m_uPltMin = 0;              ///< Min address of PLT table.
	ADDRESS     m_uPltMax = 0;              ///< Max address (1 past last) of PLT.
	ADDRESS    *m_pImportStubs = nullptr;   ///< An array of import stubs.
	//ADDRESS     m_uBaseAddr;                ///< Base image virtual address.
	//size_t      m_uImageSize;               ///< Total image size (bytes).
	ADDRESS     first_extern;               ///< Where the first extern will be placed.
	ADDRESS     next_extern = 0;            ///< Where the next extern will be placed.
	int        *m_sh_link = nullptr;        ///< Pointer to array of sh_link values.
	int        *m_sh_info = nullptr;        ///< Pointer to array of sh_info values.
};

#endif
