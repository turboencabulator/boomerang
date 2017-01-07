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
#include "SymTab.h"  // For SymTab (probably unused)

#include <functional>
#include <map>
#include <string>

typedef std::map<ADDRESS, std::string, std::less<ADDRESS> > RelocMap;

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
#define SHT_NOBITS      8  ///< Bss
#define SHT_REL         9  ///< Relocation table (no addend)
#define SHT_RELA        4  ///< Relocation table (with addend, e.g. RISC)
#define SHT_SYMTAB      2  ///< Symbol table
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
	virtual            ~ElfBinaryFile();

	virtual LOADFMT     getFormat() const { return LOADFMT_ELF; }
	virtual MACHINE     getMachine() const;
	virtual std::list<const char *> getDependencyList();

	virtual bool        isLibrary() const;
	virtual ADDRESS     getImageBase() const;
	virtual size_t      getImageSize() const;

private:
	        int         elfRead2(short *ps) const;
	        int         elfRead4(int *pi) const;
	        void        elfWrite4(int *pi, int val);
	        ADDRESS     NativeToHostAddress(ADDRESS uNative);
public:
	virtual int         readNative1(ADDRESS a) const;
	virtual int         readNative2(ADDRESS a) const;
	virtual int         readNative4(ADDRESS a) const;
	virtual QWord       readNative8(ADDRESS a) const;
	virtual float       readNativeFloat4(ADDRESS a) const;
	virtual double      readNativeFloat8(ADDRESS a) const;

	/**
	 * \name Symbol table functions
	 * \{
	 */
	virtual void        addSymbol(ADDRESS uNative, const char *pName);
	        void        dumpSymbols();
	virtual const char *getSymbolByAddress(ADDRESS uAddr);
	virtual ADDRESS     getAddressByName(const char *pName, bool bNoTypeOK = false);
	virtual int         getSizeByName(const char *pName, bool bNoTypeOK = false);
	virtual const char *getFilenameSymbolFor(const char *sym);
	        int         getDistanceByName(const char *pName, const char *pSectName);
	        int         getDistanceByName(const char *pName);
	virtual ADDRESS    *getImportStubs(int &numImports);
	virtual std::vector<ADDRESS> getExportedAddresses(bool funcsOnly = true);
	//virtual std::map<ADDRESS, const char *> *getDynamicGlobalMap();
	virtual std::map<ADDRESS, std::string> &getSymbols() { return m_SymTab; }
	/** \} */

	/**
	 * \name Relocation table functions
	 * \{
	 */
	//virtual bool        isAddressRelocatable(ADDRESS uNative);
	//virtual ADDRESS     getRelocatedAddress(ADDRESS uNative);
	//virtual ADDRESS     applyRelocation(ADDRESS uNative, ADDRESS uWord);
	// Get symbol associated with relocation at address, if any
	//virtual const char *getRelocSym(ADDRESS uNative, ADDRESS *a = NULL, unsigned int *sz = NULL);
	virtual bool        isRelocationAt(ADDRESS uNative);
	/** \} */

	/**
	 * \name Analysis functions
	 * \{
	 */
	virtual bool        isDynamicLinkedProc(ADDRESS wNative);
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();
	/** \} */

protected:
	virtual bool        RealLoad(const char *sName);
	//virtual bool        PostLoad(void *handle);

private:
	// Not meant to be used externally, but sometimes you just have to have it.
	        const char *getStrPtr(int idx, int offset);

#if 0 // Cruft?
	// Similarly here; sometimes you just need to change a section's link
	// and info fields.  idx is the section index; link and info are
	// indices to other sections that will be idx's sh_link and sh_info
	// respectively.
	        void        setLinkAndInfo(int idx, int link, int info);

	// Header functions
	virtual ADDRESS     getFirstHeaderAddress();  // Get ADDRESS of main header
	        ADDRESS     getNextHeaderAddress();   // Get any other headers

	// Write an ELF object file for a given procedure
	        void        writeObjectFile(std::string &path, const char *name, void *ptxt, int txtsz, RelocMap &reloc);
	        void        writeNative4(ADDRESS nat, unsigned int n);

	        int         ProcessElfFile();         // Does most of the work
	        bool        getNextMember();          // Load next member of archive
	        void        AddRelocsAsSyms(int secIndex);
	        void        SetRelocInfo(SectionInfo *pSect);
#endif
	        void        AddSyms(int secIndex);
	        void        applyRelocations();
	        bool        ValueByName(const char *pName, SymValue *pVal, bool bNoTypeOK = false);
	        bool        SearchValueByName(const char *pName, SymValue *pVal);
	        bool        SearchValueByName(const char *pName, SymValue *pVal, const char *pSectName, const char *pStrName);
	        ADDRESS     findRelPltOffset(int i, ADDRESS addrRelPlt, int sizeRelPlt, int numRelPlt, ADDRESS addrPlt);

	        FILE       *m_fd;                       ///< File stream.
	        long        m_lImageSize;               ///< Size of image in bytes.
	        char       *m_pImage;                   ///< Pointer to the loaded image.
	        Elf32_Phdr *m_pPhdrs;                   ///< Pointer to program headers.
	        Elf32_Shdr *m_pShdrs;                   ///< Array of section header structs.
	        char       *m_pStrings;                 ///< Pointer to the string section.
	        char        m_elfEndianness;            ///< 1 = Big Endian.

	/**
	 * \brief Map from address to symbol name.
	 *
	 * Contains symbols from the various elf symbol tables, and possibly
	 * some symbols with fake addresses.
	 */
	        std::map<ADDRESS, std::string> m_SymTab;

	        SymTab      m_Reloc;                    ///< Object to store the reloc syms.
	        Elf32_Rel  *m_pReloc;                   ///< Pointer to the relocation section.
	        Elf32_Sym  *m_pSym;                     ///< Pointer to loaded symbol section.
	        bool        m_bAddend;                  ///< true if reloc table has addend.
	        ADDRESS     m_uLastAddr;                ///< Save last address looked up.
	        int         m_iLastSize;                ///< Size associated with that name.
	        ADDRESS     m_uPltMin;                  ///< Min address of PLT table.
	        ADDRESS     m_uPltMax;                  ///< Max address (1 past last) of PLT.
	        ADDRESS    *m_pImportStubs;             ///< An array of import stubs.
	        ADDRESS     m_uBaseAddr;                ///< Base image virtual address.
	        size_t      m_uImageSize;               ///< Total image size (bytes).
	        ADDRESS     first_extern;               ///< Where the first extern will be placed.
	        ADDRESS     next_extern;                ///< Where the next extern will be placed.
	        int        *m_sh_link;                  ///< Pointer to array of sh_link values.
	        int        *m_sh_info;                  ///< Pointer to array of sh_info values.
};

#endif
