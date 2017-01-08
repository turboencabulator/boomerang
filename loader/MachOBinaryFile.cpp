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

#include "nlist.h"
#include "macho-apple.h"

#include "objc/objc-class.h"
#include <cstdarg>  // For va_list for MinGW at least
#include "objc/objc-runtime.h"

#include <cassert>
#include <cstdio>
#include <cstring>

//#define DEBUG_MACHO_LOADER
//#define DEBUG_MACHO_LOADER_OBJC

MachOBinaryFile::MachOBinaryFile() :
	header(NULL),
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
	delete    header;
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
	header = new struct mach_header;
	ifs.read((char *)header, sizeof *header);

	if ((header->magic != MH_MAGIC) && (_BMMH(header->magic) != MH_MAGIC)) {
		fprintf(stderr, "error loading file %s, bad Mach-O magic\n", getFilename());
		return false;
	}

	// check for swapped bytes
	swap_bytes = (_BMMH(header->magic) == MH_MAGIC);

	// Determine CPU type
	if (BMMH(header->cputype) == 0x07)
		machine = MACHINE_PENTIUM;
	else
		machine = MACHINE_PPC;

	std::vector<struct segment_command> segments;
	std::vector<struct nlist> symbols;
	unsigned startlocal, nlocal, startdef, ndef, startundef, nundef;
	std::vector<struct section> stubs_sects;
	char *strtbl = NULL;
	unsigned *indirectsymtbl = NULL;
	ADDRESS objc_symbols = NO_ADDRESS, objc_modules = NO_ADDRESS, objc_strings = NO_ADDRESS, objc_refs = NO_ADDRESS;
	unsigned objc_modules_size = 0;

	ifs.seekg(sizeof *header);
	for (unsigned i = 0; i < BMMH(header->ncmds); i++) {
		struct load_command cmd;
		std::streamsize pos = ifs.tellg();
		ifs.read((char *)&cmd, sizeof cmd);

		ifs.seekg(pos);
		switch (BMMH(cmd.cmd)) {
		case LC_SEGMENT:
			{
				struct segment_command seg;
				ifs.read((char *)&seg, sizeof seg);
				segments.push_back(seg);
#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "seg addr %x size %i fileoff %x filesize %i flags %x\n", BMMH(seg.vmaddr), BMMH(seg.vmsize), BMMH(seg.fileoff), BMMH(seg.filesize), BMMH(seg.flags));
#endif
				for (unsigned n = 0; n < BMMH(seg.nsects); n++) {
					struct section sect;
					ifs.read((char *)&sect, sizeof sect);
#ifdef DEBUG_MACHO_LOADER
					fprintf(stdout, "    sectname %s segname %s addr %x size %i flags %x\n", sect.sectname, sect.segname, BMMH(sect.addr), BMMH(sect.size), BMMH(sect.flags));
#endif
					if ((BMMH(sect.flags) & SECTION_TYPE) == S_SYMBOL_STUBS) {
						stubs_sects.push_back(sect);
#ifdef DEBUG_MACHO_LOADER
						fprintf(stdout, "        symbol stubs section, start index %i, stub size %i\n", BMMH(sect.reserved1), BMMH(sect.reserved2));
#endif
					}
					if (!strcmp(sect.sectname, SECT_OBJC_SYMBOLS)) {
						assert(objc_symbols == NO_ADDRESS);
						objc_symbols = BMMH(sect.addr);
					}
					if (!strcmp(sect.sectname, SECT_OBJC_MODULES)) {
						assert(objc_modules == NO_ADDRESS);
						objc_modules = BMMH(sect.addr);
						objc_modules_size = BMMH(sect.size);
					}
					if (!strcmp(sect.sectname, SECT_OBJC_STRINGS)) {
						assert(objc_strings == NO_ADDRESS);
						objc_strings = BMMH(sect.addr);
					}
					if (!strcmp(sect.sectname, SECT_OBJC_REFS)) {
						assert(objc_refs == NO_ADDRESS);
						objc_refs = BMMH(sect.addr);
					}
				}
			}
			break;
		case LC_SYMTAB:
			{
				struct symtab_command syms;
				ifs.read((char *)&syms, sizeof syms);
				ifs.seekg(BMMH(syms.stroff));
				strtbl = new char[BMMH(syms.strsize)];
				ifs.read(strtbl, BMMH(syms.strsize));
				ifs.seekg(BMMH(syms.symoff));
				for (unsigned n = 0; n < BMMH(syms.nsyms); n++) {
					struct nlist sym;
					ifs.read((char *)&sym, sizeof sym);
					symbols.push_back(sym);
#ifdef DEBUG_MACHO_LOADER
					//fprintf(stdout, "got sym %s flags %x value %x\n", strtbl + BMMH(sym.n_un.n_strx), sym.n_type, BMMH(sym.n_value));
#endif
				}
#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "symtab contains %i symbols\n", BMMH(syms.nsyms));
#endif
			}
			break;
		case LC_DYSYMTAB:
			{
				struct dysymtab_command syms;
				ifs.read((char *)&syms, sizeof syms);
#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "dysymtab local %i %i defext %i %i undef %i %i\n",
				        BMMH(syms.ilocalsym), BMMH(syms.nlocalsym),
				        BMMH(syms.iextdefsym), BMMH(syms.nextdefsym),
				        BMMH(syms.iundefsym), BMMH(syms.nundefsym));
#endif
				startlocal = BMMH(syms.ilocalsym);
				nlocal = BMMH(syms.nlocalsym);
				startdef = BMMH(syms.iextdefsym);
				ndef = BMMH(syms.nextdefsym);
				startundef = BMMH(syms.iundefsym);
				nundef = BMMH(syms.nundefsym);

#ifdef DEBUG_MACHO_LOADER
				fprintf(stdout, "dysymtab has %i indirect symbols: ", BMMH(syms.nindirectsyms));
#endif
				indirectsymtbl = new unsigned[BMMH(syms.nindirectsyms)];
				ifs.seekg(BMMH(syms.indirectsymoff));
				ifs.read((char *)indirectsymtbl, sizeof *indirectsymtbl * BMMH(syms.nindirectsyms));
#ifdef DEBUG_MACHO_LOADER
				for (unsigned j = 0; j < BMMH(syms.nindirectsyms); j++) {
					fprintf(stdout, "%i ", BMMH(indirectsymtbl[j]));
				}
				fprintf(stdout, "\n");
#endif
			}
			break;
		default:
#ifdef DEBUG_MACHO_LOADER
			fprintf(stderr, "not handled load command %x\n", BMMH(cmd.cmd));
#endif
			// yep, there's lots of em
			break;
		}

		ifs.seekg(pos + BMMH(cmd.cmdsize));
	}

	struct segment_command *lowest = &segments[0], *highest = &segments[0];
	for (unsigned i = 1; i < segments.size(); i++) {
		if (BMMH(segments[i].vmaddr) < BMMH(lowest->vmaddr))
			lowest = &segments[i];
		if (BMMH(segments[i].vmaddr) > BMMH(highest->vmaddr))
			highest = &segments[i];
	}

	loaded_addr = BMMH(lowest->vmaddr);
	loaded_size = BMMH(highest->vmaddr) - BMMH(lowest->vmaddr) + BMMH(highest->vmsize);

	base = new char[loaded_size];

	m_iNumSections = segments.size();
	m_pSections = new SectionInfo[m_iNumSections];

	for (unsigned i = 0; i < segments.size(); i++) {
		ifs.seekg(BMMH(segments[i].fileoff));
		ADDRESS a = BMMH(segments[i].vmaddr);
		unsigned sz = BMMH(segments[i].vmsize);
		unsigned fsz = BMMH(segments[i].filesize);
		memset(base + a - loaded_addr, 0, sz);
		ifs.read(base + a - loaded_addr, fsz);
#ifdef DEBUG_MACHO_LOADER
		fprintf(stderr, "loaded segment %x %i in mem %i in file\n", a, sz, fsz);
#endif

		char *name = new char[17];
		strncpy(name, segments[i].segname, 16);
		name[16] = '\0';
		m_pSections[i].pSectionName = name;
		m_pSections[i].uNativeAddr = BMMH(segments[i].vmaddr);
		m_pSections[i].uHostAddr = (ADDRESS)base + BMMH(segments[i].vmaddr) - loaded_addr;
		m_pSections[i].uSectionSize = BMMH(segments[i].vmsize);

		unsigned long l = BMMH(segments[i].initprot);
		m_pSections[i].bBss      = false; // TODO
		m_pSections[i].bCode     =  (l & VM_PROT_EXECUTE) != 0;
		m_pSections[i].bData     =  (l & VM_PROT_READ)    != 0;
		m_pSections[i].bReadOnly = ~(l & VM_PROT_WRITE)   == 0;  // FIXME: This is always false.
	}

	// process stubs_sects
	for (unsigned j = 0; j < stubs_sects.size(); j++) {
		for (unsigned i = 0; i < BMMH(stubs_sects[j].size) / BMMH(stubs_sects[j].reserved2); i++) {
			unsigned startidx = BMMH(stubs_sects[j].reserved1);
			unsigned symbol = BMMH(indirectsymtbl[startidx + i]);
			ADDRESS addr = BMMH(stubs_sects[j].addr) + i * BMMH(stubs_sects[j].reserved2);
#ifdef DEBUG_MACHO_LOADER
			fprintf(stdout, "stub for %s at %x\n", strtbl + BMMH(symbols[symbol].n_un.n_strx), addr);
#endif
			const char *name = strtbl + BMMH(symbols[symbol].n_un.n_strx);
			if (*name == '_')  // we want printf not _printf
				name++;
			m_SymA[addr] = name;
			dlprocs[addr] = name;
		}
	}

	// process the remaining symbols
	for (unsigned i = 0; i < symbols.size(); i++) {
		const char *name = strtbl + BMMH(symbols[i].n_un.n_strx);
		if (BMMH(symbols[i].n_un.n_strx) != 0 && BMMH(symbols[i].n_value) != 0 && *name != 0) {

#ifdef DEBUG_MACHO_LOADER
			fprintf(stdout, "symbol %s at %x type %x\n", name,
			        BMMH(symbols[i].n_value),
			        BMMH(symbols[i].n_type) & N_TYPE);
#endif
			if (*name == '_')  // we want main not _main
				name++;
			m_SymA[BMMH(symbols[i].n_value)] = name;
		}
	}

	// process objective-c section
	if (objc_modules != NO_ADDRESS) {
#ifdef DEBUG_MACHO_LOADER_OBJC
		fprintf(stdout, "processing objective-c section\n");
#endif
		for (unsigned i = 0; i < objc_modules_size;) {
			struct objc_module *module = (struct objc_module *)((ADDRESS)base + objc_modules - loaded_addr + i);
			const char *name = (const char *)((ADDRESS)base + BMMH(module->name) - loaded_addr);
			Symtab symtab = (Symtab)((ADDRESS)base + BMMH(module->symtab) - loaded_addr);
#ifdef DEBUG_MACHO_LOADER_OBJC
			fprintf(stdout, "module %s (%i classes)\n", name, BMMHW(symtab->cls_def_cnt));
#endif
			ObjcModule *m = &modules[name];
			m->name = name;
			for (unsigned j = 0; j < BMMHW(symtab->cls_def_cnt); j++) {
				struct objc_class *def = (struct objc_class *)((ADDRESS)base + BMMH(symtab->defs[j]) - loaded_addr);
				const char *name = (const char *)((ADDRESS)base + BMMH(def->name) - loaded_addr);
#ifdef DEBUG_MACHO_LOADER_OBJC
				fprintf(stdout, "  class %s\n", name);
#endif
				ObjcClass *cl = &m->classes[name];
				cl->name = name;
				struct objc_ivar_list *ivars = (struct objc_ivar_list *)((ADDRESS)base + BMMH(def->ivars) - loaded_addr);
				for (unsigned k = 0; k < static_cast<unsigned int>(BMMH(ivars->ivar_count)); k++) {
					struct objc_ivar *ivar = &ivars->ivar_list[k];
					const char *name = (const char *)((ADDRESS)base + BMMH(ivar->ivar_name) - loaded_addr);
					const char *types = (const char *)((ADDRESS)base + BMMH(ivar->ivar_type) - loaded_addr);
#ifdef DEBUG_MACHO_LOADER_OBJC
					fprintf(stdout, "    ivar %s %s %x\n", name, types, BMMH(ivar->ivar_offset));
#endif
					ObjcIvar *iv = &cl->ivars[name];
					iv->name = name;
					iv->type = types;
					iv->offset = BMMH(ivar->ivar_offset);
				}
				// this is weird, why is it defined as a ** in the struct but used as a * in otool?
				struct objc_method_list *methods = (struct objc_method_list *)((ADDRESS)base + BMMH(def->methodLists) - loaded_addr);
				for (unsigned k = 0; k < static_cast<unsigned int>(BMMH(methods->method_count)); k++) {
					struct objc_method *method = &methods->method_list[k];
					const char *name = (const char *)((ADDRESS)base + BMMH(method->method_name) - loaded_addr);
					const char *types = (const char *)((ADDRESS)base + BMMH(method->method_types) - loaded_addr);
#ifdef DEBUG_MACHO_LOADER_OBJC
					fprintf(stdout, "    method %s %s %x\n", name, types, BMMH((void *)method->method_imp));
#endif
					ObjcMethod *me = &cl->methods[name];
					me->name = name;
					me->types = types;
					me->addr = BMMH((void *)method->method_imp);
				}
			}
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
int MachOBinaryFile::machORead2(short *ps) const
{
	unsigned char *p = (unsigned char *)ps;
	// Big endian
	int n = (int)(p[1] + (p[0] << 8));
	return n;
}

/**
 * \brief Read 4 bytes from native addr.
 */
int MachOBinaryFile::machORead4(int *pi) const
{
	short *p = (short *)pi;
	int n1 = machORead2(p);
	int n2 = machORead2(p + 1);
	int n = (int)(n2 | (n1 << 16));
	return n;
}

#if 0
void *MachOBinaryFile::BMMH(void *x)
{
	if (swap_bytes) return _BMMH(x);
	else return x;
}
#endif

char *MachOBinaryFile::BMMH(char *x)
{
	if (swap_bytes) return (char *)_BMMH(x);
	else return x;
}

unsigned int MachOBinaryFile::BMMH(void *x)
{
	if (swap_bytes) return (unsigned int)_BMMH(x);
	else return (unsigned int)x;
}

const char *MachOBinaryFile::BMMH(const char *x)
{
	if (swap_bytes) return (const char *)_BMMH(x);
	else return x;
}

unsigned int MachOBinaryFile::BMMH(unsigned long x)
{
	if (swap_bytes) return _BMMH(x);
	else return x;
}

unsigned int MachOBinaryFile::BMMH(long int &x)
{
	if (swap_bytes) return _BMMH(x);
	else return x;
}

signed int MachOBinaryFile::BMMH(signed int x)
{
	if (swap_bytes) return _BMMH(x);
	else return x;
}

unsigned int MachOBinaryFile::BMMH(unsigned int x)
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
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0)
		si = getSectionInfo(0);
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	return *(char *)host;
}

int MachOBinaryFile::readNative2(ADDRESS nat) const
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0) return 0;
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	return machORead2((short *)host);
}

int MachOBinaryFile::readNative4(ADDRESS nat) const
{
	SectionInfo *si = getSectionInfoByAddr(nat);
	if (si == 0) return 0;
	ADDRESS host = si->uHostAddr - si->uNativeAddr + nat;
	return machORead4((int *)host);
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
