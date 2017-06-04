/**
 * \file
 * \brief Contains the implementation of the class MachOBinaryFile.
 *
 * \authors
 * Copyright (C) 2000, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MachOBinaryFile.h"


#if 0
#include <mach/machine.h>
#else
typedef uint32_t cpu_type_t;
typedef uint32_t cpu_subtype_t;
#endif // Needed before mach-o/loader.h

#if 0
#include <mach/vm_prot.h>
#else
typedef uint32_t vm_prot_t;
#define VM_PROT_NONE    ((vm_prot_t) 0x00)
#define VM_PROT_READ    ((vm_prot_t) 0x01)      /* read permission */
#define VM_PROT_WRITE   ((vm_prot_t) 0x02)      /* write permission */
#define VM_PROT_EXECUTE ((vm_prot_t) 0x04)      /* execute permission */
#endif // Needed before mach-o/loader.h

#include "mach-o/loader.h"
#include "mach-o/nlist.h"

#if 0
#include <objc/objc-class.h>
#include <objc/objc-runtime.h>
#else
typedef struct objc_class *Class;

typedef struct objc_object {
	Class isa;
} *id;

typedef struct objc_selector *SEL;
typedef id (*IMP)(id, SEL, ...);

struct objc_class {
	struct objc_class *isa;
	struct objc_class *super_class;
	const char *name;
	long version;
	long info;
	long instance_size;
	struct objc_ivar_list *ivars;

	struct objc_method_list **methodLists;

	struct objc_cache *cache;
	struct objc_protocol_list *protocols;
};

struct objc_ivar {
	char *ivar_name;
	char *ivar_type;
	int ivar_offset;
#ifdef __alpha__
	int space;
#endif
};

struct objc_ivar_list {
	int ivar_count;
#ifdef __alpha__
	int space;
#endif
	struct objc_ivar ivar_list[1];  /* variable length structure */
};

struct objc_method {
	SEL method_name;
	char *method_types;
	IMP method_imp;
};

struct objc_method_list {
	struct objc_method_list *obsolete;

	int method_count;
#ifdef __alpha__
	int space;
#endif
	struct objc_method method_list[1];  /* variable length structure */
};

typedef struct objc_symtab *Symtab;
struct objc_symtab {
	unsigned long sel_ref_cnt;
	SEL *refs;
	unsigned short cls_def_cnt;
	unsigned short cat_def_cnt;
	void *defs[1];  /* variable size */
};

struct objc_module {
	unsigned long version;
	unsigned long size;
	const char *name;
	Symtab symtab;
};
#endif

#include <cassert>
#include <cstdio>
#include <cstring>

//#define DEBUG_MACHO_LOADER
//#define DEBUG_MACHO_LOADER_OBJC

MachOBinaryFile::MachOBinaryFile() :
	base(NULL),
	machine(MACHINE_PPC),
	swap_bytes(false)
{
}

MachOBinaryFile::~MachOBinaryFile()
{
	for (int i = 0; i < m_iNumSections; i++)
		delete [] m_pSections[i].pSectionName;
	delete [] m_pSections;
	delete [] base;
}

ADDRESS MachOBinaryFile::getEntryPoint()
{
	return entrypoint;
}

ADDRESS MachOBinaryFile::getMainEntryPoint()
{
	ADDRESS aMain = getAddressByName("main", true);
	if (aMain != NO_ADDRESS)
		return aMain;
	aMain = getAddressByName("_main", true);
	if (aMain != NO_ADDRESS)
		return aMain;

	return NO_ADDRESS;
}

bool MachOBinaryFile::load(std::istream &ifs)
{
	struct mach_header header;
	ifs.read((char *)&header, sizeof header);

	// check for swapped bytes
	if (header.magic == MH_MAGIC) {
		swap_bytes = false;
	} else if (header.magic == MH_CIGAM) {
		swap_bytes = true;
	} else {
		fprintf(stderr, "error loading file %s, bad Mach-O magic\n", getFilename());
		return false;
	}

	header.magic      = BMMH(header.magic);
	header.cputype    = BMMH(header.cputype);
	header.cpusubtype = BMMH(header.cpusubtype);
	header.filetype   = BMMH(header.filetype);
	header.ncmds      = BMMH(header.ncmds);
	header.sizeofcmds = BMMH(header.sizeofcmds);
	header.flags      = BMMH(header.flags);

	// Determine CPU type
	if (header.cputype == 0x07)
		machine = MACHINE_PENTIUM;
	else
		machine = MACHINE_PPC;

	std::vector<struct segment_command> segments;
	std::vector<struct nlist> symbols;
	std::vector<struct section> stubs_sects;
	char *strtbl = NULL;
	unsigned *indirectsymtbl = NULL;
	ADDRESS objc_symbols = NO_ADDRESS, objc_modules = NO_ADDRESS, objc_strings = NO_ADDRESS, objc_refs = NO_ADDRESS;
	unsigned objc_modules_size = 0;

	for (unsigned i = 0; i < header.ncmds; i++) {
		std::streamsize pos = ifs.tellg();

		struct load_command cmd;
		ifs.read((char *)&cmd, sizeof cmd);
		cmd.cmd     = BMMH(cmd.cmd);
		cmd.cmdsize = BMMH(cmd.cmdsize);

		ifs.seekg(pos);
		switch (cmd.cmd) {
		case LC_SEGMENT:
			{
				struct segment_command seg;
				ifs.read((char *)&seg, sizeof seg);
				seg.cmd      = BMMH(seg.cmd);
				seg.cmdsize  = BMMH(seg.cmdsize);
				seg.vmaddr   = BMMH(seg.vmaddr);
				seg.vmsize   = BMMH(seg.vmsize);
				seg.fileoff  = BMMH(seg.fileoff);
				seg.filesize = BMMH(seg.filesize);
				seg.maxprot  = BMMH(seg.maxprot);
				seg.initprot = BMMH(seg.initprot);
				seg.nsects   = BMMH(seg.nsects);
				seg.flags    = BMMH(seg.flags);
				segments.push_back(seg);

#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "seg addr %x size %i fileoff %x filesize %i flags %x\n", seg.vmaddr, seg.vmsize, seg.fileoff, seg.filesize, seg.flags);
#endif
				for (unsigned n = 0; n < seg.nsects; n++) {
					struct section sect;
					ifs.read((char *)&sect, sizeof sect);
					sect.addr      = BMMH(sect.addr);
					sect.size      = BMMH(sect.size);
					sect.offset    = BMMH(sect.offset);
					sect.align     = BMMH(sect.align);
					sect.reloff    = BMMH(sect.reloff);
					sect.nreloc    = BMMH(sect.nreloc);
					sect.flags     = BMMH(sect.flags);
					sect.reserved1 = BMMH(sect.reserved1);
					sect.reserved2 = BMMH(sect.reserved2);

#ifdef DEBUG_MACHO_LOADER
					fprintf(stdout, "    sectname %s segname %s addr %x size %i flags %x\n", sect.sectname, sect.segname, sect.addr, sect.size, sect.flags);
#endif
					if ((sect.flags & SECTION_TYPE) == S_SYMBOL_STUBS) {
						stubs_sects.push_back(sect);
#ifdef DEBUG_MACHO_LOADER
						fprintf(stdout, "        symbol stubs section, start index %i, stub size %i\n", sect.reserved1, sect.reserved2);
#endif
					}
					if (!strcmp(sect.sectname, SECT_OBJC_SYMBOLS)) {
						assert(objc_symbols == NO_ADDRESS);
						objc_symbols = sect.addr;
					}
					if (!strcmp(sect.sectname, SECT_OBJC_MODULES)) {
						assert(objc_modules == NO_ADDRESS);
						objc_modules = sect.addr;
						objc_modules_size = sect.size;
					}
					if (!strcmp(sect.sectname, SECT_OBJC_STRINGS)) {
						assert(objc_strings == NO_ADDRESS);
						objc_strings = sect.addr;
					}
					if (!strcmp(sect.sectname, SECT_OBJC_REFS)) {
						assert(objc_refs == NO_ADDRESS);
						objc_refs = sect.addr;
					}
				}
			}
			break;

		case LC_SYMTAB:
			{
				struct symtab_command syms;
				ifs.read((char *)&syms, sizeof syms);
				syms.cmd     = BMMH(syms.cmd);
				syms.cmdsize = BMMH(syms.cmdsize);
				syms.symoff  = BMMH(syms.symoff);
				syms.nsyms   = BMMH(syms.nsyms);
				syms.stroff  = BMMH(syms.stroff);
				syms.strsize = BMMH(syms.strsize);

				ifs.seekg(syms.stroff);
				strtbl = new char[syms.strsize];
				ifs.read(strtbl, syms.strsize);

				ifs.seekg(syms.symoff);
				for (unsigned n = 0; n < syms.nsyms; n++) {
					struct nlist sym;
					ifs.read((char *)&sym, sizeof sym);
					sym.n_un.n_strx = BMMH(sym.n_un.n_strx);
					//sym.n_type      = BMMH(sym.n_type);
					//sym.n_sect      = BMMH(sym.n_sect);
					//sym.n_desc      = BMMH(sym.n_desc);
					sym.n_value     = BMMH(sym.n_value);
					symbols.push_back(sym);

#ifdef DEBUG_MACHO_LOADER
					//fprintf(stdout, "got sym %s flags %x value %x\n", strtbl + sym.n_un.n_strx, sym.n_type, sym.n_value);
#endif
				}
#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "symtab contains %i symbols\n", syms.nsyms);
#endif
			}
			break;

		case LC_DYSYMTAB:
			{
				struct dysymtab_command syms;
				ifs.read((char *)&syms, sizeof syms);
				syms.cmd            = BMMH(syms.cmd);
				syms.cmdsize        = BMMH(syms.cmdsize);
				syms.ilocalsym      = BMMH(syms.ilocalsym);
				syms.nlocalsym      = BMMH(syms.nlocalsym);
				syms.iextdefsym     = BMMH(syms.iextdefsym);
				syms.nextdefsym     = BMMH(syms.nextdefsym);
				syms.iundefsym      = BMMH(syms.iundefsym);
				syms.nundefsym      = BMMH(syms.nundefsym);
				syms.tocoff         = BMMH(syms.tocoff);
				syms.ntoc           = BMMH(syms.ntoc);
				syms.modtaboff      = BMMH(syms.modtaboff);
				syms.nmodtab        = BMMH(syms.nmodtab);
				syms.extrefsymoff   = BMMH(syms.extrefsymoff);
				syms.nextrefsyms    = BMMH(syms.nextrefsyms);
				syms.indirectsymoff = BMMH(syms.indirectsymoff);
				syms.nindirectsyms  = BMMH(syms.nindirectsyms);
				syms.extreloff      = BMMH(syms.extreloff);
				syms.nextrel        = BMMH(syms.nextrel);
				syms.locreloff      = BMMH(syms.locreloff);
				syms.nlocrel        = BMMH(syms.nlocrel);

#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "dysymtab local %i %i defext %i %i undef %i %i\n",
				        syms.ilocalsym, syms.nlocalsym,
				        syms.iextdefsym, syms.nextdefsym,
				        syms.iundefsym, syms.nundefsym);
				fprintf(stdout, "dysymtab has %i indirect symbols: ", syms.nindirectsyms);
#endif
				ifs.seekg(syms.indirectsymoff);
				indirectsymtbl = new unsigned[syms.nindirectsyms];
				ifs.read((char *)indirectsymtbl, sizeof *indirectsymtbl * syms.nindirectsyms);
				for (unsigned j = 0; j < syms.nindirectsyms; j++) {
					indirectsymtbl[j] = BMMH(indirectsymtbl[j]);
				}
#ifdef DEBUG_MACHO_LOADER
				for (unsigned j = 0; j < syms.nindirectsyms; j++) {
					fprintf(stdout, "%i ", indirectsymtbl[j]);
				}
				fprintf(stdout, "\n");
#endif
			}
			break;

		default:
#ifdef DEBUG_MACHO_LOADER
			fprintf(stderr, "not handled load command %x\n", cmd.cmd);
#endif
			// yep, there's lots of em
			break;
		}

		ifs.seekg(pos + cmd.cmdsize);
	}

	struct segment_command *lowest = &segments[0], *highest = &segments[0];
	for (unsigned i = 1; i < segments.size(); i++) {
		if (segments[i].vmaddr < lowest->vmaddr)
			lowest = &segments[i];
		if (segments[i].vmaddr > highest->vmaddr)
			highest = &segments[i];
	}

	loaded_addr = lowest->vmaddr;
	loaded_size = highest->vmaddr - lowest->vmaddr + highest->vmsize;

	base = new char[loaded_size];

	m_iNumSections = segments.size();
	m_pSections = new SectionInfo[m_iNumSections];

	for (unsigned i = 0; i < segments.size(); i++) {
		ifs.seekg(segments[i].fileoff);
		ADDRESS a = segments[i].vmaddr;
		unsigned sz = segments[i].vmsize;
		unsigned fsz = segments[i].filesize;
		memset(&base[a - loaded_addr], 0, sz);
		ifs.read(&base[a - loaded_addr], fsz);
#ifdef DEBUG_MACHO_LOADER
		fprintf(stderr, "loaded segment %x %i in mem %i in file\n", a, sz, fsz);
#endif

		char *name = new char[17];
		strncpy(name, segments[i].segname, 16);
		name[16] = '\0';
		m_pSections[i].pSectionName = name;
		m_pSections[i].uNativeAddr = a;
		m_pSections[i].uHostAddr = &base[a - loaded_addr];
		m_pSections[i].uSectionSize = sz;

		unsigned long l = segments[i].initprot;
		m_pSections[i].bBss      = false; // TODO
		m_pSections[i].bCode     =  (l & VM_PROT_EXECUTE) != 0;
		m_pSections[i].bData     =  (l & VM_PROT_READ)    != 0;
		m_pSections[i].bReadOnly = ~(l & VM_PROT_WRITE)   == 0;  // FIXME: This is always false.
	}

	// process stubs_sects
	for (unsigned j = 0; j < stubs_sects.size(); j++) {
		unsigned startidx = stubs_sects[j].reserved1;
		for (unsigned i = 0; i < stubs_sects[j].size / stubs_sects[j].reserved2; i++) {
			unsigned symbol = indirectsymtbl[startidx + i];
			ADDRESS addr = stubs_sects[j].addr + i * stubs_sects[j].reserved2;
			const char *name = strtbl + symbols[symbol].n_un.n_strx;
#ifdef DEBUG_MACHO_LOADER
			fprintf(stdout, "stub for %s at %x\n", name, addr);
#endif
			if (*name == '_')  // we want printf not _printf
				name++;
			m_SymA[addr] = name;
			dlprocs[addr] = name;
		}
	}

	// process the remaining symbols
	for (unsigned i = 0; i < symbols.size(); i++) {
		const char *name = strtbl + symbols[i].n_un.n_strx;
		if (symbols[i].n_un.n_strx != 0 && symbols[i].n_value != 0 && *name != 0) {

#ifdef DEBUG_MACHO_LOADER
			fprintf(stdout, "symbol %s at %x type %x\n", name,
			        symbols[i].n_value,
			        symbols[i].n_type & N_TYPE);
#endif
			if (*name == '_')  // we want main not _main
				name++;
			m_SymA[symbols[i].n_value] = name;
		}
	}

	// process objective-c section
	if (objc_modules != NO_ADDRESS) {
#ifdef DEBUG_MACHO_LOADER_OBJC
		fprintf(stdout, "processing objective-c section\n");
#endif
		for (unsigned i = 0; i < objc_modules_size;) {
			struct objc_module *module = (struct objc_module *)&base[objc_modules - loaded_addr];
			const char *name = (const char *)&base[BMMH(module->name) - loaded_addr];
			Symtab symtab = (Symtab)&base[BMMH(module->symtab) - loaded_addr];
#ifdef DEBUG_MACHO_LOADER_OBJC
			fprintf(stdout, "module %s (%i classes)\n", name, BMMHW(symtab->cls_def_cnt));
#endif
			ObjcModule *m = &modules[name];
			m->name = name;
			for (unsigned j = 0; j < BMMHW(symtab->cls_def_cnt); j++) {
				struct objc_class *def = (struct objc_class *)&base[BMMH(symtab->defs[j]) - loaded_addr];
				const char *name = (const char *)&base[BMMH(def->name) - loaded_addr];
#ifdef DEBUG_MACHO_LOADER_OBJC
				fprintf(stdout, "  class %s\n", name);
#endif
				ObjcClass *cl = &m->classes[name];
				cl->name = name;
				struct objc_ivar_list *ivars = (struct objc_ivar_list *)&base[BMMH(def->ivars) - loaded_addr];
				for (unsigned k = 0; k < BMMH(ivars->ivar_count); k++) {
					struct objc_ivar *ivar = &ivars->ivar_list[k];
					const char *name = (const char *)&base[BMMH(ivar->ivar_name) - loaded_addr];
					const char *types = (const char *)&base[BMMH(ivar->ivar_type) - loaded_addr];
#ifdef DEBUG_MACHO_LOADER_OBJC
					fprintf(stdout, "    ivar %s %s %x\n", name, types, BMMH(ivar->ivar_offset));
#endif
					ObjcIvar *iv = &cl->ivars[name];
					iv->name = name;
					iv->type = types;
					iv->offset = BMMH(ivar->ivar_offset);
				}
				// this is weird, why is it defined as a ** in the struct but used as a * in otool?
				struct objc_method_list *methods = (struct objc_method_list *)&base[BMMH(def->methodLists) - loaded_addr];
				for (unsigned k = 0; k < BMMH(methods->method_count); k++) {
					struct objc_method *method = &methods->method_list[k];
					const char *name = (const char *)&base[BMMH(method->method_name) - loaded_addr];
					const char *types = (const char *)&base[BMMH(method->method_types) - loaded_addr];
#ifdef DEBUG_MACHO_LOADER_OBJC
					fprintf(stdout, "    method %s %s %x\n", name, types, BMMH((void *)method->method_imp));
#endif
					ObjcMethod *me = &cl->methods[name];
					me->name = name;
					me->types = types;
					me->addr = BMMH((void *)method->method_imp);
				}
			}
			objc_modules += BMMH(module->size);
			i += BMMH(module->size);
		}
	}

	// Give the entry point a symbol
	//ADDRESS entry = getMainEntryPoint();
	entrypoint = getMainEntryPoint();

	return true;
}

#if 0 // Cruft?
bool MachOBinaryFile::PostLoad(void *handle)
{
	return false;
}
#endif

const char *MachOBinaryFile::getSymbolByAddress(ADDRESS dwAddr) {
	std::map<ADDRESS, std::string>::iterator it = m_SymA.find(dwAddr);
	if (it == m_SymA.end())
		return 0;
	return it->second.c_str();
}

ADDRESS MachOBinaryFile::getAddressByName(const char *pName, bool bNoTypeOK /* = false */)
{
	// This is "looking up the wrong way" and hopefully is uncommon
	// Use linear search
	std::map<ADDRESS, std::string>::iterator it = m_SymA.begin();
	while (it != m_SymA.end()) {
		// std::cerr << "Symbol: " << it->second.c_str() << " at 0x" << std::hex << it->first << "\n";
		if (it->second == pName)
			return it->first;
		it++;
	}
	return NO_ADDRESS;
}

void MachOBinaryFile::addSymbol(ADDRESS uNative, const char *pName)
{
	m_SymA[uNative] = pName;
}

/**
 * \brief Read 2 bytes from native addr.
 */
int MachOBinaryFile::machORead2(const short *ps) const
{
	const unsigned char *p = (const unsigned char *)ps;
	// Big endian
	int n = (int)(p[1] + (p[0] << 8));
	return n;
}

/**
 * \brief Read 4 bytes from native addr.
 */
int MachOBinaryFile::machORead4(const int *pi) const
{
	const short *p = (const short *)pi;
	int n1 = machORead2(p);
	int n2 = machORead2(p + 1);
	int n = (int)(n2 | (n1 << 16));
	return n;
}

unsigned int MachOBinaryFile::BMMH(const void *x)
{
	if (swap_bytes) return (unsigned int)_BMMH(x);
	else return (unsigned int)x;
}

uint32_t MachOBinaryFile::BMMH(uint32_t x)
{
	if (swap_bytes) return _BMMH(x);
	else return x;
}

unsigned short MachOBinaryFile::BMMHW(unsigned short x)
{
	if (swap_bytes) return _BMMHW(x);
	else return x;
}

int MachOBinaryFile::readNative1(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) si = getSectionInfo(0);
	return si->uHostAddr[nat - si->uNativeAddr];
}

int MachOBinaryFile::readNative2(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return 0;
	return machORead2((const short *)&si->uHostAddr[nat - si->uNativeAddr]);
}

int MachOBinaryFile::readNative4(ADDRESS nat) const
{
	const SectionInfo *si = getSectionInfoByAddr(nat);
	if (!si) return 0;
	return machORead4((const int *)&si->uHostAddr[nat - si->uNativeAddr]);
}

QWord MachOBinaryFile::readNative8(ADDRESS nat) const
{
	int raw[2];
#ifdef WORDS_BIGENDIAN  // This tests the host machine
	// Source and host are different endianness
	raw[1] = readNative4(nat);
	raw[0] = readNative4(nat + 4);
#else
	// Source and host are same endianness
	raw[0] = readNative4(nat);
	raw[1] = readNative4(nat + 4);
#endif
	return *(QWord *)raw;
}

float MachOBinaryFile::readNativeFloat4(ADDRESS nat) const
{
	int raw = readNative4(nat);
	// Ugh! gcc says that reinterpreting from int to float is invalid!!
	//return reinterpret_cast<float>(raw);  // Note: cast, not convert!!
	return *(float *)&raw;  // Note: cast, not convert
}

double MachOBinaryFile::readNativeFloat8(ADDRESS nat) const
{
	int raw[2];
#ifdef WORDS_BIGENDIAN  // This tests the host machine
	// Source and host are different endianness
	raw[1] = readNative4(nat);
	raw[0] = readNative4(nat + 4);
#else
	// Source and host are same endianness
	raw[0] = readNative4(nat);
	raw[1] = readNative4(nat + 4);
#endif
	//return reinterpret_cast<double>(*raw);  // Note: cast, not convert!!
	return *(double *)raw;
}

const char *MachOBinaryFile::getDynamicProcName(ADDRESS uNative)
{
	return dlprocs[uNative].c_str();
}

bool MachOBinaryFile::isLibrary() const
{
	return false;
}

ADDRESS MachOBinaryFile::getImageBase() const
{
	return loaded_addr;
}

size_t MachOBinaryFile::getImageSize() const
{
	return loaded_size;
}

std::list<const char *> MachOBinaryFile::getDependencyList()
{
	return std::list<const char *>(); /* FIXME */
}

#if 0 // Cruft?
DWord MachOBinaryFile::getDelta()
{
	// Stupid function anyway: delta depends on section
	// This should work for the header only
	//return (DWord)base - LMMH(m_pPEHeader->Imagebase);
	return (DWord)base - (DWord)loaded_addr;
}
#endif

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new BinaryFile
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" BinaryFile *construct()
{
	return new MachOBinaryFile();
}
extern "C" void destruct(BinaryFile *bf)
{
	delete (MachOBinaryFile *)bf;
}
#endif
