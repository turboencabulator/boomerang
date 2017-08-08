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
#include "log.h"
#include "managed.h"
#include "proc.h"
#include "signature.h"
#include "statement.h"
#include "type.h"
#include "types.h"
#include "util.h"       // For lockFileWrite etc

#include <sys/stat.h>   // For mkdir
#include <sys/types.h>

#include <fstream>
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
		m_rootCluster = new Cluster(getNameNoPathNoExt().c_str());
	}
}

Prog::Prog(const char *name) :
	m_name(name),
	m_rootCluster(new Cluster(getNameNoPathNoExt().c_str()))
{
	// Constructor taking a name. Technically, the allocation of the space for the name could fail, but this is unlikely
	m_path = m_name;
}

Prog::~Prog()
{
	if (pFE) FrontEnd::close(pFE);
	if (pBF) BinaryFile::close(pBF);
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		delete *it;
	}
	m_procs.clear();
}

void
Prog::setName(const char *name)  // Assign a name to this program
{
	m_name = name;
	m_rootCluster->setName(name);
}

// well form the entire program
bool
Prog::wellForm()
{
	bool wellformed = true;

	for (auto it = m_procs.begin(); it != m_procs.end(); ++it)
		if (!(*it)->isLib()) {
			UserProc *u = (UserProc *)*it;
			wellformed &= u->getCFG()->wellFormCfg();
		}
	return wellformed;
}

// last fixes after decoding everything
// was in analysis.cpp
void
Prog::finishDecode()
{
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		Proc *pProc = *it;

		if (pProc->isLib()) continue;
		UserProc *p = (UserProc *)pProc;
		if (!p->isDecoded()) continue;

		p->assignProcsToCalls();
		p->finalSimplify();
	}
}

void
Prog::generateDot(std::ostream &os)
{
	os << "digraph Cfg {\n";

	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		Proc *pProc = *it;
		if (pProc->isLib()) continue;
		UserProc *p = (UserProc *)pProc;
		if (!p->isDecoded()) continue;
		// Subgraph for the proc name
		os << "\n\tsubgraph cluster_" << p->getName() << " {\n"
		   << "\t\tcolor=gray;\n\t\tlabel=\"" << p->getName() << "\";\n";
		// Generate dotty CFG for this proc
		p->getCFG()->generateDot(os);
	}

	os << "}\n";
}

void
Prog::generateCode(Cluster *cluster, UserProc *proc, bool intermixRTL)
{
	std::string basedir = m_rootCluster->makeDirs();
	std::ofstream os;
	if (cluster) {
		cluster->openStream("c");
		cluster->closeStreams();
	}
	if (!cluster || cluster == m_rootCluster) {
		os.open(m_rootCluster->getOutPath("c"));
		if (!proc) {
			HLLCode *code = Boomerang::get()->getHLLCode();
			bool global = false;
			if (Boomerang::get()->noDecompile) {
				const char *sections[] = { "rodata", "data", "data1", 0 };
				for (int j = 0; sections[j]; ++j) {
					std::string str = ".";
					str += sections[j];
					const SectionInfo *info = pBF->getSectionInfoByName(str.c_str());
					str = "start_";
					str += sections[j];
					code->AddGlobal(str.c_str(), new IntegerType(32, -1), new Const(info ? info->uNativeAddr : (unsigned int)-1));
					str = sections[j];
					str += "_size";
					code->AddGlobal(str.c_str(), new IntegerType(32, -1), new Const(info ? info->uSectionSize : (unsigned int)-1));
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
				global = true;
			}
			for (auto it1 = globals.begin(); it1 != globals.end(); ++it1) {
				// Check for an initial value
				Exp *e = nullptr;
				e = (*it1)->getInitialValue(this);
				//if (e) {
					code->AddGlobal((*it1)->getName(), (*it1)->getType(), e);
					global = true;
				//}
			}
			if (global) code->print(os);  // Avoid blank line if no globals
		}
	}

	// First declare prototypes for all but the first proc
	bool first = true, proto = false;
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		if ((*it)->isLib()) continue;
		if (first) {
			first = false;
			continue;
		}
		proto = true;
		UserProc *up = (UserProc *)*it;
		HLLCode *code = Boomerang::get()->getHLLCode(up);
		code->AddPrototype(up);  // May be the wrong signature if up has ellipsis
		if (!cluster || cluster == m_rootCluster)
			code->print(os);
	}
	if ((proto && !cluster) || cluster == m_rootCluster)
		os << "\n";  // Separate prototype(s) from first proc

	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		Proc *pProc = *it;
		if (pProc->isLib()) continue;
		UserProc *up = (UserProc *)pProc;
		if (!up->isDecoded()) continue;
		if (proc && up != proc)
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
	os.close();
	m_rootCluster->closeStreams();
}

void
Prog::generateRTL(Cluster *cluster, UserProc *proc)
{
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		Proc *pProc = *it;
		if (pProc->isLib()) continue;
		UserProc *p = (UserProc *)pProc;
		if (!p->isDecoded()) continue;
		if (proc && p != proc)
			continue;
		if (cluster && p->getCluster() != cluster)
			continue;

		p->getCluster()->openStream("rtl");
		p->print(p->getCluster()->getStream());
	}
	m_rootCluster->closeStreams();
}

Statement *
Prog::getStmtAtLex(Cluster *cluster, unsigned int begin, unsigned int end)
{
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		Proc *pProc = *it;
		if (pProc->isLib()) continue;
		UserProc *p = (UserProc *)pProc;
		if (!p->isDecoded()) continue;
		if (cluster && p->getCluster() != cluster)
			continue;

		if (p->getCluster() == cluster) {
			Statement *s = p->getStmtAtLex(begin, end);
			if (s)
				return s;
		}
	}
	return nullptr;
}

const char *
Cluster::makeDirs()
{
	std::string path;
	if (parent)
		path = parent->makeDirs();
	else
		path = Boomerang::get()->getOutputPath();
	if (getNumChildren() > 0 || !parent) {
		path = path + "/" + name;
		mkdir(path.c_str(), 0777);
	}
	return strdup(path.c_str());
}

void
Cluster::removeChild(Cluster *n)
{
	auto it = children.begin();
	for (; it != children.end(); ++it)
		if (*it == n)
			break;
	assert(it != children.end());
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
Cluster::find(const char *nam)
{
	if (name == nam)
		return this;
	for (unsigned i = 0; i < children.size(); ++i) {
		Cluster *c = children[i]->find(nam);
		if (c)
			return c;
	}
	return nullptr;
}

const char *
Cluster::getOutPath(const char *ext)
{
	std::string basedir = makeDirs();
	// Ugh - should probably return a whole std::string
	return strdup((basedir + "/" + name + "." + ext).c_str());
}

void
Cluster::openStream(const char *ext)
{
	if (out.is_open())
		return;
	out.open(getOutPath(ext));
	stream_ext = ext;
	if (!strcmp(ext, "xml")) {
		out << "<?xml version=\"1.0\"?>\n";
		if (parent)
			out << "<procs>\n";
	}
}

void
Cluster::openStreams(const char *ext)
{
	openStream(ext);
	for (unsigned i = 0; i < children.size(); ++i)
		children[i]->openStreams(ext);
}

void
Cluster::closeStreams()
{
	if (out.is_open()) {
		out.close();
	}
	for (unsigned i = 0; i < children.size(); ++i)
		children[i]->closeStreams();
}

bool
Prog::clusterUsed(Cluster *c)
{
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it)
		if ((*it)->getCluster() == c)
			return true;
	return false;
}

Cluster *
Prog::getDefaultCluster(const char *name)
{
	const char *cfname = nullptr;
	if (pBF) cfname = pBF->getFilenameSymbolFor(name);
	if (!cfname)
		return m_rootCluster;
	if (strcmp(cfname + strlen(cfname) - 2, ".c"))
		return m_rootCluster;
	LOG << "got filename " << cfname << " for " << name << "\n";
	char *fname = strdup(cfname);
	fname[strlen(fname) - 2] = 0;
	Cluster *c = findCluster(fname);
	if (!c) {
		c = new Cluster(fname);
		m_rootCluster->addChild(c);
	}
	return c;
}

void
Prog::generateCode(std::ostream &os)
{
	HLLCode *code = Boomerang::get()->getHLLCode();
	for (auto it1 = globals.begin(); it1 != globals.end(); ++it1) {
		// Check for an initial value
		Exp *e = nullptr;
		e = (*it1)->getInitialValue(this);
		if (e)
			code->AddGlobal((*it1)->getName(), (*it1)->getType(), e);
	}
	code->print(os);
	delete code;
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		Proc *pProc = *it;
		if (pProc->isLib()) continue;
		UserProc *p = (UserProc *)pProc;
		if (!p->isDecoded()) continue;
		p->getCFG()->compressCfg();
		code = Boomerang::get()->getHLLCode(p);
		p->generateCode(code);
		code->print(os);
		delete code;
	}
}

// Print this program, mainly for debugging
void
Prog::print(std::ostream &out)
{
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		Proc *pProc = *it;
		if (pProc->isLib()) continue;
		UserProc *p = (UserProc *)pProc;
		if (!p->isDecoded()) continue;

		// decoded userproc.. print it
		p->print(out);
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
	const char *pName = pBF->getSymbolByAddress(uAddr);
	bool bLib = pBF->isDynamicLinkedProc(uAddr) | pBF->isStaticLinkedLibProc(uAddr);
	if (!pName) {
		// No name. Give it a numbered name
		std::ostringstream ost;
		ost << "proc" << m_iNumberedProc++;
		pName = strdup(ost.str().c_str());
		if (VERBOSE)
			LOG << "assigning name " << pName << " to addr " << uAddr << "\n";
	}
	pProc = newProc(pName, uAddr, bLib);
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
Prog::newProc(const char *name, ADDRESS uNative, bool bLib /*= false*/)
{
	Proc *pProc;
	std::string sname(name);
	if (bLib)
		pProc = new LibProc(this, sname, uNative);
	else
		pProc = new UserProc(this, sname, uNative);

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

	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		if (*it == uProc) {
			m_procs.erase(it);
			break;
		}
	}

	// Delete the UserProc object as well
	delete uProc;
}

void
Prog::removeProc(const char *name)
{
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it)
		if (std::string(name) == (*it)->getName()) {
			Boomerang::get()->alert_remove(*it);
			m_procs.erase(it);
			break;
		}
}

/*==============================================================================
 * FUNCTION:    Prog::getNumProcs
 * OVERVIEW:    Return the number of real (non deleted) procedures
 * RETURNS:     The number of procedures
 *============================================================================*/
int
Prog::getNumProcs()
{
	return m_procs.size();
}

int
Prog::getNumUserProcs()
{
	int n = 0;
	for (auto it = m_procs.cbegin(); it != m_procs.cend(); ++it)
		if (!(*it)->isLib())
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
	for (int i = 0; i < idx; ++i)
		++it;
	return (*it);
}

/*==============================================================================
 * FUNCTION:    Prog::findProc
 * OVERVIEW:    Return a pointer to the associated Proc object, or nullptr if none
 * NOTE:        Could return -1 for a deleted Proc
 * PARAMETERS:  Native address of the procedure entry point
 * RETURNS:     Pointer to the Proc object, or 0 if none, or -1 if deleted
 *============================================================================*/
Proc *
Prog::findProc(ADDRESS uAddr) const
{
	auto it = m_procLabels.find(uAddr);
	if (it == m_procLabels.cend())
		return nullptr;
	else
		return it->second;
}

Proc *
Prog::findProc(const char *name) const
{
	for (auto it = m_procs.cbegin(); it != m_procs.cend(); ++it)
		if (!strcmp((*it)->getName(), name))
			return *it;
	return nullptr;
}

// get a library procedure by name; create if does not exist
LibProc *
Prog::getLibraryProc(const char *nam)
{
	Proc *p = findProc(nam);
	if (p && p->isLib())
		return (LibProc *)p;
	return (LibProc *)newProc(nam, NO_ADDRESS, true);
}

Signature *
Prog::getLibSignature(const char *nam)
{
	return pFE->getLibSignature(nam);
}

void
Prog::rereadLibSignatures()
{
	pFE->readLibraryCatalog();
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		if ((*it)->isLib()) {
			(*it)->setSignature(getLibSignature((*it)->getName()));
			std::set<CallStatement *> &callers = (*it)->getCallers();
			for (auto it1 = callers.begin(); it1 != callers.end(); ++it1)
				(*it1)->setSigArguments();
			Boomerang::get()->alert_update_signature(*it);
		}
	}
}

platform
Prog::getFrontEndId()
{
	return pFE->getFrontEndId();
}

Signature *
Prog::getDefaultSignature(const char *name)
{
	return pFE->getDefaultSignature(name);
}

std::vector<Exp *> &
Prog::getDefaultParams()
{
	return pFE->getDefaultParams();
}

std::vector<Exp *> &
Prog::getDefaultReturns()
{
	return pFE->getDefaultReturns();
}

bool
Prog::isWin32()
{
	return pFE->isWin32();
}

const char *
Prog::getGlobalName(ADDRESS uaddr)
{
	// FIXME: inefficient
	for (auto it = globals.begin(); it != globals.end(); ++it) {
		if ((*it)->getAddress() == uaddr)
			return (*it)->getName();
		else if ((*it)->getAddress() < uaddr
		      && (*it)->getAddress() + (*it)->getType()->getSize() / 8 > uaddr)
			return (*it)->getName();
	}
	if (pBF)
		return pBF->getSymbolByAddress(uaddr);
	return nullptr;
}

void
Prog::dumpGlobals()
{
	for (auto it = globals.begin(); it != globals.end(); ++it) {
		(*it)->print(std::cerr, this);
		std::cerr << "\n";
	}
}

ADDRESS
Prog::getGlobalAddr(const char *nam)
{
	for (auto it = globals.begin(); it != globals.end(); ++it) {
		if (!strcmp((*it)->getName(), nam))
			return (*it)->getAddress();
	}
	return pBF->getAddressByName(nam);
}

Global *
Prog::getGlobal(const char *nam)
{
	for (auto it = globals.begin(); it != globals.end(); ++it) {
		if (!strcmp((*it)->getName(), nam))
			return *it;
	}
	return nullptr;
}

bool
Prog::globalUsed(ADDRESS uaddr, Type *knownType)
{
	Global *global;

	for (auto it = globals.begin(); it != globals.end(); ++it) {
		if ((*it)->getAddress() == uaddr) {
			if (knownType) (*it)->meetType(knownType);
			return true;
		} else if ((*it)->getAddress() < uaddr && (*it)->getAddress() + (*it)->getType()->getSize() / 8 > uaddr) {
			if (knownType) (*it)->meetType(knownType);
			return true;
		}
	}

	if (!pBF->getSectionInfoByAddr(uaddr)) {
		if (VERBOSE)
			LOG << "refusing to create a global at address that is in no known section of the binary: " << uaddr << "\n";
		return false;
	}

	const char *nam = newGlobalName(uaddr);
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

	global = new Global(ty, uaddr, nam);
	globals.insert(global);

	if (VERBOSE) {
		LOG << "globalUsed: name " << nam << ", address " << uaddr;
		if (knownType)
			LOG << ", known type " << ty->getCtype() << "\n";
		else
			LOG << ", guessed type " << ty->getCtype() << "\n";
	}
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
	const char *nam = newGlobalName(u);
	int sz = pBF->getSizeByName(nam);
	if (sz == 0)
		return new ArrayType(t);  // An "unbounded" array
	int n = t->getSize() / 8;
	if (n == 0) n = 1;
	return new ArrayType(t, sz / n);
}

Type *
Prog::guessGlobalType(const char *nam, ADDRESS u)
{
	int sz = pBF->getSizeByName(nam);
	if (sz == 0) {
		// Check if it might be a string
		const char *str = getStringConstant(u);
		if (str)
			// return char* and hope it is dealt with properly
			return new PointerType(new CharType());
	}
	Type *ty;
	switch (sz) {
	case 1: case 2: case 4: case 8:
		ty = new IntegerType(sz * 8);
		break;
	default:
		ty = new ArrayType(new CharType(), sz);
	}
	return ty;
}

const char *
Prog::newGlobalName(ADDRESS uaddr)
{
	const char *nam = getGlobalName(uaddr);
	if (!nam) {
		std::ostringstream os;
		os << "global" << globals.size();
		nam = strdup(os.str().c_str());
		if (VERBOSE)
			LOG << "naming new global: " << nam << " at address " << uaddr << "\n";
	}
	return nam;
}

Type *
Prog::getGlobalType(const char *nam)
{
	for (auto it = globals.begin(); it != globals.end(); ++it)
		if (!strcmp((*it)->getName(), nam))
			return (*it)->getType();
	return nullptr;
}

void
Prog::setGlobalType(const char *nam, Type *ty)
{
	// FIXME: inefficient
	for (auto it = globals.begin(); it != globals.end(); ++it) {
		if (!strcmp((*it)->getName(), nam)) {
			(*it)->setType(ty);
			return;
		}
	}
}

// get a string constant at a given address if appropriate
// if knownString, it is already known to be a char*
const char *
Prog::getStringConstant(ADDRESS uaddr, bool knownString /* = false */)
{
	const SectionInfo *si = pBF->getSectionInfoByAddr(uaddr);
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
		char last = 0;
		for (int i = 0; i < 6; ++i) {
			char c = p[i];
			if (c == 0) break;
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
Prog::getFloatConstant(ADDRESS uaddr, bool &ok, int bits)
{
	ok = true;
	const SectionInfo *si = pBF->getSectionInfoByAddr(uaddr);
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
	for (auto it = m_procs.cbegin(); it != m_procs.cend(); ++it) {
		Proc *p = (*it);
		if (p->getNativeAddress() == uAddr)
			return p;
		if (p->isLib()) continue;

		UserProc *u = (UserProc *)p;
		if (u->containsAddr(uAddr))
			return p;
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
	std::string::size_type n = m_name.rfind('/');
	if (n == m_name.npos) n = m_name.rfind('\\');
	if (n == m_name.npos)
		return m_name;
	return m_name.substr(n + 1);
}

std::string
Prog::getNameNoPathNoExt() const
{
	std::string nopath = getNameNoPath();
	std::string::size_type n = nopath.rfind('.');
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
	if (it == m_procLabels.end())
		return nullptr;
	return it->second;
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
	if (it == m_procLabels.end())
		return nullptr;
	return it->second;
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
	it = m_procs.begin();
	while (it != m_procs.end() && (*it)->isLib())
		++it;
	if (it == m_procs.end())
		return nullptr;
	return (UserProc *)*it;
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
	++it;
	while (it != m_procs.end() && (*it)->isLib())
		++it;
	if (it == m_procs.end())
		return nullptr;
	return (UserProc *)*it;
}

/*==============================================================================
 * FUNCTION:    getCodeInfo
 * OVERVIEW:    Lookup the given native address in the code section, returning a host pointer corresponding to the same
 *               address
 * PARAMETERS:  uNative: Native address of the candidate string or constant
 *              last: will be set to one past end of the code section (host)
 *              delta: will be set to the difference between the host and native addresses
 * RETURNS:     Host pointer if in range; nullptr if not
 *              Also sets 2 reference parameters (see above)
 *============================================================================*/
const void *
Prog::getCodeInfo(ADDRESS uAddr, const char *&last, ptrdiff_t &delta)
{
	delta = 0;
	last = nullptr;
	int n = pBF->getNumSections();
	// Search all code and read-only sections
	for (int i = 0; i < n; ++i) {
		const SectionInfo *pSect = pBF->getSectionInfo(i);
		if ((!pSect->bCode) && (!pSect->bReadOnly))
			continue;
		if ((uAddr < pSect->uNativeAddr) || (uAddr >= pSect->uNativeAddr + pSect->uSectionSize))
			continue;  // Try the next section
		delta = pSect->uHostAddr - (char *)pSect->uNativeAddr;
		last = pSect->uHostAddr + pSect->uSectionSize;
		return (const void *)(uAddr + delta);
	}
	return nullptr;
}

void
Prog::decodeEntryPoint(ADDRESS a)
{
	Proc *p = (UserProc *)findProc(a);
	if (!p || (!p->isLib() && !((UserProc *)p)->isDecoded())) {
		if (a < pBF->getLimitTextLow() || a >= pBF->getLimitTextHigh()) {
			std::cerr << "attempt to decode entrypoint at address outside text area, addr=" << a << "\n";
			if (VERBOSE)
				LOG << "attempt to decode entrypoint at address outside text area, addr=" << a << "\n";
			return;
		}
		pFE->decode(this, a);
		finishDecode();
	}
	if (!p)
		p = findProc(a);
	assert(p);
	if (!p->isLib())  // -sf procs marked as __nodecode are treated as library procs (?)
		entryProcs.push_back((UserProc *)p);
}

void
Prog::setEntryPoint(ADDRESS a)
{
	Proc *p = (UserProc *)findProc(a);
	if (p && !p->isLib())
		entryProcs.push_back((UserProc *)p);
}

void
Prog::decodeEverythingUndecoded()
{
	for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
		UserProc *up = (UserProc *)*pp;
		if (!up) continue;  // Probably not needed
		if (up->isLib()) continue;
		if (up->isDecoded()) continue;
		pFE->decode(this, up->getNativeAddress());
	}
	finishDecode();
}

void
Prog::decompile()
{
	assert(!m_procs.empty());

	if (VERBOSE)
		LOG << (int)m_procs.size() << " procedures\n";

	// Start decompiling each entry point
	for (auto ee = entryProcs.begin(); ee != entryProcs.end(); ++ee) {
		std::cerr << "decompiling entry point " << (*ee)->getName() << "\n";
		if (VERBOSE)
			LOG << "decompiling entry point " << (*ee)->getName() << "\n";
		int indent = 0;
		(*ee)->decompile(new ProcList, indent);
	}

	// Just in case there are any Procs not in the call graph.
	if (Boomerang::get()->decodeMain && !Boomerang::get()->noDecodeChildren) {
		bool foundone = true;
		while (foundone) {
			foundone = false;
			for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
				UserProc *proc = (UserProc *)(*pp);
				if (proc->isLib()) continue;
				if (proc->isDecompiled()) continue;
				int indent = 0;
				proc->decompile(new ProcList, indent);
				foundone = true;
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
		for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
			UserProc *proc = (UserProc *)(*pp);
			if (proc->isLib()) continue;
			proc->printXML();
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
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		if ((*it)->isLib()) continue;
		UserProc *u = (UserProc *)(*it);
		Exp *search = new Location(opGlobal, new Terminal(opWild), u);
		// Search each statement in u, excepting implicit assignments (their uses don't count, since they don't really
		// exist in the program representation)
		StatementList stmts;
		u->getStatements(stmts);
		for (auto ss = stmts.begin(); ss != stmts.end(); ++ss) {
			Statement *s = *ss;
			if (s->isImplicit()) continue;  // Ignore the uses in ImplicitAssigns
			bool found = s->searchAll(search, usedGlobals);
			if (found && DEBUG_UNUSED)
				LOG << " a global is used by stmt " << s->getNumber() << "\n";
		}
	}

	// make a map to find a global by its name (could be a global var too)
	std::map<std::string, Global *> namedGlobals;
	for (auto it = globals.begin(); it != globals.end(); ++it)
		namedGlobals[(*it)->getName()] = (*it);

	// rebuild the globals vector
	const char *name;
	Global *usedGlobal;

	globals.clear();
	for (auto it = usedGlobals.begin(); it != usedGlobals.end(); ++it) {
		if (DEBUG_UNUSED)
			LOG << " " << *it << " is used\n";
		name = ((Const *)(*it)->getSubExp1())->getStr();
		usedGlobal = namedGlobals[name];
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
	bool change = false;
	for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
		UserProc *proc = (UserProc *)(*pp);
		if (proc->isLib()) continue;
		if (!proc->isDecoded()) continue;  // e.g. use -sf file to just prototype the proc
		removeRetSet.insert(proc);
	}
	// The workset is processed in arbitrary order. May be able to do better, but note that sometimes changes propagate
	// down the call tree (no caller uses potential returns for child), and sometimes up the call tree (removal of
	// returns and/or dead code removes parameters, which affects all callers).
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
	for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
		UserProc *proc = (UserProc *)(*pp);
		if (proc->isLib()) continue;
		if (VERBOSE) {
			LOG << "===== before transformation from SSA form for " << proc->getName() << " =====\n";
			proc->printToLog();
			LOG << "===== end before transformation from SSA for " << proc->getName() << " =====\n\n";
			if (Boomerang::get()->dotFile)
				proc->printDFG();
		}
		proc->fromSSAform();
		if (VERBOSE) {
			LOG << "===== after transformation from SSA form for " << proc->getName() << " =====\n";
			proc->printToLog();
			LOG << "===== end after transformation from SSA for " << proc->getName() << " =====\n\n";
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
	for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
		UserProc *proc = (UserProc *)(*pp);
		if (proc->isLib()) continue;
		if (!proc->isDecoded()) continue;
		proc->conTypeAnalysis();
	}
	if (VERBOSE || DEBUG_TA)
		LOG << "=== end type analysis ===\n";
}

void
Prog::globalTypeAnalysis()
{
	if (VERBOSE || DEBUG_TA)
		LOG << "### start global data-flow-based type analysis ###\n";
	for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
		UserProc *proc = (UserProc *)(*pp);
		if (proc->isLib()) continue;
		if (!proc->isDecoded()) continue;
		// FIXME: this just does local TA again. Need to meet types for all parameter/arguments, and return/results!
		// This will require a repeat until no change loop
		std::cout << "global type analysis for " << proc->getName() << "\n";
		proc->typeAnalysis();
	}
	if (VERBOSE || DEBUG_TA)
		LOG << "### end type analysis ###\n";
}

void
Prog::rangeAnalysis()
{
	for (auto pp = m_procs.begin(); pp != m_procs.end(); ++pp) {
		UserProc *proc = (UserProc *)(*pp);
		if (proc->isLib()) continue;
		if (!proc->isDecoded()) continue;
		proc->rangeAnalysis();
		proc->logSuspectMemoryDefs();
	}
}

void
Prog::printCallGraph()
{
	std::string fname1 = Boomerang::get()->getOutputPath() + "callgraph.out";
	std::string fname2 = Boomerang::get()->getOutputPath() + "callgraph.dot";
	int fd1 = lockFileWrite(fname1.c_str());
	int fd2 = lockFileWrite(fname2.c_str());
	std::ofstream f1(fname1.c_str());
	std::ofstream f2(fname2.c_str());
	std::set<Proc *> seen;
	std::map<Proc *, int> spaces;
	std::map<Proc *, Proc *> parent;
	std::list<Proc *> procList;
	f2 << "digraph callgraph {\n";
	for (auto pp = entryProcs.begin(); pp != entryProcs.end(); ++pp)
		procList.push_back(*pp);
	spaces[procList.front()] = 0;
	while (!procList.empty()) {
		Proc *p = procList.front();
		procList.erase(procList.begin());
		if ((unsigned)p == NO_ADDRESS)
			continue;
		if (seen.find(p) == seen.end()) {
			seen.insert(p);
			int n = spaces[p];
			for (int i = 0; i < n; ++i)
				f1 << "\t ";
			f1 << p->getName() << " @ " << std::hex << p->getNativeAddress();
			if (parent.find(p) != parent.end())
				f1 << " [parent=" << parent[p]->getName() << "]";
			f1 << std::endl;
			if (!p->isLib()) {
				++n;
				UserProc *u = (UserProc *)p;
				std::list<Proc *> &calleeList = u->getCallees();
				for (auto it1 = calleeList.rbegin(); it1 != calleeList.rend(); ++it1) {
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

void
printProcsRecursive(Proc *proc, int indent, std::ofstream &f, std::set<Proc *> &seen)
{
	bool fisttime = false;
	if (seen.find(proc) == seen.end()) {
		seen.insert(proc);
		fisttime = true;
	}
	for (int i = 0; i < indent; ++i)
		f << "\t ";

	if (!proc->isLib() && fisttime) { // seen lib proc
		f << "0x" << std::hex << proc->getNativeAddress();
		f << " __nodecode __incomplete void " << proc->getName() << "();\n";

		UserProc *u = (UserProc *)proc;
		std::list<Proc *> &calleeList = u->getCallees();
		for (auto it1 = calleeList.begin(); it1 != calleeList.end(); ++it1) {
			printProcsRecursive(*it1, indent + 1, f, seen);
		}
		for (int i = 0; i < indent; ++i)
			f << "\t ";
		f << "// End of " << proc->getName() << "\n";
	} else {
		f << "// " << proc->getName() << "();\n";
	}
}

void
Prog::printSymbolsToFile()
{
	std::cerr << "entering Prog::printSymbolsToFile\n";
	std::string fname = Boomerang::get()->getOutputPath() + "symbols.h";
	int fd = lockFileWrite(fname.c_str());
	std::ofstream f(fname.c_str());

	/* Print procs */
	f << "/* Functions: */\n";
	std::set<Proc *> seen;
	for (auto pp = entryProcs.begin(); pp != entryProcs.end(); ++pp)
		printProcsRecursive(*pp, 0, f, seen);

	f << "/* Leftovers: */\n"; // don't forget the rest
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it)
		if (!(*it)->isLib() && seen.find(*it) == seen.end()) {
			printProcsRecursive(*it, 0, f, seen);
		}

	f.close();
	unlockFile(fd);
	std::cerr << "leaving Prog::printSymbolsToFile\n";
}

void
Prog::printCallGraphXML()
{
	if (!DUMP_XML)
		return;
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it)
		(*it)->clearVisited();
	std::string fname = Boomerang::get()->getOutputPath() + "callgraph.xml";
	int fd = lockFileWrite(fname.c_str());
	std::ofstream f(fname.c_str());
	f << "<prog name=\"" << getName() << "\">\n";
	f << "\t<callgraph>\n";
	for (auto pp = entryProcs.begin(); pp != entryProcs.end(); ++pp)
		(*pp)->printCallGraphXML(f, 2);
	for (auto it = m_procs.begin(); it != m_procs.end(); ++it) {
		if (!(*it)->isVisited() && !(*it)->isLib()) {
			(*it)->printCallGraphXML(f, 2);
		}
	}
	f << "\t</callgraph>\n";
	f << "</prog>\n";
	f.close();
	unlockFile(fd);
}

void
Prog::readSymbolFile(const char *fname)
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

	for (auto it = par.symbols.begin(); it != par.symbols.end(); ++it) {
		if ((*it)->sig) {
			Proc *p = newProc((*it)->sig->getName(), (*it)->addr,
			                  pBF->isDynamicLinkedProcPointer((*it)->addr)
			                  // NODECODE isn't really the right modifier; perhaps we should have a LIB modifier,
			                  // to specifically specify that this function obeys library calling conventions
			               || (*it)->mods->noDecode);
			if (!(*it)->mods->incomplete) {
				p->setSignature((*it)->sig->clone());
				p->getSignature()->setForced(true);
			}
		} else {
			const char *nam = (*it)->nam.c_str();
			if (strlen(nam) == 0) {
				nam = newGlobalName((*it)->addr);
			}
			Type *ty = (*it)->ty;
			if (!ty) {
				ty = guessGlobalType(nam, (*it)->addr);
			}
			globals.insert(new Global(ty, (*it)->addr, nam));
		}
	}

	for (auto it2 = par.refs.begin(); it2 != par.refs.end(); ++it2) {
		pFE->addRefHint((*it2)->addr, (*it2)->nam.c_str());
	}
}

Global::~Global()
{
	// Do-nothing d'tor
}

Exp *
Global::getInitialValue(Prog *prog)
{
	Exp *e = nullptr;
	const SectionInfo *si = prog->getSectionInfoByAddr(uaddr);
	if (si && si->isAddressBss(uaddr))
		// This global is in the BSS, so it can't be initialised
		return nullptr;
	if (!si)
		return nullptr;
	e = prog->readNativeAs(uaddr, type);
	return e;
}

void
Global::print(std::ostream &os, Prog *prog)
{
	Exp *init = getInitialValue(prog);
	os << type << " " << nam << " at " << std::hex << uaddr << std::dec
	   << " initial value " << (init ? init->prints() : "<none>");
}

Exp *
Prog::readNativeAs(ADDRESS uaddr, Type *type)
{
	Exp *e = nullptr;
	const SectionInfo *si = pBF->getSectionInfoByAddr(uaddr);
	if (!si)
		return nullptr;
	if (type->resolvesToPointer()) {
		ADDRESS init = pBF->readNative4(uaddr);
		if (init == 0)
			return new Const(0);
		const char *nam = getGlobalName(init);
		if (nam)
			// TODO: typecast?
			return Location::global(nam, nullptr);
		if (type->asPointer()->getPointsTo()->resolvesToChar()) {
			const char *str = getStringConstant(init);
			if (str)
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
				LOG << "unable to read native address " << addr << " as type " << t->getCtype() << "\n";
				v = new Const(-1);
			}
			if (n->isNil()) {
				n = new Binary(opList, v, n);
				e = n;
			} else {
				assert(n->getSubExp2()->getOper() == opNil);
				n->setSubExp2(new Binary(opList, v, n->getSubExp2()));
				n = n->getSubExp2();
			}
		}
		return e;
	}
	if (type->resolvesToArray() && type->asArray()->getBaseType()->resolvesToChar()) {
		const char *str = getStringConstant(uaddr, true);
		if (str) {
			// Make a global string
			return new Const(str);
		}
	}
	if (type->resolvesToArray()) {
		int nelems = -1;
		const char *nam = getGlobalName(uaddr);
		int base_sz = type->asArray()->getBaseType()->getSize() / 8;
		if (nam)
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
				assert(n->getSubExp2()->getOper() == opNil);
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
	std::ofstream os;
	pFE->processProc(proc->getNativeAddress(), proc, os);
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
	ClusterMemo *m = new ClusterMemo(mId);
	m->name = name;
	m->children = children;
	m->parent = parent;
	return m;
}

void
Cluster::readMemo(Memo *mm, bool dec)
{
	ClusterMemo *m = dynamic_cast<ClusterMemo *>(mm);

	name = m->name;
	children = m->children;
	parent = m->parent;

	for (auto it = children.begin(); it != children.end(); ++it)
		(*it)->restoreMemo(m->mId, dec);
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
	GlobalMemo *m = new GlobalMemo(mId);
	m->type = type;
	m->uaddr = uaddr;
	m->nam = nam;

	type->takeMemo(mId);
	return m;
}

void
Global::readMemo(Memo *mm, bool dec)
{
	GlobalMemo *m = dynamic_cast<GlobalMemo *>(mm);

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
	ProgMemo *m = new ProgMemo(mId);
	m->m_name = m_name;
	m->m_path = m_path;
	m->m_procs = m_procs;
	m->m_procLabels = m_procLabels;
	m->globals = globals;
	m->globalMap = globalMap;
	m->m_iNumberedProc = m_iNumberedProc;
	m->m_rootCluster = m_rootCluster;

	for (auto it = m_procs.begin(); it != m_procs.end(); ++it)
		(*it)->takeMemo(m->mId);
	m_rootCluster->takeMemo(m->mId);
	for (auto it = globals.begin(); it != globals.end(); ++it)
		(*it)->takeMemo(m->mId);

	return m;
}

void
Prog::readMemo(Memo *mm, bool dec)
{
	ProgMemo *m = dynamic_cast<ProgMemo *>(mm);
	m_name = m->m_name;
	m_path = m->m_path;
	m_procs = m->m_procs;
	m_procLabels = m->m_procLabels;
	globals = m->globals;
	globalMap = m->globalMap;
	m_iNumberedProc = m->m_iNumberedProc;
	m_rootCluster = m->m_rootCluster;

	for (auto it = m_procs.begin(); it != m_procs.end(); ++it)
		(*it)->restoreMemo(m->mId, dec);
	m_rootCluster->restoreMemo(m->mId, dec);
	for (auto it = globals.begin(); it != globals.end(); ++it)
		(*it)->restoreMemo(m->mId, dec);
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
		std::cerr << "attempt to decode fragment outside text area, addr=" << a << "\n";
		if (VERBOSE)
			LOG << "attempt to decode fragment outside text area, addr=" << a << "\n";
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
		const std::map<ADDRESS, std::string> &symbols = pBF->getSymbols();
		auto sym = symbols.find(a);
		if (sym != symbols.end()) {
			const char *n = sym->second.c_str();
			unsigned int sz = pBF->getSizeByName(n);
			if (!getGlobal(n)) {
				Global *global = new Global(new SizeType(sz * 8), a, n);
				globals.insert(global);
			}
			e = new Unary(opAddrOf, Location::global(n, nullptr));
		} else {
			const char *str = getStringConstant(a);
			if (str)
				e = new Const(str);
			else {
				// check for accesses into the middle of symbols
				for (auto it = symbols.begin(); it != symbols.end(); ++it) {
					if (a > it->first
					 && a < it->first + pBF->getSizeByName(it->second.c_str())) {
						int off = a - it->first;
						e = new Binary(opPlus,
						               new Unary(opAddrOf, Location::global(it->second.c_str(), nullptr)),
						               new Const(off));
						break;
					}
				}
			}
		}
	}
	return e;
}
