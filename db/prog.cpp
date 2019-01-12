/**
 * \file
 * \brief Implementation of the program class.  Holds information of interest
 *        to the whole program.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002-2003, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "prog.h"

#include "BinaryFile.h"
#include "ansi-c-parser.h"
#include "boomerang.h"
#include "cfg.h"
#include "cluster.h"
#include "exp.h"
#include "frontend.h"
#include "hllcode.h"
#include "managed.h"
#include "proc.h"
#include "signature.h"
#include "statement.h"
#include "type.h"
#include "types.h"
#include "util.h"       // For lockFileWrite etc

#include <sys/stat.h>   // For mkdir
#include <sys/types.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

#include <cassert>
#include <cstdlib>
#include <cstring>

Prog::Prog() :
	m_rootCluster(new Cluster("prog"))
{
	// Default constructor
}

void
Prog::setFrontEnd(FrontEnd *pFE)
{
	pBF = pFE->getBinaryFile();
	this->pFE = pFE;
	if (pBF && pBF->getFilename()) {
		m_name = pBF->getFilename();
		m_rootCluster = new Cluster(getNameNoPathNoExt());
	}
}

Prog::Prog(const std::string &name) :
	m_name(name),
	m_path(name),
	m_rootCluster(new Cluster(getNameNoPathNoExt()))
{
}

Prog::~Prog()
{
	if (pFE) FrontEnd::close(pFE);
	if (pBF) BinaryFile::close(pBF);
	for (const auto &proc : m_procs)
		delete proc;
}

void
Prog::setName(const std::string &name)  // Assign a name to this program
{
	m_name = name;
	m_rootCluster->setName(name);
}

// well form the entire program
bool
Prog::wellForm()
{
	bool wellformed = true;

	for (const auto &proc : m_procs)
		if (auto up = dynamic_cast<UserProc *>(proc))
			wellformed &= up->getCFG()->wellFormCfg();

	return wellformed;
}

// last fixes after decoding everything
// was in analysis.cpp
void
Prog::finishDecode()
{
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;

			up->assignProcsToCalls();
			up->finalSimplify();
		}
	}
}

void
Prog::generateDot(std::ostream &os) const
{
	os << "digraph Cfg {\n";

	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			// Subgraph for the proc name
			os << "\tsubgraph cluster_" << up->getName() << " {\n"
			   << "\t\tcolor=gray;\n\t\tlabel=\"" << up->getName() << "\";\n";
			// Generate dotty CFG for this proc
			up->getCFG()->generateDot(os);
		}
	}

	os << "}\n";
}

void
Prog::generateCode(Cluster *cluster, UserProc *uProc, bool intermixRTL)
{
	std::string basedir = m_rootCluster->makeDirs();
	std::ofstream os;
	if (cluster) {
		cluster->openStream("c");
		cluster->closeStreams();
	}
	if (!cluster || cluster == m_rootCluster) {
		os.open(m_rootCluster->getOutPath("c"));
		if (!uProc) {
			HLLCode *code = Boomerang::get()->getHLLCode();
			bool global_added = false;
			if (Boomerang::get()->noDecompile) {
				const char *sections[] = { "rodata", "data", "data1", nullptr };
				for (int j = 0; sections[j]; ++j) {
					std::string str = ".";
					str += sections[j];
					auto info = pBF->getSectionInfoByName(str);
					str = "start_";
					str += sections[j];
					code->AddGlobal(str, new IntegerType(32, -1), new Const(info ? info->uNativeAddr : (unsigned int)-1));
					str = sections[j];
					str += "_size";
					code->AddGlobal(str, new IntegerType(32, -1), new Const(info ? info->uSectionSize : (unsigned int)-1));
					Exp *l = new Terminal(opNil);
					for (unsigned int i = 0; info && i < info->uSectionSize; ++i) {
						int n = pBF->readNative1(info->uNativeAddr + info->uSectionSize - 1 - i);
						if (n < 0)
							n = 256 + n;
						l = new Binary(opList, new Const(n), l);
					}
					code->AddGlobal(sections[j], new ArrayType(new IntegerType(8, -1), info ? info->uSectionSize : 0), l);
				}
				code->AddGlobal("source_endianness", new IntegerType(), new Const(getFrontEndId() != PLAT_PENTIUM));
				os << "#include \"boomerang.h\"\n\n";
				global_added = true;
			}
			for (const auto &global : globals) {
				// Check for an initial value
				Exp *e = global->getInitialValue(this);
				//if (e) {
					code->AddGlobal(global->getName(), global->getType(), e);
					global_added = true;
				//}
			}
			if (global_added) code->print(os);  // Avoid blank line if no globals
		}
	}

	// First declare prototypes for all but the first proc
	bool first = true, proto = false;
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (first) {
				first = false;
				continue;
			}
			proto = true;
			HLLCode *code = Boomerang::get()->getHLLCode(up);
			code->AddPrototype(up);  // May be the wrong signature if up has ellipsis
			if (!cluster || cluster == m_rootCluster)
				code->print(os);
		}
	}
	if ((proto && !cluster) || cluster == m_rootCluster)
		os << "\n";  // Separate prototype(s) from first proc

	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			if (uProc && up != uProc)
				continue;
			up->getCFG()->compressCfg();
			HLLCode *code = Boomerang::get()->getHLLCode(up);
			up->generateCode(code);
			if (up->getCluster() == m_rootCluster) {
				if (!cluster || cluster == m_rootCluster)
					code->print(os);
			} else {
				if (!cluster || cluster == up->getCluster()) {
					up->getCluster()->openStream("c");
					code->print(up->getCluster()->getStream());
				}
			}
		}
	}
	os.close();
	m_rootCluster->closeStreams();
}

void
Prog::generateRTL(Cluster *cluster, UserProc *uProc) const
{
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			if (uProc && up != uProc)
				continue;
			if (cluster && up->getCluster() != cluster)
				continue;

			up->getCluster()->openStream("rtl");
			up->print(up->getCluster()->getStream());
		}
	}
	m_rootCluster->closeStreams();
}

#if 0 // Cruft?
Statement *
Prog::getStmtAtLex(Cluster *cluster, unsigned int begin, unsigned int end) const
{
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			if (cluster && up->getCluster() != cluster)
				continue;

			if (up->getCluster() == cluster) {
				if (auto s = up->getStmtAtLex(begin, end))
					return s;
			}
		}
	}
	return nullptr;
}
#endif

std::string
Cluster::makeDirs() const
{
	std::string path;
	if (parent)
		path = parent->makeDirs();
	else
		path = Boomerang::get()->getOutputPath();
	if (getNumChildren() > 0 || !parent) {
		path = path + name;
		mkdir(path.c_str(), 0777);
	}
	return path;
}

void
Cluster::removeChild(Cluster *n)
{
	auto it = std::find(children.begin(), children.end(), n);
	assert(it != children.end());
	if (it != children.end())
		children.erase(it);
}

void
Cluster::addChild(Cluster *n)
{
	if (n->parent)
		n->parent->removeChild(n);
	children.push_back(n);
	n->parent = this;
}

Cluster *
Cluster::find(const std::string &nam)
{
	if (name == nam)
		return this;
	for (const auto &child : children) {
		if (auto c = child->find(nam))
			return c;
	}
	return nullptr;
}

std::string
Cluster::getOutPath(const std::string &ext) const
{
	return makeDirs() + "/" + name + "." + ext;
}

void
Cluster::openStream(const std::string &ext)
{
	if (out.is_open())
		return;
	out.open(getOutPath(ext));
	if (ext == "xml") {
		out << "<?xml version=\"1.0\"?>\n";
		if (parent)
			out << "<procs>\n";
	}
}

void
Cluster::openStreams(const std::string &ext)
{
	openStream(ext);
	for (const auto &child : children)
		child->openStreams(ext);
}

void
Cluster::closeStreams()
{
	if (out.is_open())
		out.close();
	for (const auto &child : children)
		child->closeStreams();
}

bool
Prog::clusterUsed(Cluster *c) const
{
	for (const auto &proc : m_procs)
		if (proc->getCluster() == c)
			return true;
	return false;
}

Cluster *
Prog::getDefaultCluster(const std::string &name) const
{
	if (pBF) {
		if (auto cfname = pBF->getFilenameSymbolFor(name)) {
			auto fname = std::string(cfname);
			auto len = fname.length();
			if (len >= 2 && fname.compare(len - 2, fname.npos, ".c") == 0) {
				LOG << "got filename " << fname << " for " << name << "\n";
				fname.erase(len - 2);
				auto c = findCluster(fname);
				if (!c) {
					c = new Cluster(fname);
					m_rootCluster->addChild(c);
				}
				return c;
			}
		}
	}
	return m_rootCluster;
}

void
Prog::generateCode(std::ostream &os)
{
	HLLCode *code = Boomerang::get()->getHLLCode();
	for (const auto &global : globals) {
		// Check for an initial value
		if (auto e = global->getInitialValue(this))
			code->AddGlobal(global->getName(), global->getType(), e);
	}
	code->print(os);
	delete code;
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			up->getCFG()->compressCfg();
			code = Boomerang::get()->getHLLCode(up);
			up->generateCode(code);
			code->print(os);
			delete code;
		}
	}
}

// Print this program, mainly for debugging
void
Prog::print(std::ostream &out) const
{
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;

			// decoded userproc.. print it
			up->print(out);
		}
	}
}

/*==============================================================================
 * FUNCTION:    Prog::setNewProc
 * NOTE:        Formally Frontend::newProc
 * OVERVIEW:    Call this function when a procedure is discovered (usually by
 *                decoding a call instruction). That way, it is given a name
 *                that can be displayed in the dot file, etc. If we assign it
 *                a number now, then it will retain this number always
 * PARAMETERS:  uAddr - Native address of the procedure entry point
 * RETURNS:     Pointer to the Proc object, or nullptr if this is a deleted (not to
 *                be decoded) address
 *============================================================================*/
Proc *
Prog::setNewProc(ADDRESS uAddr)
{
	// this test fails when decoding sparc, why?  Please investigate - trent
	// Likely because it is in the Procedure Linkage Table (.plt), which for Sparc is in the data section
	//assert(uAddr >= limitTextLow && uAddr < limitTextHigh);
	// Check if we already have this proc
	Proc *pProc = findProc(uAddr);
	if (pProc == (Proc *)-1)  // Already decoded and deleted?
		return nullptr;  // Yes, exit with nullptr
	if (pProc)
		// Yes, we are done
		return pProc;
	ADDRESS other = pBF->isJumpToAnotherAddr(uAddr);
	if (other != NO_ADDRESS)
		uAddr = other;
	auto name = std::string();
	if (auto pName = pBF->getSymbolByAddress(uAddr)) {
		name = pName;
	} else {
		// No name. Give it a numbered name
		std::ostringstream ost;
		ost << "proc" << m_iNumberedProc++;
		name = ost.str();
		if (VERBOSE)
			LOG << "assigning name " << name << " to addr 0x" << std::hex << uAddr << std::dec << "\n";
	}
	bool bLib = pBF->isDynamicLinkedProc(uAddr) | pBF->isStaticLinkedLibProc(uAddr);
	pProc = newProc(name, uAddr, bLib);
	return pProc;
}

/*==============================================================================
 * FUNCTION:    Prog::newProc
 * OVERVIEW:    Creates a new Proc object, adds it to the list of procs in this Prog object, and adds the address to
 *                  the list
 * PARAMETERS:  name: Name for the proc
 *              uNative: Native address of the entry point of the proc
 *              bLib: If true, this will be a libProc; else a UserProc
 * RETURNS:     A pointer to the new Proc object
 *============================================================================*/
Proc *
Prog::newProc(const std::string &name, ADDRESS uNative, bool bLib /*= false*/)
{
	Proc *pProc;
	if (bLib)
		pProc = new LibProc(this, name, uNative);
	else
		pProc = new UserProc(this, name, uNative);

	m_procs.push_back(pProc);  // Append this to list of procs
	m_procLabels[uNative] = pProc;
	// alert the watchers of a new proc
	Boomerang::get()->alert_new(pProc);
	return pProc;
}

/*==============================================================================
 * FUNCTION:       Prog::remProc
 * OVERVIEW:       Removes the UserProc from this Prog object's list, and deletes as much as possible of the Proc
 * PARAMETERS:     proc: pointer to the UserProc object to be removed
 *============================================================================*/
void
Prog::remProc(UserProc *uProc)
{
	// Delete the cfg etc.
	uProc->deleteCFG();

	// Replace the entry in the procedure map with -1 as a warning not to decode that address ever again
	m_procLabels[uProc->getNativeAddress()] = (Proc *)-1;

	auto it = std::find(m_procs.begin(), m_procs.end(), uProc);
	if (it != m_procs.end())
		m_procs.erase(it);

	// Delete the UserProc object as well
	delete uProc;
}

void
Prog::removeProc(const std::string &name)
{
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		if (name == (*it)->getName()) {
			Boomerang::get()->alert_remove(*it);
			m_procs.erase(it);
			break;
		}
	}
}

/*==============================================================================
 * FUNCTION:    Prog::getNumProcs
 * OVERVIEW:    Return the number of real (non deleted) procedures
 * RETURNS:     The number of procedures
 *============================================================================*/
int
Prog::getNumProcs() const
{
	return m_procs.size();
}

int
Prog::getNumUserProcs() const
{
	int n = 0;
	for (const auto &proc : m_procs)
		if (dynamic_cast<UserProc *>(proc))
			++n;
	return n;
}

/*==============================================================================
 * FUNCTION:    Prog::getProc
 * OVERVIEW:    Return a pointer to the indexed Proc object
 * PARAMETERS:  Index of the proc
 * RETURNS:     Pointer to the Proc object, or nullptr if index invalid
 *============================================================================*/
Proc *
Prog::getProc(int idx) const
{
	// Return the indexed procedure. If this is used often, we should use a vector instead of a list
	// If index is invalid, result will be nullptr
	if ((idx < 0) || (idx >= (int)m_procs.size())) return nullptr;
	auto it = m_procs.cbegin();
	std::advance(it, idx);
	return *it;
}

/*==============================================================================
 * FUNCTION:    Prog::findProc
 * OVERVIEW:    Return a pointer to the associated Proc object, or nullptr if none
 * NOTE:        Could return -1 for a deleted Proc
 * PARAMETERS:  Native address of the procedure entry point
 * RETURNS:     Pointer to the Proc object, or 0 if none, or -1 if deleted
 *============================================================================*/
Proc *
Prog::findProc(ADDRESS addr) const
{
	auto it = m_procLabels.find(addr);
	if (it != m_procLabels.cend())
		return it->second;
	return nullptr;
}

Proc *
Prog::findProc(const std::string &name) const
{
	for (const auto &proc : m_procs)
		if (proc->getName() == name)
			return proc;
	return nullptr;
}

// get a library procedure by name; create if does not exist
LibProc *
Prog::getLibraryProc(const std::string &nam)
{
	if (auto p = findProc(nam))
		if (auto lp = dynamic_cast<LibProc *>(p))
			return lp;
	return (LibProc *)newProc(nam, NO_ADDRESS, true);
}

Signature *
Prog::getLibSignature(const std::string &nam) const
{
	return pFE->getLibSignature(nam);
}

void
Prog::rereadLibSignatures()
{
	pFE->readLibraryCatalog();
	for (const auto &proc : m_procs) {
		if (dynamic_cast<LibProc *>(proc)) {
			proc->setSignature(getLibSignature(proc->getName()));
			const auto &callers = proc->getCallers();
			for (const auto &caller : callers)
				caller->setSigArguments();
			Boomerang::get()->alert_update_signature(proc);
		}
	}
}

platform
Prog::getFrontEndId() const
{
	return pFE->getFrontEndId();
}

Signature *
Prog::getDefaultSignature(const std::string &name) const
{
	return pFE->getDefaultSignature(name);
}

std::vector<Exp *> &
Prog::getDefaultParams() const
{
	return pFE->getDefaultParams();
}

std::vector<Exp *> &
Prog::getDefaultReturns() const
{
	return pFE->getDefaultReturns();
}

bool
Prog::isWin32() const
{
	return pFE->isWin32();
}

const char *
Prog::getGlobalName(ADDRESS addr) const
{
	// FIXME: inefficient
	for (const auto &global : globals) {
		if (global->getAddress() == addr
		 || (global->getAddress() < addr
		  && global->getAddress() + global->getType()->getSize() / 8 > addr))
			return global->getName().c_str();
	}
	if (pBF)
		return pBF->getSymbolByAddress(addr);
	return nullptr;
}

ADDRESS
Prog::getGlobalAddr(const std::string &nam) const
{
	for (const auto &global : globals) {
		if (global->getName() == nam)
			return global->getAddress();
	}
	return pBF->getAddressByName(nam);
}

Global *
Prog::getGlobal(const std::string &nam) const
{
	for (const auto &global : globals) {
		if (global->getName() == nam)
			return global;
	}
	return nullptr;
}

bool
Prog::globalUsed(ADDRESS uaddr, Type *knownType)
{
	for (const auto &global : globals) {
		if (global->getAddress() == uaddr) {
			if (knownType) global->meetType(knownType);
			return true;
		} else if (global->getAddress() < uaddr && global->getAddress() + global->getType()->getSize() / 8 > uaddr) {
			if (knownType) global->meetType(knownType);
			return true;
		}
	}

	if (!pBF->getSectionInfoByAddr(uaddr)) {
		if (VERBOSE)
			LOG << "refusing to create a global at address that is in no known section of the binary: 0x" << std::hex << uaddr << std::dec << "\n";
		return false;
	}

	auto nam = newGlobalName(uaddr);
	Type *ty;
	if (knownType) {
		ty = knownType;
		if (ty->resolvesToArray() && ty->asArray()->isUnbounded()) {
			Type *baseType = ty->asArray()->getBaseType();
			int baseSize = 0;
			if (baseType) baseSize = baseType->getSize() / 8;  // Size in bytes
			int sz = pBF->getSizeByName(nam);
			if (sz && baseSize)
				// Note: since ty is a pointer and has not been cloned, this will also set the type for knownType
				ty->asArray()->setLength(sz / baseSize);
		}
	} else
		ty = guessGlobalType(nam, uaddr);

	globals.insert(new Global(ty, uaddr, nam));

	if (VERBOSE)
		LOG << "globalUsed: name " << nam
		    << ", address 0x" << std::hex << uaddr << std::dec
		    << (knownType ? ", known type " : ", guessed type ") << ty->getCtype() << "\n";
	return true;
}

const std::map<ADDRESS, std::string> &
Prog::getSymbols() const
{
	return pBF->getSymbols();
}

ArrayType *
Prog::makeArrayType(ADDRESS u, Type *t)
{
	auto nam = newGlobalName(u);
	int sz = pBF->getSizeByName(nam);
	if (sz == 0)
		return new ArrayType(t);  // An "unbounded" array
	int n = t->getBytes();
	if (n == 0) n = 1;
	return new ArrayType(t, sz / n);
}

Type *
Prog::guessGlobalType(const std::string &nam, ADDRESS u) const
{
	int sz = pBF->getSizeByName(nam);
	if (sz == 0) {
		// Check if it might be a string
		if (getStringConstant(u))
			// return char* and hope it is dealt with properly
			return new PointerType(new CharType());
	}
	switch (sz) {
	case 1: case 2: case 4: case 8:
		return new IntegerType(sz * 8);
	default:
		return new ArrayType(new CharType(), sz);
	}
}

std::string
Prog::newGlobalName(ADDRESS addr)
{
	if (auto nam = getGlobalName(addr))
		return std::string(nam);

	std::ostringstream os;
	os << "global" << globals.size();
	if (VERBOSE)
		LOG << "naming new global: " << os.str() << " at address 0x" << std::hex << addr << std::dec << "\n";
	return os.str();
}

Type *
Prog::getGlobalType(const std::string &nam) const
{
	for (const auto &global : globals)
		if (global->getName() == nam)
			return global->getType();
	return nullptr;
}

void
Prog::setGlobalType(const std::string &nam, Type *ty)
{
	// FIXME: inefficient
	for (const auto &global : globals) {
		if (global->getName() == nam) {
			global->setType(ty);
			return;
		}
	}
}

// get a string constant at a given address if appropriate
// if knownString, it is already known to be a char*
const char *
Prog::getStringConstant(ADDRESS uaddr, bool knownString /* = false */) const
{
	auto si = pBF->getSectionInfoByAddr(uaddr);
	// Too many compilers put constants, including string constants, into read/write sections
	//if (si && si->bReadOnly)
	if (si && !si->isAddressBss(uaddr)) {
		// At this stage, only support ascii, null terminated, non unicode strings.
		// At least 4 of the first 6 chars should be printable ascii
		const char *p = &si->uHostAddr[uaddr - si->uNativeAddr];
		if (knownString)
			// No need to guess... this is hopefully a known string
			return p;
		int printable = 0;
		char last = '\0';
		for (int i = 0; i < 6; ++i) {
			char c = p[i];
			if (c == '\0') break;
			if (c >= ' ' && c < '\x7F') ++printable;
			last = c;
		}
		if (printable >= 4)
			return p;
		// Just a hack while type propagations are not yet ready
		if (last == '\n' && printable >= 2)
			return p;
	}
	return nullptr;
}

double
Prog::getFloatConstant(ADDRESS uaddr, bool &ok, int bits) const
{
	ok = true;
	auto si = pBF->getSectionInfoByAddr(uaddr);
	if (si && si->bReadOnly) {
		if (bits == 64) {
			return pBF->readNativeFloat8(uaddr);
		} else {
			assert(bits == 32);
			return pBF->readNativeFloat4(uaddr);
		}
	}
	ok = false;
	return 0.0;
}

/*==============================================================================
 * FUNCTION:    Prog::findContainingProc
 * OVERVIEW:    Return a pointer to the Proc object containing uAddr, or 0 if none
 * NOTE:        Could return -1 for a deleted Proc
 * PARAMETERS:  Native address to search for
 * RETURNS:     Pointer to the Proc object, or 0 if none, or -1 if deleted
 *============================================================================*/
Proc *
Prog::findContainingProc(ADDRESS uAddr) const
{
	for (const auto &proc : m_procs) {
		if (proc->getNativeAddress() == uAddr)
			return proc;
		if (auto up = dynamic_cast<UserProc *>(proc))
			if (up->containsAddr(uAddr))
				return proc;
	}
	return nullptr;
}

/*==============================================================================
 * FUNCTION:    Prog::isProcLabel
 * OVERVIEW:    Return true if this is a real procedure
 * PARAMETERS:  Native address of the procedure entry point
 * RETURNS:     True if a real (non deleted) proc
 *============================================================================*/
bool
Prog::isProcLabel(ADDRESS addr)
{
	return !!m_procLabels[addr];
}

/*==============================================================================
 * FUNCTION:    Prog::getNameNoPath
 * OVERVIEW:    Get the name for the progam, without any path at the front
 * PARAMETERS:  None
 * RETURNS:     A string with the name
 *============================================================================*/
std::string
Prog::getNameNoPath() const
{
	auto n = m_name.rfind('/');
	if (n == m_name.npos) n = m_name.rfind('\\');
	if (n == m_name.npos)
		return m_name;
	return m_name.substr(n + 1);
}

std::string
Prog::getNameNoPathNoExt() const
{
	auto nopath = getNameNoPath();
	auto n = nopath.rfind('.');
	if (n == nopath.npos)
		return nopath;
	return nopath.substr(0, n);
}

/*==============================================================================
 * FUNCTION:    Prog::getFirstProc
 * OVERVIEW:    Return a pointer to the first Proc object for this program
 * NOTE:        The it parameter must be passed to getNextProc
 * PARAMETERS:  it: An uninitialised PROGMAP::const_iterator
 * RETURNS:     A pointer to the first Proc object; could be 0 if none
 *============================================================================*/
Proc *
Prog::getFirstProc(PROGMAP::const_iterator &it)
{
	it = m_procLabels.begin();
	while (it != m_procLabels.end() && (it->second == (Proc *)-1))
		++it;
	if (it != m_procLabels.end())
		return it->second;
	return nullptr;
}

/*==============================================================================
 * FUNCTION:    Prog::getNextProc
 * OVERVIEW:    Return a pointer to the next Proc object for this program
 * NOTE:        The it parameter must be from a previous call to getFirstProc or getNextProc
 * PARAMETERS:  it: A PROGMAP::const_iterator as above
 * RETURNS:     A pointer to the next Proc object; could be 0 if no more
 *============================================================================*/
Proc *
Prog::getNextProc(PROGMAP::const_iterator &it)
{
	++it;
	while (it != m_procLabels.end() && (it->second == (Proc *)-1))
		++it;
	if (it != m_procLabels.end())
		return it->second;
	return nullptr;
}

/*==============================================================================
 * FUNCTION:    Prog::getFirstUserProc
 * OVERVIEW:    Return a pointer to the first UserProc object for this program
 * NOTE:        The it parameter must be passed to getNextUserProc
 * PARAMETERS:  it: An uninitialised std::list<Proc*>::iterator
 * RETURNS:     A pointer to the first UserProc object; could be 0 if none
 *============================================================================*/
UserProc *
Prog::getFirstUserProc(std::list<Proc *>::iterator &it)
{
	for (it = m_procs.begin(); it != m_procs.end(); ++it)
		if (auto up = dynamic_cast<UserProc *>(*it))
			return up;
	return nullptr;
}

/*==============================================================================
 * FUNCTION:    Prog::getNextUserProc
 * OVERVIEW:    Return a pointer to the next UserProc object for this program
 * NOTE:        The it parameter must be from a previous call to
 *                getFirstUserProc or getNextUserProc
 * PARAMETERS:  it: A std::list<Proc*>::iterator
 * RETURNS:     A pointer to the next UserProc object; could be 0 if no more
 *============================================================================*/
UserProc *
Prog::getNextUserProc(std::list<Proc *>::iterator &it)
{
	for (++it; it != m_procs.end(); ++it)
		if (auto up = dynamic_cast<UserProc *>(*it))
			return up;
	return nullptr;
}

void
Prog::decodeEntryPoint(ADDRESS a)
{
	auto p = findProc(a);
	auto up = dynamic_cast<UserProc *>(p);
	if (!p || (up && !up->isDecoded())) {
		if (a < pBF->getLimitTextLow() || a >= pBF->getLimitTextHigh()) {
			std::cerr << "attempt to decode entrypoint at address outside text area, addr=0x" << std::hex << a << std::dec << "\n";
			if (VERBOSE)
				LOG << "attempt to decode entrypoint at address outside text area, addr=0x" << std::hex << a << std::dec << "\n";
			return;
		}
		pFE->decode(this, a);
		finishDecode();

		if (!p) {
			p = findProc(a);
			assert(p);
			up = dynamic_cast<UserProc *>(p);
		}
	}

	if (up)  // -sf procs marked as __nodecode are treated as library procs (?)
		entryProcs.push_back(up);
}

void
Prog::setEntryPoint(ADDRESS a)
{
	if (auto p = findProc(a))
		if (auto up = dynamic_cast<UserProc *>(p))
			entryProcs.push_back(up);
}

void
Prog::decodeEverythingUndecoded()
{
	for (const auto &proc : m_procs) {
		if (!proc) continue;  // Probably not needed
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (up->isDecoded()) continue;
			pFE->decode(this, up->getNativeAddress());
		}
	}
	finishDecode();
}

void
Prog::decompile()
{
	assert(!m_procs.empty());

	if (VERBOSE)
		LOG << m_procs.size() << " procedures\n";

	// Start decompiling each entry point
	for (const auto &proc : entryProcs) {
		std::cerr << "decompiling entry point " << proc->getName() << "\n";
		if (VERBOSE)
			LOG << "decompiling entry point " << proc->getName() << "\n";
		int indent = 0;
		proc->decompile(new ProcList, indent);
	}

	// Just in case there are any Procs not in the call graph.
	if (Boomerang::get()->decodeMain && !Boomerang::get()->noDecodeChildren) {
		bool foundone = true;
		while (foundone) {
			foundone = false;
			for (const auto &proc : m_procs) {
				if (auto up = dynamic_cast<UserProc *>(proc)) {
					if (up->isDecompiled()) continue;
					int indent = 0;
					up->decompile(new ProcList, indent);
					foundone = true;
				}
			}
		}
	}

	// Type analysis, if requested
	if (CON_TYPE_ANALYSIS && DFA_TYPE_ANALYSIS) {
		std::cerr << "can't use two types of type analysis at once!\n";
		CON_TYPE_ANALYSIS = false;
	}
	globalTypeAnalysis();


	if (!Boomerang::get()->noDecompile) {
		if (!Boomerang::get()->noRemoveReturns) {
			// A final pass to remove returns not used by any caller
			if (VERBOSE)
				LOG << "prog: global removing unused returns\n";
			// Repeat until no change. Note 100% sure if needed.
			while (removeUnusedReturns());
		}

		// print XML after removing returns
		for (const auto &proc : m_procs) {
			if (auto up = dynamic_cast<UserProc *>(proc)) {
				up->printXML();
			}
		}
	}

	if (VERBOSE)
		LOG << "transforming from SSA\n";

	// Now it is OK to transform out of SSA form
	fromSSAform();

	// Note: removeUnusedLocals() is now in UserProc::generateCode()

	removeUnusedGlobals();
}

void
Prog::removeUnusedGlobals()
{
	if (VERBOSE)
		LOG << "removing unused globals\n";

	// seach for used globals
	std::list<Exp *> usedGlobals;
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			Exp *search = new Location(opGlobal, new Terminal(opWild), up);
			// Search each statement in up, excepting implicit assignments (their uses don't count, since they don't really
			// exist in the program representation)
			StatementList stmts;
			up->getStatements(stmts);
			for (const auto &s : stmts) {
				if (dynamic_cast<ImplicitAssign *>(s)) continue;  // Ignore the uses in ImplicitAssigns
				bool found = s->searchAll(search, usedGlobals);
				if (found && DEBUG_UNUSED)
					LOG << " a global is used by stmt " << s->getNumber() << "\n";
			}
		}
	}

	// make a map to find a global by its name (could be a global var too)
	std::map<std::string, Global *> namedGlobals;
	for (const auto &global : globals)
		namedGlobals[global->getName()] = global;

	// rebuild the globals vector
	globals.clear();
	for (const auto &global : usedGlobals) {
		if (DEBUG_UNUSED)
			LOG << " " << *global << " is used\n";
		const char *name = ((Const *)global->getSubExp1())->getStr();
		Global *usedGlobal = namedGlobals[name];
		if (usedGlobal) {
			globals.insert(usedGlobal);
		} else {
			LOG << "warning: an expression refers to a nonexistent global\n";
		}
	}
}

// This is the global removing of unused and redundant returns. The initial idea is simple enough: remove some returns
// according to the formula returns(p) = modifieds(p) isect union(live at c) for all c calling p.
// However, removing returns reduces the uses, leading to three effects:
// 1) The statement that defines the return, if only used by that return, becomes unused
// 2) if the return is implicitly defined, then the parameters may be reduced, which affects all callers
// 3) if the return is defined at a call, the location may no longer be live at the call. If not, you need to check
//   the child, and do the union again (hence needing a list of callers) to find out if this change also affects that
//   child.
// Return true if any change
bool
Prog::removeUnusedReturns()
{
	// For each UserProc. Each proc may process many others, so this may duplicate some work. Really need a worklist of
	// procedures not yet processed.
	// Define a workset for the procedures who have to have their returns checked
	// This will be all user procs, except those undecoded (-sf says just trust the given signature)
	std::set<UserProc *> removeRetSet;
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;  // e.g. use -sf file to just prototype the proc
			removeRetSet.insert(up);
		}
	}
	// The workset is processed in arbitrary order. May be able to do better, but note that sometimes changes propagate
	// down the call tree (no caller uses potential returns for child), and sometimes up the call tree (removal of
	// returns and/or dead code removes parameters, which affects all callers).
	bool change = false;
	while (!removeRetSet.empty()) {
		auto it = removeRetSet.begin();  // Pick the first element of the set
		change |= (*it)->removeRedundantReturns(removeRetSet);
		// Note: removing the currently processed item here should prevent unnecessary reprocessing of self recursive
		// procedures
		removeRetSet.erase(it);  // Remove the current element (may no longer be the first)
	}
	return change;
}

// Have to transform out of SSA form after the above final pass
void
Prog::fromSSAform()
{
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (VERBOSE) {
				LOG << "===== before transformation from SSA form for " << up->getName() << " =====\n"
				    << *up
				    << "===== end before transformation from SSA for " << up->getName() << " =====\n\n";
				if (Boomerang::get()->dotFile)
					up->printDFG();
			}
			up->fromSSAform();
			if (VERBOSE) {
				LOG << "===== after transformation from SSA form for " << up->getName() << " =====\n"
				    << *up
				    << "===== end after transformation from SSA for " << up->getName() << " =====\n\n";
			}
		}
	}
}

void
Prog::conTypeAnalysis()
{
	if (VERBOSE || DEBUG_TA)
		LOG << "=== start constraint-based type analysis ===\n";
	// FIXME: This needs to be done bottom of the call-tree first, with repeat until no change for cycles
	// in the call graph
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			up->conTypeAnalysis();
		}
	}
	if (VERBOSE || DEBUG_TA)
		LOG << "=== end type analysis ===\n";
}

void
Prog::globalTypeAnalysis()
{
	if (VERBOSE || DEBUG_TA)
		LOG << "### start global data-flow-based type analysis ###\n";
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			// FIXME: this just does local TA again. Need to meet types for all parameter/arguments, and return/results!
			// This will require a repeat until no change loop
			std::cout << "global type analysis for " << up->getName() << "\n";
			up->typeAnalysis();
		}
	}
	if (VERBOSE || DEBUG_TA)
		LOG << "### end type analysis ###\n";
}

void
Prog::rangeAnalysis()
{
	for (const auto &proc : m_procs) {
		if (auto up = dynamic_cast<UserProc *>(proc)) {
			if (!up->isDecoded()) continue;
			up->rangeAnalysis();
			up->logSuspectMemoryDefs();
		}
	}
}

void
Prog::printCallGraph() const
{
	std::string fname1 = Boomerang::get()->getOutputPath() + "callgraph.out";
	std::string fname2 = Boomerang::get()->getOutputPath() + "callgraph.dot";
	int fd1 = lockFileWrite(fname1);
	int fd2 = lockFileWrite(fname2);
	std::ofstream f1(fname1);
	std::ofstream f2(fname2);
	std::set<Proc *> seen;
	std::map<Proc *, int> spaces;
	std::map<Proc *, Proc *> parent;
	std::list<Proc *> procList;
	f2 << "digraph callgraph {\n";
	procList.insert(procList.end(), entryProcs.begin(), entryProcs.end());
	spaces[procList.front()] = 0;
	while (!procList.empty()) {
		auto p = procList.front();
		procList.pop_front();
		if ((unsigned)p == NO_ADDRESS)
			continue;
		if (!seen.count(p)) {
			seen.insert(p);
			int n = spaces[p];
			for (int i = 0; i < n; ++i)
				f1 << "\t ";
			f1 << p->getName() << " @ " << std::hex << p->getNativeAddress() << std::dec;
			auto it = parent.find(p);
			if (it != parent.end())
				f1 << " [parent=" << it->second->getName() << "]";
			f1 << std::endl;
			if (auto up = dynamic_cast<UserProc *>(p)) {
				++n;
				const auto &calleeList = up->getCallees();
				for (auto it1 = calleeList.crbegin(); it1 != calleeList.crend(); ++it1) {
					procList.push_front(*it1);
					spaces[*it1] = n;
					parent[*it1] = p;
					f2 << p->getName() << " -> " << (*it1)->getName() << ";\n";
				}
			}
		}
	}
	f2 << "}\n";
	f1.close();
	f2.close();
	unlockFile(fd1);
	unlockFile(fd2);
}

static void
printProcsRecursive(Proc *proc, int indent, std::ofstream &f, std::set<Proc *> &seen)
{
	bool firsttime = false;
	if (!seen.count(proc)) {
		seen.insert(proc);
		firsttime = true;
	}
	for (int i = 0; i < indent; ++i)
		f << "\t";

	auto up = dynamic_cast<UserProc *>(proc);
	if (up && firsttime) { // seen lib proc
		f << "0x" << std::hex << proc->getNativeAddress() << std::dec;
		f << " __nodecode __incomplete void " << proc->getName() << "();\n";

		const auto &calleeList = up->getCallees();
		for (const auto &callee : calleeList) {
			printProcsRecursive(callee, indent + 1, f, seen);
		}
		for (int i = 0; i < indent; ++i)
			f << "\t";
		f << "// End of " << proc->getName() << "\n";
	} else {
		f << "// " << proc->getName() << "();\n";
	}
}

void
Prog::printSymbolsToFile() const
{
	std::cerr << "entering Prog::printSymbolsToFile\n";
	std::string fname = Boomerang::get()->getOutputPath() + "symbols.h";
	int fd = lockFileWrite(fname);
	std::ofstream f(fname);

	/* Print procs */
	f << "/* Functions: */\n";
	std::set<Proc *> seen;
	for (const auto &proc : entryProcs)
		printProcsRecursive(proc, 0, f, seen);

	f << "/* Leftovers: */\n"; // don't forget the rest
	for (const auto &proc : m_procs)
		if (dynamic_cast<UserProc *>(proc) && !seen.count(proc))
			printProcsRecursive(proc, 0, f, seen);

	f.close();
	unlockFile(fd);
	std::cerr << "leaving Prog::printSymbolsToFile\n";
}

void
Prog::printCallGraphXML() const
{
	if (!DUMP_XML)
		return;
	for (const auto &proc : m_procs)
		proc->clearVisited();
	std::string fname = Boomerang::get()->getOutputPath() + "callgraph.xml";
	int fd = lockFileWrite(fname);
	std::ofstream f(fname);
	f << "<prog name=\"" << getName() << "\">\n";
	f << "\t<callgraph>\n";
	for (const auto &proc : entryProcs)
		proc->printCallGraphXML(f, 2);
	for (const auto &proc : m_procs)
		if (dynamic_cast<UserProc *>(proc) && !proc->isVisited())
			proc->printCallGraphXML(f, 2);
	f << "\t</callgraph>\n";
	f << "</prog>\n";
	f.close();
	unlockFile(fd);
}

void
Prog::readSymbolFile(const std::string &fname)
{
	std::ifstream ifs(fname);
	if (!ifs.good()) {
		LOG << "can't open `" << fname << "'\n";
		exit(1);
	}

	AnsiCParser par(ifs, false);
	platform plat = getFrontEndId();
	callconv cc = CONV_C;
	if (isWin32()) cc = CONV_PASCAL;
	par.yyparse(plat, cc);
	ifs.close();

	for (const auto &sym : par.symbols) {
		if (sym->sig) {
			Proc *p = newProc(sym->sig->getName(), sym->addr,
			                  pBF->isDynamicLinkedProcPointer(sym->addr)
			                  // NODECODE isn't really the right modifier; perhaps we should have a LIB modifier,
			                  // to specifically specify that this function obeys library calling conventions
			               || sym->mods->noDecode);
			if (!sym->mods->incomplete) {
				p->setSignature(sym->sig->clone());
				p->getSignature()->setForced(true);
			}
		} else {
			auto nam = sym->nam;
			if (nam.empty()) {
				nam = newGlobalName(sym->addr);
			}
			Type *ty = sym->ty;
			if (!ty) {
				ty = guessGlobalType(nam, sym->addr);
			}
			globals.insert(new Global(ty, sym->addr, nam));
		}
	}

	for (const auto &ref : par.refs) {
		pFE->addRefHint(ref->addr, ref->nam);
	}
}

Global::~Global()
{
	// Do-nothing d'tor
}

Exp *
Global::getInitialValue(Prog *prog) const
{
	auto si = prog->getSectionInfoByAddr(uaddr);
	if (!si || si->isAddressBss(uaddr))
		// This global is in the BSS, so it can't be initialised
		return nullptr;
	return prog->readNativeAs(uaddr, type);
}

void
Global::print(std::ostream &os, Prog *prog) const
{
	Exp *init = getInitialValue(prog);
	os << type << " " << nam << " at " << std::hex << uaddr << std::dec
	   << " initial value " << (init ? init->prints() : "<none>");
}

Exp *
Prog::readNativeAs(ADDRESS uaddr, Type *type) const
{
	Exp *e = nullptr;
	auto si = pBF->getSectionInfoByAddr(uaddr);
	if (!si)
		return nullptr;
	if (type->resolvesToPointer()) {
		ADDRESS init = pBF->readNative4(uaddr);
		if (init == 0)
			return new Const(0);
		if (auto nam = getGlobalName(init))
			// TODO: typecast?
			return Location::global(nam, nullptr);
		if (type->asPointer()->getPointsTo()->resolvesToChar()) {
			if (auto str = getStringConstant(init))
				return new Const(str);
		}
	}
	if (type->resolvesToCompound()) {
		CompoundType *c = type->asCompound();
		Exp *n = e = new Terminal(opNil);
		for (unsigned int i = 0; i < c->getNumTypes(); ++i) {
			ADDRESS addr = uaddr + c->getOffsetTo(i) / 8;
			Type *t = c->getType(i);
			Exp *v = readNativeAs(addr, t);
			if (!v) {
				LOG << "unable to read native address 0x" << std::hex << addr << std::dec << " as type " << t->getCtype() << "\n";
				v = new Const(-1);
			}
			if (n->isNil()) {
				n = new Binary(opList, v, n);
				e = n;
			} else {
				assert(n->getSubExp2()->isNil());
				n->setSubExp2(new Binary(opList, v, n->getSubExp2()));
				n = n->getSubExp2();
			}
		}
		return e;
	}
	if (type->resolvesToArray() && type->asArray()->getBaseType()->resolvesToChar()) {
		if (auto str = getStringConstant(uaddr, true)) {
			// Make a global string
			return new Const(str);
		}
	}
	if (type->resolvesToArray()) {
		int nelems = -1;
		int base_sz = type->asArray()->getBaseType()->getSize() / 8;
		if (auto nam = getGlobalName(uaddr))
			nelems = pBF->getSizeByName(nam) / base_sz;
		Exp *n = e = new Terminal(opNil);
		for (int i = 0; nelems == -1 || i < nelems; ++i) {
			Exp *v = readNativeAs(uaddr + i * base_sz, type->asArray()->getBaseType());
			if (!v)
				break;
			if (n->isNil()) {
				n = new Binary(opList, v, n);
				e = n;
			} else {
				assert(n->getSubExp2()->isNil());
				n->setSubExp2(new Binary(opList, v, n->getSubExp2()));
				n = n->getSubExp2();
			}
			// "null" terminated
			if (nelems == -1 && v->isConst() && ((Const *)v)->getInt() == 0)
				break;
		}
	}
	if (type->resolvesToInteger() || type->resolvesToSize()) {
		int size;
		if (type->resolvesToInteger())
			size = type->asInteger()->getSize();
		else
			size = type->asSize()->getSize();
		switch (size) {
		case 8:
			e = new Const((int)si->uHostAddr[uaddr - si->uNativeAddr]);
			break;
		case 16:
			// Note: must respect endianness
			e = new Const(pBF->readNative2(uaddr));
			break;
		case 32:
			e = new Const(pBF->readNative4(uaddr));
			break;
		case 64:
			e = new Const(pBF->readNative8(uaddr));
			break;
		}
	}
	if (type->resolvesToFloat()) {
		switch (type->asFloat()->getSize()) {
		case 32:
			e = new Const(pBF->readNativeFloat4(uaddr));
			break;
		case 64:
			e = new Const(pBF->readNativeFloat8(uaddr));
			break;
		}
	}
	return e;
}

void
Global::meetType(Type *ty)
{
	bool ch;
	type = type->meetWith(ty, ch);
}

void
Prog::reDecode(UserProc *proc)
{
	pFE->processProc(proc->getNativeAddress(), proc);
}

#ifdef USING_MEMO
class ClusterMemo : public Memo {
public:
	ClusterMemo(int mId) : Memo(mId) { }

	std::string name;
	std::vector<Cluster *> children;
	Cluster *parent;
};

Memo *
Cluster::makeMemo(int mId)
{
	auto m = new ClusterMemo(mId);
	m->name = name;
	m->children = children;
	m->parent = parent;
	return m;
}

void
Cluster::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<ClusterMemo *>(mm);

	name = m->name;
	children = m->children;
	parent = m->parent;

	for (const auto &child : children)
		child->restoreMemo(m->mId, dec);
}

class GlobalMemo : public Memo {
public:
	GlobalMemo(int mId) : Memo(mId) { }

	Type *type;
	ADDRESS uaddr;
	std::string nam;
};

Memo *
Global::makeMemo(int mId)
{
	auto m = new GlobalMemo(mId);
	m->type = type;
	m->uaddr = uaddr;
	m->nam = nam;

	type->takeMemo(mId);
	return m;
}

void
Global::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<GlobalMemo *>(mm);

	type = m->type;
	uaddr = m->uaddr;
	nam = m->nam;

	type->restoreMemo(m->mId, dec);
}

class ProgMemo : public Memo {
public:
	ProgMemo(int m) : Memo(m) { }

	std::string m_name, m_path;
	std::list<Proc *> m_procs;
	PROGMAP m_procLabels;
	std::set<Global *> globals;
	DataIntervalMap globalMap;
	int m_iNumberedProc;
	Cluster *m_rootCluster;
};

Memo *
Prog::makeMemo(int mId)
{
	auto m = new ProgMemo(mId);
	m->m_name = m_name;
	m->m_path = m_path;
	m->m_procs = m_procs;
	m->m_procLabels = m_procLabels;
	m->globals = globals;
	m->globalMap = globalMap;
	m->m_iNumberedProc = m_iNumberedProc;
	m->m_rootCluster = m_rootCluster;

	for (const auto &proc : m_procs)
		proc->takeMemo(m->mId);
	m_rootCluster->takeMemo(m->mId);
	for (const auto &global : globals)
		global->takeMemo(m->mId);

	return m;
}

void
Prog::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<ProgMemo *>(mm);
	m_name = m->m_name;
	m_path = m->m_path;
	m_procs = m->m_procs;
	m_procLabels = m->m_procLabels;
	globals = m->globals;
	globalMap = m->globalMap;
	m_iNumberedProc = m->m_iNumberedProc;
	m_rootCluster = m->m_rootCluster;

	for (const auto &proc : m_procs)
		proc->restoreMemo(m->mId, dec);
	m_rootCluster->restoreMemo(m->mId, dec);
	for (const auto &global : globals)
		global->restoreMemo(m->mId, dec);
}

/*
    After every undoable operation:
                                         ? ||
        prog->takeMemo();                1 |.1|
        always deletes any memos before the cursor, adds new memo leaving cursor pointing to new memo

    For first undo:
                                         ? |.4321|
        prog->restoreMemo(inc);          4 |5.4321|
        takes a memo of previous state, always leaves cursor pointing at current state

    For second undo:
                                         4 |5.4321|
        prog->restoreMemo(inc);          3 |54.321|

    To redo:
                                         3  |54.321|
        prog->restoreMemo(dec);          4  |5.4321|
 */

void
Memoisable::takeMemo(int mId)
{
	if (cur_memo != memos.end() && (*cur_memo)->mId == mId && mId != -1)
		return;

	if (cur_memo != memos.begin()) {
		auto it = memos.begin();
		while (it != cur_memo)
			it = memos.erase(it);
	}

	if (mId == -1) {
		if (cur_memo == memos.end())
			mId = 1;
		else
			mId = memos.front()->mId + 1;
	}

	Memo *m = makeMemo(mId);

	memos.push_front(m);
	cur_memo = memos.begin();
}

void
Memoisable::restoreMemo(int mId, bool dec)
{
	if (memos.begin() == memos.end())
		return;

	if ((*cur_memo)->mId == mId && mId != -1)
		return;

	if (dec) {
		if (cur_memo == memos.begin())
			return;
		--cur_memo;
	} else {
		++cur_memo;
		if (cur_memo == memos.end()) {
			--cur_memo;
			return;
		}
	}

	Memo *m = *cur_memo;
	if (m->mId != mId && mId != -1)
		return;

	readMemo(m, dec);
}

bool
Memoisable::canRestore(bool dec)
{
	if (memos.begin() == memos.end())
		return false;

	if (dec) {
		if (cur_memo == memos.begin())
			return false;
	} else {
		++cur_memo;
		if (cur_memo == memos.end()) {
			--cur_memo;
			return false;
		}
		--cur_memo;
	}
	return true;
}

void
Memoisable::takeMemo()
{
	takeMemo(-1);
}

void
Memoisable::restoreMemo(bool dec)
{
	restoreMemo(-1, dec);
}
#endif

void
Prog::decodeFragment(UserProc *proc, ADDRESS a)
{
	if (a >= pBF->getLimitTextLow() && a < pBF->getLimitTextHigh())
		pFE->decodeFragment(proc, a);
	else {
		std::cerr << "attempt to decode fragment outside text area, addr=0x" << std::hex << a << std::dec << "\n";
		if (VERBOSE)
			LOG << "attempt to decode fragment outside text area, addr=0x" << std::hex << a << std::dec << "\n";
	}
}

Exp *
Prog::addReloc(Exp *e, ADDRESS lc)
{
	assert(e->isConst());
	Const *c = (Const *)e;

	// relocations have been applied to the constant, so if there is a
	// relocation for this lc then we should be able to replace the constant
	// with a symbol.

	if (pBF->isRelocationAt(lc)) {
		ADDRESS a = c->getInt();  // FIXME: Why not c->getAddr()?
		const auto &symbols = pBF->getSymbols();
		auto sym = symbols.find(a);
		if (sym != symbols.end()) {
			const auto &n = sym->second;
			unsigned int sz = pBF->getSizeByName(n);
			if (!getGlobal(n))
				globals.insert(new Global(new SizeType(sz * 8), a, n));
			e = new Unary(opAddrOf, Location::global(n, nullptr));
		} else {
			if (auto str = getStringConstant(a)) {
				e = new Const(str);
			} else {
				// check for accesses into the middle of symbols
				for (const auto &symbol : symbols) {
					if (a > symbol.first
					 && a < symbol.first + pBF->getSizeByName(symbol.second)) {
						int off = a - symbol.first;
						e = new Binary(opPlus,
						               new Unary(opAddrOf, Location::global(symbol.second, nullptr)),
						               new Const(off));
						break;
					}
				}
			}
		}
	}
	return e;
}
