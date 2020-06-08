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
#include <objc/runtime.h>
#elif 0  // Structure/member sizes become host-width, not native-width
typedef struct objc_class *Class;
struct objc_class {
	Class isa;
	Class super_class;
	const char *name;
	long version;
	long info;
	long instance_size;
	struct objc_ivar_list *ivars;
	struct objc_method_list **methodLists;
	struct objc_cache *cache;
	struct objc_protocol_list *protocols;
};

struct objc_object {
	Class isa;
};

typedef struct objc_object *id;
typedef struct objc_selector *SEL;
typedef id (*IMP)(id, SEL, ...);

typedef struct objc_ivar *Ivar;
struct objc_ivar {
	char *ivar_name;
	char *ivar_type;
	int ivar_offset;
#ifdef __LP64__
	int space;
#endif
};

struct objc_ivar_list {
	int ivar_count;
#ifdef __LP64__
	int space;
#endif
	/* variable length structure */
	struct objc_ivar ivar_list[1];
};

typedef struct objc_method *Method;
struct objc_method {
	SEL method_name;
	char *method_types;
	IMP method_imp;
};

struct objc_method_list {
	struct objc_method_list *obsolete;
	int method_count;
#ifdef __LP64__
	int space;
#endif
	/* variable length structure */
	struct objc_method method_list[1];
};

typedef struct objc_symtab *Symtab;
struct objc_symtab {
	unsigned long sel_ref_cnt;
	SEL *refs;
	unsigned short cls_def_cnt;
	unsigned short cat_def_cnt;
	void *defs[1];  /* variable size */
};

typedef struct objc_module *Module;
struct objc_module {
	unsigned long version;
	unsigned long size;
	const char *name;
	Symtab symtab;
};
#else  // Assume ILP32 for native-width items
struct objc_class {
	uint32_t isa;
	uint32_t super_class;
	uint32_t name;
	int32_t version;
	int32_t info;
	int32_t instance_size;
	uint32_t ivars;
	uint32_t methodLists;
	uint32_t cache;
	uint32_t protocols;
};

struct objc_ivar {
	uint32_t ivar_name;
	uint32_t ivar_type;
	int32_t ivar_offset;
};

struct objc_ivar_list {
	int32_t ivar_count;
	/* variable length structure */
	struct objc_ivar ivar_list[1];
};

struct objc_method {
	uint32_t method_name;
	uint32_t method_types;
	uint32_t method_imp;
};

struct objc_method_list {
	uint32_t obsolete;
	int32_t method_count;
	/* variable length structure */
	struct objc_method method_list[1];
};

struct objc_symtab {
	uint32_t sel_ref_cnt;
	uint32_t refs;
	uint16_t cls_def_cnt;
	uint16_t cat_def_cnt;
	uint32_t defs[1];  /* variable size */
};

struct objc_module {
	uint32_t version;
	uint32_t size;
	uint32_t name;
	uint32_t symtab;
};
#endif

#include <cassert>
#include <cstdio>
#include <cstring>

//#define DEBUG_MACHO_LOADER
//#define DEBUG_MACHO_LOADER_OBJC

MachOBinaryFile::MachOBinaryFile()
{
	bigendian = true;
}

MachOBinaryFile::~MachOBinaryFile()
{
	delete [] base;
}

ADDRESS
MachOBinaryFile::getEntryPoint() const
{
	return entrypoint;
}

ADDRESS
MachOBinaryFile::getMainEntryPoint()
{
	ADDRESS aMain = getAddressByName("main", true);
	if (aMain != NO_ADDRESS)
		return aMain;
	aMain = getAddressByName("_main", true);
	if (aMain != NO_ADDRESS)
		return aMain;

	return NO_ADDRESS;
}

bool
MachOBinaryFile::load(std::istream &ifs)
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
	const char *strtbl = nullptr;
	uint32_t *indirectsymtbl = nullptr;
	ADDRESS objc_symbols = NO_ADDRESS, objc_modules = NO_ADDRESS, objc_strings = NO_ADDRESS, objc_refs = NO_ADDRESS;
	unsigned objc_modules_size = 0;

	for (unsigned i = 0; i < header.ncmds; ++i) {
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
				char segname[sizeof seg.segname + 1];
				memcpy(segname, seg.segname, sizeof seg.segname);
				segname[sizeof seg.segname] = '\0';
				fprintf(stdout, "seg %s addr %x size %x fileoff %x filesize %x flags %x\n", segname, seg.vmaddr, seg.vmsize, seg.fileoff, seg.filesize, seg.flags);
#endif
				for (unsigned n = 0; n < seg.nsects; ++n) {
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
					char sectname[sizeof sect.sectname + 1];
					memcpy(sectname, sect.sectname, sizeof sect.sectname);
					sectname[sizeof sect.sectname] = '\0';
					fprintf(stdout, "\tsect %s addr %x size %x flags %x\n", sectname, sect.addr, sect.size, sect.flags);
#endif
					if ((sect.flags & SECTION_TYPE) == S_SYMBOL_STUBS) {
						stubs_sects.push_back(sect);
#ifdef DEBUG_MACHO_LOADER
						fprintf(stdout, "\t\tsymbol stubs section, start index %x, stub size %x\n", sect.reserved1, sect.reserved2);
#endif
					}
					if (!strncmp(sect.sectname, SECT_OBJC_SYMBOLS, sizeof sect.sectname)) {
						assert(objc_symbols == NO_ADDRESS);
						objc_symbols = sect.addr;
					}
					if (!strncmp(sect.sectname, SECT_OBJC_MODULES, sizeof sect.sectname)) {
						assert(objc_modules == NO_ADDRESS);
						objc_modules = sect.addr;
						objc_modules_size = sect.size;
					}
					if (!strncmp(sect.sectname, SECT_OBJC_STRINGS, sizeof sect.sectname)) {
						assert(objc_strings == NO_ADDRESS);
						objc_strings = sect.addr;
					}
					if (!strncmp(sect.sectname, SECT_OBJC_REFS, sizeof sect.sectname)) {
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
				ifs.read((char *)strtbl, syms.strsize);

				ifs.seekg(syms.symoff);
				for (unsigned n = 0; n < syms.nsyms; ++n) {
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
				fprintf(stdout, "dysymtab local %i %i extdef %i %i undef %i %i\n",
				        syms.ilocalsym, syms.nlocalsym,
				        syms.iextdefsym, syms.nextdefsym,
				        syms.iundefsym, syms.nundefsym);
#endif
				ifs.seekg(syms.indirectsymoff);
				indirectsymtbl = new uint32_t[syms.nindirectsyms];
				ifs.read((char *)indirectsymtbl, sizeof *indirectsymtbl * syms.nindirectsyms);
				for (unsigned j = 0; j < syms.nindirectsyms; ++j) {
					indirectsymtbl[j] = BMMH(indirectsymtbl[j]);
				}
#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "dysymtab has %i indirect symbols: ", syms.nindirectsyms);
				for (unsigned j = 0; j < syms.nindirectsyms; ++j) {
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

	const struct segment_command *lowest = &segments[0], *highest = &segments[0];
	for (const auto &segment : segments) {
		if (segment.vmaddr < lowest->vmaddr)
			lowest = &segment;
		if (segment.vmaddr > highest->vmaddr)
			highest = &segment;
	}

	loaded_addr = lowest->vmaddr;
	loaded_size = highest->vmaddr - lowest->vmaddr + highest->vmsize;

	base = new char[loaded_size];

	sections.reserve(segments.size());
	for (const auto &seg : segments) {
		ifs.seekg(seg.fileoff);
		ADDRESS a = seg.vmaddr;
		unsigned sz = seg.vmsize;
		unsigned fsz = seg.filesize;
		memset(&base[a - loaded_addr], 0, sz);
		ifs.read(&base[a - loaded_addr], fsz);
#ifdef DEBUG_MACHO_LOADER
		fprintf(stderr, "loaded segment %x %x in mem %x in file\n", a, sz, fsz);
#endif

		auto name = std::string(seg.segname, sizeof seg.segname);
		auto len = name.find('\0');
		if (len != name.npos)
			name.erase(len);

		auto sect = SectionInfo();
		sect.name = name;
		sect.uNativeAddr = a;
		sect.uHostAddr = &base[a - loaded_addr];
		sect.uSectionSize = sz;

		auto l = seg.initprot;
		sect.bBss      = false; // TODO
		sect.bCode     =  (l & VM_PROT_EXECUTE) != 0;
		sect.bData     =  (l & VM_PROT_READ)    != 0;
		sect.bReadOnly = ~(l & VM_PROT_WRITE)   == 0;  // FIXME: This is always false.
		sections.push_back(sect);
	}

	// process stubs_sects
	for (const auto &stubs_sect : stubs_sects) {
		unsigned startidx = stubs_sect.reserved1;
		for (unsigned i = 0; i < stubs_sect.size / stubs_sect.reserved2; ++i) {
			ADDRESS addr = stubs_sect.addr + i * stubs_sect.reserved2;
			const auto &symbol = symbols[indirectsymtbl[startidx + i]];
			auto name = strtbl + symbol.n_un.n_strx;
#ifdef DEBUG_MACHO_LOADER
			fprintf(stdout, "stub for %s at %x\n", name, addr);
#endif
			if (*name == '_')  // we want printf not _printf
				++name;
			m_SymA[addr] = name;
			dlprocs[addr] = name;
		}
	}

	// process the remaining symbols
	for (const auto &symbol : symbols) {
		auto name = strtbl + symbol.n_un.n_strx;
		if (symbol.n_un.n_strx != 0 && symbol.n_value != 0 && *name != '\0') {

#ifdef DEBUG_MACHO_LOADER
			fprintf(stdout, "symbol %s at %x type %x\n", name,
			        symbol.n_value,
			        symbol.n_type & N_TYPE);
#endif
			if (*name == '_')  // we want main not _main
				++name;
			m_SymA[symbol.n_value] = name;
		}
	}

	// process objective-c section
	if (objc_modules != NO_ADDRESS) {
#ifdef DEBUG_MACHO_LOADER_OBJC
		fprintf(stdout, "processing objective-c section\n");
#endif
		for (unsigned i = 0; i < objc_modules_size;) {
			auto module = (const struct objc_module *)&base[objc_modules - loaded_addr];
			auto size = BMMH(module->size);
			auto name = (const char *)&base[BMMH(module->name) - loaded_addr];
			auto symtab = (const struct objc_symtab *)&base[BMMH(module->symtab) - loaded_addr];
			auto cls_def_cnt = BMMHW(symtab->cls_def_cnt);
#ifdef DEBUG_MACHO_LOADER_OBJC
			fprintf(stdout, "module %s (%i classes)\n", name, cls_def_cnt);
#endif
			auto &m = modules[name];
			m.name = name;
			for (unsigned j = 0; j < cls_def_cnt; ++j) {
				auto def = (const struct objc_class *)&base[BMMH(symtab->defs[j]) - loaded_addr];
				auto name = (const char *)&base[BMMH(def->name) - loaded_addr];
#ifdef DEBUG_MACHO_LOADER_OBJC
				fprintf(stdout, "\tclass %s\n", name);
#endif
				auto &cl = m.classes[name];
				cl.name = name;
				auto ivars = (const struct objc_ivar_list *)&base[BMMH(def->ivars) - loaded_addr];
				auto ivar_count = BMMH(ivars->ivar_count);
				for (unsigned k = 0; k < ivar_count; ++k) {
					const auto &ivar = ivars->ivar_list[k];
					auto name = (const char *)&base[BMMH(ivar.ivar_name) - loaded_addr];
					auto type = (const char *)&base[BMMH(ivar.ivar_type) - loaded_addr];
					auto offset = BMMH(ivar.ivar_offset);
#ifdef DEBUG_MACHO_LOADER_OBJC
					fprintf(stdout, "\t\tivar %s %s %x\n", name, type, offset);
#endif
					auto &iv = cl.ivars[name];
					iv.name = name;
					iv.type = type;
					iv.offset = offset;
				}
				// this is weird, why is it defined as a ** in the struct but used as a * in otool?
				auto methods = (const struct objc_method_list *)&base[BMMH(def->methodLists) - loaded_addr];
				auto method_count = BMMH(methods->method_count);
				for (unsigned k = 0; k < method_count; ++k) {
					const auto &method = methods->method_list[k];
					auto name = (const char *)&base[BMMH(method.method_name) - loaded_addr];
					auto types = (const char *)&base[BMMH(method.method_types) - loaded_addr];
					auto imp = BMMH(method.method_imp);
#ifdef DEBUG_MACHO_LOADER_OBJC
					fprintf(stdout, "\t\tmethod %s %s %x\n", name, types, imp);
#endif
					auto &me = cl.methods[name];
					me.name = name;
					me.types = types;
					me.addr = imp;
				}
			}
			objc_modules += size;
			i += size;
		}
	}

	// Give the entry point a symbol
	//ADDRESS entry = getMainEntryPoint();
	entrypoint = getMainEntryPoint();

	return true;
}

#if 0 // Cruft?
bool
MachOBinaryFile::PostLoad(void *handle)
{
	return false;
}
#endif

const char *
MachOBinaryFile::getSymbolByAddress(ADDRESS dwAddr)
{
	auto it = m_SymA.find(dwAddr);
	if (it == m_SymA.end())
		return nullptr;
	return it->second.c_str();
}

ADDRESS
MachOBinaryFile::getAddressByName(const std::string &name, bool bNoTypeOK) const
{
	// This is "looking up the wrong way" and hopefully is uncommon.  Use linear search
	for (const auto &sym : m_SymA) {
		// std::cerr << "Symbol: " << sym.second << " at 0x" << std::hex << sym.first << std::dec << "\n";
		if (sym.second == name)
			return sym.first;
	}
	return NO_ADDRESS;
}

void
MachOBinaryFile::addSymbol(ADDRESS uNative, const std::string &name)
{
	m_SymA[uNative] = name;
}

uint32_t
MachOBinaryFile::BMMH(uint32_t x) const
{
	if (swap_bytes) return BH32(&x);
	else return x;
}

uint16_t
MachOBinaryFile::BMMHW(uint16_t x) const
{
	if (swap_bytes) return BH16(&x);
	else return x;
}

// FIXME:  Should this be isDynamicLinkedProcPointer() instead?
//         getDynamicProcName() is always used with isDynamicLinkedProcPointer().
bool
MachOBinaryFile::isDynamicLinkedProc(ADDRESS uNative) const
{
	return !!dlprocs.count(uNative);
}

const char *
MachOBinaryFile::getDynamicProcName(ADDRESS uNative) const
{
	auto it = dlprocs.find(uNative);
	if (it != dlprocs.end())
		return it->second.c_str();
	return nullptr;
}

#if 0 // Cruft?
bool
MachOBinaryFile::isLibrary() const
{
	return false;
}

ADDRESS
MachOBinaryFile::getImageBase() const
{
	return loaded_addr;
}

size_t
MachOBinaryFile::getImageSize() const
{
	return loaded_size;
}
#endif

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new BinaryFile
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" BinaryFile *
construct()
{
	return new MachOBinaryFile();
}
extern "C" void
destruct(BinaryFile *bf)
{
	delete (MachOBinaryFile *)bf;
}
#endif
