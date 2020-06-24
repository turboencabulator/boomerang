/**
 * \file
 * \brief Contains common code for all front ends.
 *
 * The majority of frontend logic remains in the source dependent files such
 * as sparcfrontend.cpp.
 *
 * \authors
 * Copyright (C) 1999-2001, The University of Queensland
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

#include "frontend.h"

#include "BinaryFile.h"
#include "ansi-c-parser.h"
#include "boomerang.h"
#include "cfg.h"
#include "decoder.h"
#include "exp.h"
#include "proc.h"
#include "prog.h"
#include "register.h"
#include "rtl.h"
#include "signature.h"
#include "types.h"

#ifdef DYNAMIC
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#else
#include "mipsfrontend.h"
#include "pentiumfrontend.h"
#include "ppcfrontend.h"
#include "sparcfrontend.h"
#include "st20frontend.h"
#endif

#include <fstream>
#include <queue>
#include <sstream>

#include <cstring>
#include <cassert>

/**
 * Takes some parameters to save passing these around a lot.
 *
 * \param bf    The BinaryFile object (loader).
 * \param prog  Program being decoded.
 */
FrontEnd::FrontEnd(BinaryFile *bf, Prog *prog) :
	pBF(bf),
	prog(prog)
{
}

/**
 * \brief Creates and returns an instance of the appropriate subclass.
 *
 * Get an instance of a class derived from FrontEnd, returning a pointer to
 * the object of that class.  Do this by guessing the machine for the binary
 * file whose name is name, loading the appropriate library using
 * dlopen/dlsym, running the "construct" function in that library, and
 * returning the result.
 *
 * \param name  Name of the file (BinaryFile) to open.
 * \param prog  Passed to the constructor.
 *
 * \returns A new FrontEnd subclass instance.  Use close() to destroy it.
 */
FrontEnd *
FrontEnd::open(const char *name, Prog *prog)
{
	auto bf = BinaryFile::open(name);
	if (!bf) return nullptr;
	auto fe = FrontEnd::open(bf, prog);
	if (!fe) BinaryFile::close(bf);
	return fe;
}

/**
 * \overload
 * \param bf    Passed to the constructor.
 * \param prog  Passed to the constructor.
 */
FrontEnd *
FrontEnd::open(BinaryFile *bf, Prog *prog)
{
	MACHINE machine = bf->getMachine();

#ifdef DYNAMIC
	const char *libname;
	switch (machine) {
	case MACHINE_PENTIUM: libname = MODPREFIX "pentiumfrontend" MODSUFFIX; break;
	case MACHINE_SPARC:   libname = MODPREFIX   "sparcfrontend" MODSUFFIX; break;
	case MACHINE_PPC:     libname = MODPREFIX     "ppcfrontend" MODSUFFIX; break;
	case MACHINE_ST20:    libname = MODPREFIX    "st20frontend" MODSUFFIX; break;
	case MACHINE_MIPS:    libname = MODPREFIX    "mipsfrontend" MODSUFFIX; break;
	default:
		std::cerr << "Machine architecture not supported!\n";
		return nullptr;
	}

	// Load the specific frontend library
	void *handle = dlopen(libname, RTLD_LAZY);
	if (!handle) {
		std::cout << "cannot load library: " << dlerror() << "\n";
		return nullptr;
	}

	// Reset errors
	const char *error = dlerror();

	// Use the handle to find symbols
	const char *symbol = "construct";
	constructFcn construct = (constructFcn)dlsym(handle, symbol);
	error = dlerror();
	if (error) {
		std::cerr << "cannot load symbol '" << symbol << "': " << error << "\n";
		dlclose(handle);
		return nullptr;
	}
	symbol = "destruct";
	destructFcn destruct = (destructFcn)dlsym(handle, symbol);
	error = dlerror();
	if (error) {
		std::cerr << "cannot load symbol '" << symbol << "': " << error << "\n";
		dlclose(handle);
		return nullptr;
	}

	// Call the construct function
	FrontEnd *fe = construct(bf, prog);

	// Stash pointers in the constructed object, for use by FrontEnd::close
	fe->dlHandle = handle;
	fe->destruct = destruct;

	return fe;
#else
	switch (machine) {
	case MACHINE_PENTIUM: return new PentiumFrontEnd(bf, prog);
	case MACHINE_SPARC:   return new   SparcFrontEnd(bf, prog);
	case MACHINE_PPC:     return new     PPCFrontEnd(bf, prog);
	case MACHINE_ST20:    return new    ST20FrontEnd(bf, prog);
	case MACHINE_MIPS:    return new    MIPSFrontEnd(bf, prog);
	default:
		std::cerr << "Machine architecture not supported!\n";
		return nullptr;
	}
#endif
}

/**
 * \brief Destroys an instance created by open() or new.
 */
void
FrontEnd::close(FrontEnd *fe)
{
#ifdef DYNAMIC
	// Retrieve the stashed pointers
	void *handle = fe->dlHandle;
	destructFcn destruct = fe->destruct;

	// Destruct in an appropriate way.
	// The C++ dlopen mini HOWTO says to always use a matching
	// construct/destruct pair in case of new/delete overloading.
	if (handle) {
		destruct(fe);
		dlclose(handle);
	} else
#endif
		delete fe;
}

/**
 * \brief Is this a win32 frontend?
 */
bool
FrontEnd::isWin32() const
{
	return pBF->getFormat() == LOADFMT_PE;
}

bool
FrontEnd::noReturnCallDest(const std::string &name)
{
	return name == "_exit"
	    || name == "exit"
	    || name == "ExitProcess"
	    || name == "abort"
	    || name == "_assert";
}

/**
 * \brief Read library signatures from a catalog.
 */
void
FrontEnd::readLibraryCatalog(const std::string &path)
{
	std::ifstream inf(path);
	if (!inf.good()) {
		std::cerr << "can't open `" << path << "'\n";
		exit(1);
	}

	while (!inf.eof()) {
		std::string name;
		std::getline(inf, name);
		std::string::size_type j = name.find('#');
		if (j != name.npos)
			name.erase(j);
		j = name.find_last_not_of(" \t\n\v\f\r");
		if (j != name.npos)
			name.erase(j + 1);
		else
			continue;
		std::string path = Boomerang::get().getProgPath() + "signatures/" + name;
		callconv cc = CONV_C;  // Most APIs are C calling convention
		if (name == "windows.h") cc = CONV_PASCAL;    // One exception
		if (name == "mfc.h")     cc = CONV_THISCALL;  // Another exception
		readLibrarySignatures(path, cc);
	}
	inf.close();
}

/**
 * \brief Read library signatures from the default catalog.
 */
void
FrontEnd::readLibraryCatalog()
{
	librarySignatures.clear();
	std::string path = Boomerang::get().getProgPath() + "signatures/";
	readLibraryCatalog(path + "common.hs");
	readLibraryCatalog(path + Signature::platformName(getFrontEndId()) + ".hs");
	if (isWin32()) {
		readLibraryCatalog(path + "win32.hs");
	}
}

/**
 * \brief Locate the starting address of "main" in the code section.
 *
 * \returns Native pointer if found; NO_ADDRESS if not.
 */
ADDRESS
FrontEnd::getMainEntryPoint(bool &gotMain)
{
	ADDRESS start = pBF->getMainEntryPoint();
	if (start != NO_ADDRESS) {
		gotMain = true;
		return start;
	}

	start = pBF->getEntryPoint();
	gotMain = start != NO_ADDRESS;
	return start;
}

/**
 * \brief Returns a list of all available entrypoints.
 */
std::vector<ADDRESS>
FrontEnd::getEntryPoints()
{
	std::vector<ADDRESS> entrypoints;
	bool gotMain = false;
	ADDRESS a = getMainEntryPoint(gotMain);
	if (a != NO_ADDRESS) {
		entrypoints.push_back(a);
	} else {  // try some other tricks
		const char *fname = pBF->getFilename();
		// X11 Module
		if (!strcmp(fname + strlen(fname) - 6, "_drv.o")) {
			const char *p = fname + strlen(fname) - 6;
			while (*p != '/' && *p != '\\' && p != fname)
				--p;
			if (p != fname) {
				++p;
				auto name = std::string(p);
				name.erase(name.length() - 6);
				name += "ModuleData";
				ADDRESS a = pBF->getAddressByName(name, true);
				if (a != NO_ADDRESS) {
					//ADDRESS vers = pBF->readNative4(a);
					if (ADDRESS setup = pBF->readNative4(a + 4)) {
						Type *ty = Type::getNamedType("ModuleSetupProc");
						assert(ty->isFunc());
						UserProc *proc = (UserProc *)prog->setNewProc(setup);
						assert(proc);
						Signature *sig = ty->asFunc()->getSignature()->clone();
						if (auto sym = pBF->getSymbolByAddress(setup))
							sig->setName(sym);
						sig->setForced(true);
						proc->setSignature(sig);
						entrypoints.push_back(setup);
					}
					if (ADDRESS teardown = pBF->readNative4(a + 8)) {
						Type *ty = Type::getNamedType("ModuleTearDownProc");
						assert(ty->isFunc());
						UserProc *proc = (UserProc *)prog->setNewProc(teardown);
						assert(proc);
						Signature *sig = ty->asFunc()->getSignature()->clone();
						if (auto sym = pBF->getSymbolByAddress(teardown))
							sig->setName(sym);
						sig->setForced(true);
						proc->setSignature(sig);
						entrypoints.push_back(teardown);
					}
				}
			}
		}
		// Linux kernel module
		if (!strcmp(fname + strlen(fname) - 3, ".ko")) {
			a = pBF->getAddressByName("init_module");
			if (a != NO_ADDRESS)
				entrypoints.push_back(a);
			a = pBF->getAddressByName("cleanup_module");
			if (a != NO_ADDRESS)
				entrypoints.push_back(a);
		}
	}
	return entrypoints;
}

/**
 * \brief Decode all undecoded procedures.
 */
void
FrontEnd::decode()
{
	Boomerang::get().alert_decode_start(pBF->getLimitTextLow(), pBF->getLimitTextHigh() - pBF->getLimitTextLow());

	bool gotMain;
	ADDRESS a = getMainEntryPoint(gotMain);
	if (VERBOSE)
		LOG << "start: 0x" << std::hex << a << std::dec << " gotmain: " << (gotMain ? "true" : "false") << "\n";
	if (a == NO_ADDRESS) {
		std::vector<ADDRESS> entrypoints = getEntryPoints();
		for (const auto &entrypoint : entrypoints)
			decode(entrypoint);
		return;
	}

	decode(a);
	prog->setEntryPoint(a);

	if (gotMain) {
		static const char *mainName[] = { "main", "WinMain", "DriverEntry" };
		const char *name = pBF->getSymbolByAddress(a);
		if (!name)
			name = mainName[0];
		for (size_t i = 0; i < sizeof mainName / sizeof *mainName; ++i) {
			if (!strcmp(name, mainName[i])) {
				if (auto proc = prog->findProc(a)) {
					if (auto fty = dynamic_cast<FuncType *>(Type::getNamedType(name))) {
						proc->setSignature(fty->getSignature()->clone());
						proc->getSignature()->setName(name);
						//proc->getSignature()->setFullSig(true);  // Don't add or remove parameters
						proc->getSignature()->setForced(true);   // Don't add or remove parameters
					} else {
						LOG << "unable to find signature for known entrypoint " << name << "\n";
					}
				} else {
					if (VERBOSE)
						LOG << "no proc found for address 0x" << std::hex << a << std::dec << "\n";
				}
				break;
			}
		}
	}
}

/**
 * \brief Decode all procs starting at a given address in a given program.
 *
 * Somehow, a == NO_ADDRESS has come to mean decode anything not already decoded.
 */
void
FrontEnd::decode(ADDRESS a)
{
	if (a != NO_ADDRESS) {
		prog->setNewProc(a);
		if (VERBOSE)
			LOG << "starting decode at address 0x" << std::hex << a << std::dec << "\n";
		if (auto proc = prog->findProc(a)) {
			if (auto up = dynamic_cast<UserProc *>(proc)) {
				processProc(a, up);
				up->setDecoded();
			} else {
				LOG << "NOT decoding library proc at address 0x" << std::hex << a << std::dec << "\n";
				return;
			}
		} else {
			if (VERBOSE)
				LOG << "no proc found at address 0x" << std::hex << a << std::dec << "\n";
			return;
		}

	} else {  // a == NO_ADDRESS
		bool change = true;
		while (change) {
			change = false;
			PROGMAP::const_iterator it;
			for (auto proc = prog->getFirstProc(it); proc; proc = prog->getNextProc(it)) {
				if (auto up = dynamic_cast<UserProc *>(proc)) {
					if (up->isDecoded()) continue;

					// undecoded userproc.. decode it
					change = true;
					if (processProc(up->getNativeAddress(), up))
						up->setDecoded();
					else
						break;
					// Break out of the loops if not decoding children
					if (Boomerang::get().noDecodeChildren)
						break;
				}
			}
			if (Boomerang::get().noDecodeChildren)
				break;
		}
	}
	prog->wellForm();
}

#if 0 // Cruft?
/**
 * \brief Decode one proc starting at a given address in a given program.
 *
 * \param a  Should be the address of a UserProc.
 */
void
FrontEnd::decodeOnly(ADDRESS a)
{
	auto proc = prog->setNewProc(a);
	auto up = dynamic_cast<UserProc *>(proc);
	assert(up);
	if (processProc(up->getNativeAddress(), up))
		up->setDecoded();
	prog->wellForm();
}
#endif

/**
 * \brief Decode a fragment of a procedure, e.g. for each destination of a
 * switch statement.
 */
void
FrontEnd::decodeFragment(UserProc *proc, ADDRESS a)
{
	if (a >= pBF->getLimitTextLow() && a < pBF->getLimitTextHigh()) {
		if (Boomerang::get().traceDecoder)
			LOG << "decoding fragment at 0x" << std::hex << a << std::dec << "\n";
		processProc(a, proc);
	} else {
		std::cerr << "attempt to decode fragment outside text area, addr=0x" << std::hex << a << std::dec << "\n";
		if (VERBOSE)
			LOG << "attempt to decode fragment outside text area, addr=0x" << std::hex << a << std::dec << "\n";
	}
}

DecodeResult &
FrontEnd::decodeInstruction(ADDRESS pc)
{
	if (!pBF->getSectionInfoByAddr(pc)) {
		LOG << "ERROR: attempted to decode outside any known segment 0x" << std::hex << pc << std::dec << "\n";
		static DecodeResult invalid;
		invalid.reset();
		invalid.valid = false;
		return invalid;
	}
	return getDecoder().decodeInstruction(pc, pBF);
}

/**
 * \brief Returns a symbolic name for a register index.
 */
const char *
FrontEnd::getRegName(int idx)
{
	return getDecoder().getRegName(idx);
}

int
FrontEnd::getRegSize(int idx)
{
	return getDecoder().getRegSize(idx);
}

/**
 * \brief Read the library signatures from a file.
 *
 * \param path  The file to read from.
 * \param cc    The calling convention assumed.
 */
void
FrontEnd::readLibrarySignatures(const std::string &path, callconv cc)
{
	std::ifstream ifs(path);
	if (!ifs.good()) {
		std::cerr << "can't open `" << path << "'\n";
		exit(1);
	}

	AnsiCParser p(ifs, false);
	platform plat = getFrontEndId();
	p.yyparse(plat, cc);
	ifs.close();

	for (const auto &sig : p.signatures) {
#if 0
		std::cerr << "readLibrarySignatures from " << path << ": " << sig->getName() << "\n";
#endif
		librarySignatures[sig->getName()] = sig;
		sig->setSigFile(path);
	}
}

/**
 * \brief Return a signature that matches the architecture best.
 */
Signature *
FrontEnd::getDefaultSignature(const std::string &name) const
{
	// Get a default library signature
	if (isWin32())
		return Signature::instantiate(PLAT_PENTIUM, CONV_PASCAL, name.c_str());
	else
		return Signature::instantiate(getFrontEndId(), CONV_C, name.c_str());
}

/**
 * \brief Lookup a library signature by name.
 */
Signature *
FrontEnd::getLibSignature(const std::string &name) const
{
	// Look up the name in the librarySignatures map
	auto it = librarySignatures.find(name);
	if (it != librarySignatures.end()) {
		// Don't clone here; cloned in CallStatement::setSigArguments
		auto signature = it->second;
		signature->setUnknown(false);
		return signature;
	}
	LOG << "Unknown library function " << name << "\n";
	return getDefaultSignature(name);
}

/**
 * \brief Process a procedure, given a native (source machine) address.
 *
 * This is the main function for decoding a procedure.  It is usually
 * overridden in the derived class to do source machine specific things.
 *
 * \param addr  The address at which the procedure starts.
 * \param proc  The procedure object.
 * \param spec  If true, this is a speculative decode
 *              (so give up on any invalid instruction).
 *
 * \note This is a sort of generic front end.  For many processors, this will
 * be overridden in the FrontEnd derived class, sometimes calling this
 * function to do most of the work.  Sparc is an exception.
 *
 * \returns true on a good decode (no illegal instructions).
 */
bool
FrontEnd::processProc(ADDRESS addr, UserProc *proc, bool spec)
{
	// just in case you missed it
	Boomerang::get().alert_new(proc);

	// We have a set of CallStatement pointers. These may be disregarded if this is a speculative decode
	// that fails (i.e. an illegal instruction is found). If not, this set will be used to add to the set of calls
	// to be analysed in the cfg, and also to call newProc()
	std::list<CallStatement *> callList;

	auto cfg = proc->getCFG();

	// If this is a speculative decode, the second time we decode the same address, we get no cfg. Else an error.
	if (spec && !cfg)
		return false;
	assert(cfg);

	// Clear the pointer used by the caller prologue code to access the last call rtl of this procedure
	//getDecoder().resetLastCall();

	int nTotalBytes = 0;
	ADDRESS startAddr = addr;
	ADDRESS lastAddr = addr;

	// Initialise the queue of control flow targets that have yet to be decoded.
	cfg->enqueue(addr);

	while ((addr = cfg->dequeue()) != NO_ADDRESS) {
		// The list of RTLs for the current basic block
		auto BB_rtls = (std::list<RTL *> *)nullptr;

		// Indicates whether or not the next instruction to be decoded is the lexical successor of the current one.
		// Will be true for all NCTs and for CTIs with a fall through branch.
		// Keep decoding sequentially until a CTI without a fall through branch is decoded
		bool sequentialDecode = true;
		while (sequentialDecode) {

			// Decode and classify the current source instruction
			if (Boomerang::get().traceDecoder)
				LOG << "*0x" << std::hex << addr << std::dec << "\t";

			// Decode the inst at addr.
			auto inst = decodeInstruction(addr);

			// If invalid and we are speculating, just exit
			if (spec && !inst.valid)
				return false;

			// Need to construct a new list of RTLs if a basic block has just been finished but decoding is
			// continuing from its lexical successor
			if (!BB_rtls)
				BB_rtls = new std::list<RTL *>();

			if (!inst.valid) {
				// Alert the watchers to the problem
				Boomerang::get().alert_decode_bad(addr);

				// An invalid instruction. Most likely because a call did not return (e.g. call _exit()), etc.
				// Best thing is to emit a INVALID BB, and continue with valid instructions
				if (VERBOSE) {
					LOG << "Warning: invalid instruction at 0x" << std::hex << addr << ": ";
					// Emit the next 4 bytes for debugging
					for (int ii = 0; ii < 4; ++ii)
						LOG << (unsigned)(pBF->readNative1(addr + ii) & 0xFF) << " ";
					LOG << std::dec << "\n";
				}

				// Emit the RTL anyway, so we have the address and maybe some other clues
				BB_rtls->push_back(new RTL(addr));
				auto bb = cfg->newBB(BB_rtls, INVALID);
				BB_rtls = nullptr;
				sequentialDecode = false;
				continue;
			}

			// alert the watchers that we have decoded an instruction
			Boomerang::get().alert_decode_inst(addr, inst.numBytes);
			nTotalBytes += inst.numBytes;

			// Check if this is an already decoded jump instruction (from a previous pass with propagation etc)
			// If so, we throw away the just decoded RTL (but we still may have needed to calculate the number
			// of bytes.. ick.)
			RTL *rtl = inst.rtl;
			auto ff = previouslyDecoded.find(addr);
			if (ff != previouslyDecoded.end())
				rtl = ff->second;

			if (!rtl) {
				// This can happen if an instruction is "cancelled", e.g. call to __main in a hppa program
				// Just ignore the whole instruction
				if (inst.numBytes > 0)
					addr += inst.numBytes;
				continue;
			}

			// Display RTL representation if asked
			if (Boomerang::get().printRtl)
				LOG << *rtl;

			// For each Statement in the RTL
			//std::list<Statement*>& sl = rtl->getList();
			std::list<Statement *> sl = rtl->getList();
			// Make a copy (!) of the list. This is needed temporarily to work around the following problem.
			// We are currently iterating an RTL, which could be a return instruction. The RTL is passed to
			// createReturnBlock; if this is not the first return statement, it will get cleared, and this will
			// cause problems with the current iteration. The effects seem to be worse for MSVC/Windows.
			// This problem will likely be easier to cope with when the RTLs are removed, and there are special
			// Statements to mark the start of instructions (and their native address).
			// FIXME: However, this workaround breaks logic below where a GOTO is changed to a CALL followed by a return
			// if it points to the start of a known procedure
#if 1
			for (auto ss = sl.begin(); ss != sl.end(); ++ss) { // }
#else
			// The counter is introduced because ss != sl.end() does not work as it should
			// FIXME: why? Does this really fix the problem?
			int counter = sl.size();
			for (auto ss = sl.begin(); counter > 0; ++ss, --counter) {
#endif
				Statement *s = *ss;
				s->setProc(proc);  // let's do this really early!
				auto it = refHints.find(rtl->getAddress());
				if (it != refHints.end()) {
					const auto &nam = it->second;
					ADDRESS gu = prog->getGlobalAddr(nam);
					if (gu != NO_ADDRESS) {
						s->searchAndReplace(new Const((int)gu), new Unary(opAddrOf, Location::global(nam, proc)));
					}
				}
				s->simplify();

				// Check for a call to an already existing procedure (including self recursive jumps), or to the PLT
				// (note that a LibProc entry for the PLT function may not yet exist)
				if (s->getKind() == STMT_GOTO) {
					auto jump = static_cast<GotoStatement *>(s);
					auto dest = jump->getFixedDest();
					if (dest != NO_ADDRESS) {
						Proc *destProc = prog->findProc(dest);
						if (!destProc) {
							if (pBF->isDynamicLinkedProc(dest))
								destProc = prog->setNewProc(dest);
						}
						if (destProc && destProc != (Proc *)-1) {
							auto call = new CallStatement(dest);
							s = call;
							call->setDestProc(destProc);
							call->setReturnAfterCall(true);
							// also need to change it in the actual RTL
							assert(ss == --sl.end());
							rtl->replaceLastStmt(s);
							*ss = s;
						}
					}
				}

				switch (s->getKind()) {

				case STMT_GOTO:
					{
						auto jump = static_cast<GotoStatement *>(s);
						auto dest = jump->getFixedDest();

						// Handle one way jumps and computed jumps separately
						if (dest != NO_ADDRESS) {

							BB_rtls->push_back(rtl);
							auto bb = cfg->newBB(BB_rtls, ONEWAY);
							BB_rtls = nullptr;  // Clear when make new BB
							sequentialDecode = false;
							handleBranch(dest, bb, cfg);
						}
					}
					break;

				case STMT_CASE:
					{
						auto jump = static_cast<CaseStatement *>(s);
						auto dest = jump->getDest();
						if (!dest) {  // Happens if already analysed (now redecoding)
							// SWITCH_INFO *psi = jump->getSwitchInfo();
							BB_rtls->push_back(rtl);
							auto bb = cfg->newBB(BB_rtls, NWAY);
							BB_rtls = nullptr;
							sequentialDecode = false;
							processSwitch(bb, proc);        // decode arms, set out edges, etc
						} else if (dest->isMemOf()  // Check for indirect calls to library functions, especially in Win32 programs
						        && dest->getSubExp1()->isIntConst()
						        && pBF->isDynamicLinkedProcPointer(((Const *)dest->getSubExp1())->getAddr())) {
							if (VERBOSE)
								LOG << "jump to a library function: " << *jump << ", replacing with a call/ret.\n";
							// jump to a library function
							// replace with a call ret
							std::string func = pBF->getDynamicProcName(((Const *)dest->getSubExp1())->getAddr());
							auto call = new CallStatement;
							call->setDest(dest->clone());
							LibProc *lp = proc->getProg()->getLibraryProc(func);
							if (!lp)
								LOG << "getLibraryProc returned nullptr, aborting\n";
							assert(lp);
							call->setDestProc(lp);
							BB_rtls->push_back(new RTL(rtl->getAddress(), call));
							auto bb = cfg->newBB(BB_rtls, CALL);
							BB_rtls = nullptr;
							sequentialDecode = false;
							appendSyntheticReturn(bb, proc);
							if (rtl->getAddress() == proc->getNativeAddress()) {
								// it's a thunk
								// Proc *lp = prog->findProc(func);
								func = std::string("__imp_") + func;
								proc->setName(func);
								//lp->setName(func);
								Boomerang::get().alert_update_signature(proc);
							}
							callList.push_back(call);
							ss = sl.end(); --ss;  // get out of the loop
						} else {  // We create the BB as a COMPJUMP type, then change to an NWAY if it turns out to be a switch stmt
							BB_rtls->push_back(rtl);
							auto bb = cfg->newBB(BB_rtls, COMPJUMP);
							BB_rtls = nullptr;  // New RTLList for next BB
							sequentialDecode = false;

							LOG << "COMPUTED JUMP at 0x" << std::hex << addr << std::dec << ", dest = " << *dest << "\n";
							if (Boomerang::get().noDecompile) {
								// try some hacks
								if (dest->isMemOf()
								 && dest->getSubExp1()->getOper() == opPlus
								 && dest->getSubExp1()->getSubExp2()->isIntConst()) {
									// assume subExp2 is a jump table
									bb->updateType(NWAY);
									for (ADDRESS jmptbl = ((Const *)dest->getSubExp1()->getSubExp2())->getInt(); ; jmptbl += 4) {
										auto dest = pBF->readNative4(jmptbl);
										if (dest < pBF->getLimitTextLow() || dest >= pBF->getLimitTextHigh())
											break;
										LOG << "  guessed dest 0x" << std::hex << dest << std::dec << "\n";
										cfg->visit(dest, bb);
										cfg->addOutEdge(bb, dest);
									}
								}
							}
						}
					}
					break;

				case STMT_BRANCH:
					{
						auto branch = static_cast<BranchStatement *>(s);
						auto dest = branch->getFixedDest();
						BB_rtls->push_back(rtl);
						auto bb = cfg->newBB(BB_rtls, TWOWAY);
						BB_rtls = nullptr;
						handleBranch(dest, bb, cfg);

						// Add the fall-through outedge
						cfg->addOutEdge(bb, addr + inst.numBytes);

						// Continue with the next instruction.
					}
					break;

				case STMT_CALL:
					{
						auto call = static_cast<CallStatement *>(s);
						auto dest = call->getDest();

						// Check for a dynamic linked library function
						if (dest->isMemOf()
						 && dest->getSubExp1()->isIntConst()
						 && pBF->isDynamicLinkedProcPointer(((Const *)dest->getSubExp1())->getAddr())) {
							// Dynamic linked proc pointers are treated as static.
							const char *nam = pBF->getDynamicProcName(((Const *)dest->getSubExp1())->getAddr());
							Proc *p = proc->getProg()->getLibraryProc(nam);
							call->setDestProc(p);
							call->setIsComputed(false);
						}

						// Is the called function a thunk calling a library function?
						// A "thunk" is a function which only consists of: "GOTO library_function"
						if (call->getFixedDest() != NO_ADDRESS) {
							// Get the address of the called function.
							auto callAddr = call->getFixedDest();
							// It should not be in the PLT either, but getLimitTextHigh() takes this into account
							if (callAddr < pBF->getLimitTextHigh()) {
								// Decode it.
								auto decoded = decodeInstruction(callAddr);
								if (decoded.valid) { // is the instruction decoded succesfully?
									// Yes, it is. Create a Statement from it.
									auto first_statement = decoded.rtl->getList().front();
									if (first_statement) {
										first_statement->setProc(proc);
										first_statement->simplify();
										// In fact it's a computed (looked up) jump, so the jump seems to be a case statement.
										auto jump = dynamic_cast<CaseStatement *>(first_statement);
										if (jump
										 && jump->getDest()->isMemOf()
										 && jump->getDest()->getSubExp1()->isIntConst()
										 && pBF->isDynamicLinkedProcPointer(((Const *)jump->getDest()->getSubExp1())->getAddr())) {  // Is it an "DynamicLinkedProcPointer"?
											// Yes, it's a library function. Look up it's name.
											ADDRESS a = ((Const *)jump->getDest()->getSubExp1())->getAddr();
											const char *nam = pBF->getDynamicProcName(a);
											// Assign the proc to the call
											Proc *p = proc->getProg()->getLibraryProc(nam);
											if (call->getDestProc()) {
												// prevent unnecessary __imp procs
												prog->removeProc(call->getDestProc()->getName());
											}
											call->setDestProc(p);
											call->setIsComputed(false);
											call->setDest(Location::memOf(new Const(a)));
										}
									}
								}
							}
						}

						// Treat computed and static calls separately
						if (call->isComputed()) {
							BB_rtls->push_back(rtl);
							auto bb = cfg->newBB(BB_rtls, COMPCALL);
							cfg->addOutEdge(bb, addr + inst.numBytes);

							// Add this call to the list of calls to analyse. We won't
							// be able to analyse it's callee(s), of course.
							callList.push_back(call);
						} else {  // Static call
							// Find the address of the callee.
							auto newAddr = call->getFixedDest();

							// Calls with 0 offset (i.e. call the next instruction) are simply pushing the PC to the
							// stack. Treat these as non-control flow instructions and continue.
							if (newAddr == addr + inst.numBytes)
								break;

							// Call the virtual helper function. If implemented, will check for machine specific funcion
							// calls
							if (helperFunc(*BB_rtls, addr, newAddr)) {
								// We have already added to BB_rtls
								rtl = nullptr;  // Discard the call semantics
								break;
							}

							BB_rtls->push_back(rtl);
							auto bb = cfg->newBB(BB_rtls, CALL);

							// Add this non computed call site to the set of call sites which need to be analysed later.
							callList.push_back(call);

							// Record the called address as the start of a new procedure if it didn't already exist.
							if (newAddr && newAddr != NO_ADDRESS && !proc->getProg()->findProc(newAddr)) {
								callList.push_back(call);
								//newProc(proc->getProg(), newAddr);
								if (Boomerang::get().traceDecoder)
									LOG << "p0x" << std::hex << newAddr << std::dec << "\t";
							}

							// Check if this is the _exit or exit function. May prevent us from attempting to decode
							// invalid instructions, and getting invalid stack height errors
							const char *name = pBF->getSymbolByAddress(newAddr);
							if (!name
							 && call->getDest()->isMemOf()
							 && call->getDest()->getSubExp1()->isIntConst()) {
								ADDRESS a = ((Const *)call->getDest()->getSubExp1())->getInt();
								if (pBF->isDynamicLinkedProcPointer(a))
									name = pBF->getDynamicProcName(a);
							}
							if (name && noReturnCallDest(name)) {
								sequentialDecode = false;
							} else {
								if (call->isReturnAfterCall()) {
									appendSyntheticReturn(bb, proc);
									sequentialDecode = false;
								} else {
									// Add the fall through edge
									cfg->addOutEdge(bb, addr + inst.numBytes);
								}
							}
						}

						extraProcessCall(call, BB_rtls);

						// Create the list of RTLs for the next basic block and continue with the next instruction.
						BB_rtls = nullptr;
					}
					break;

				case STMT_RET:
					{
						auto bb = createReturnBlock(proc, BB_rtls, rtl);
						BB_rtls = nullptr;
						sequentialDecode = false;
					}
					break;

				case STMT_BOOLASSIGN:
					// This is just an ordinary instruction; no control transfer
					// Fall through
				case STMT_JUNCTION:
					// FIXME: Do we need to do anything here?
				case STMT_ASSIGN:
				case STMT_PHIASSIGN:
				case STMT_IMPASSIGN:
				case STMT_IMPREF:
					// Do nothing
					break;

				} // switch (s->getKind())
			}
			if (BB_rtls && rtl)
				// If non null, we haven't put this RTL into a the current BB as yet
				BB_rtls->push_back(rtl);

			if (inst.reDecode)
				// Special case: redecode the last instruction, without advancing addr by numBytes
				continue;
			addr += inst.numBytes;
			if (lastAddr < addr)
				lastAddr = addr;

			// If sequentially decoding, check if the next address happens to be the start of an existing BB. If so,
			// finish off the current BB (if any RTLs) as a fallthrough, and no need to decode again (unless it's an
			// incomplete BB, then we do decode it).
			// In fact, mustn't decode twice, because it will muck up the coverage, but also will cause subtle problems
			// like add a call to the list of calls to be processed, then delete the call RTL (e.g. Pentium 134.perl
			// benchmark)
			if (sequentialDecode && cfg->existsBB(addr)) {
				// Create the fallthrough BB, if there are any RTLs at all
				if (BB_rtls) {
					// Add an out edge to this address
					auto bb = cfg->newBB(BB_rtls, FALL);
					BB_rtls = nullptr;
					cfg->addOutEdge(bb, addr);
				}
				// Pick a new address to decode from, if the BB is complete
				if (!cfg->isIncomplete(addr))
					sequentialDecode = false;
			}
		} // while sequentialDecode
	} // while cfg->dequeue() != NO_ADDRESS

	// Add the callees to the set of CallStatements, and also to the Prog object
	for (const auto &call : callList) {
		auto dest = call->getFixedDest();
		// Don't speculatively decode procs that are outside of the main text section, apart from dynamically
		// linked ones (in the .plt)
		if (pBF->isDynamicLinkedProc(dest) || !spec || (dest < pBF->getLimitTextHigh())) {
			// Don't visit the destination of a register call
			Proc *np = call->getDestProc();
			if (!np && dest != NO_ADDRESS) {
				//np = newProc(proc->getProg(), dest);
				np = proc->getProg()->setNewProc(dest);
			}
			if (np) {
				proc->addCallee(np);
			}
		}
	}

	Boomerang::get().alert_decode_proc(proc, startAddr, lastAddr, nTotalBytes);

	if (VERBOSE)
		LOG << "finished processing proc " << proc->getName() << " at address 0x" << std::hex << proc->getNativeAddress() << std::dec << "\n";

	return true;
}

/*
 * \brief Add a synthetic return instruction and basic block (or a branch to
 * the existing return instruction).
 *
 * \param callBB  The call BB that will be followed by the return or jump.
 * \param proc    The enclosing UserProc.
 *
 * \note The call BB should be created with one out edge (the return or branch
 * BB).
 */
void
FrontEnd::appendSyntheticReturn(BasicBlock *callBB, UserProc *proc)
{
	auto cfg = proc->getCFG();
	auto pret = createReturnBlock(proc, nullptr, new RTL(callBB->getLastRtl()->getAddress() + 1, new ReturnStatement()));
	cfg->addOutEdge(callBB, pret);
}

/**
 * \brief Create a Return or a Oneway BB if a return statement already exists.
 *
 * \param proc     The enclosing UserProc.
 * \param BB_rtls  List of RTLs for the current BB (not including rtl).
 * \param rtl      The current RTL with the semantics for the return statement
 *                 (including a ReturnStatement as the last statement)
 * \returns        Pointer to the newly created BB.
 */
BasicBlock *
FrontEnd::createReturnBlock(UserProc *proc, std::list<RTL *> *BB_rtls, RTL *rtl)
{
	auto cfg = proc->getCFG();
	BasicBlock *bb;
	// Add the RTL to the list; this has the semantics for the return instruction as well as the ReturnStatement
	// The last Statement may get replaced with a GotoStatement
	if (!BB_rtls) BB_rtls = new std::list<RTL *>;  // In case no other semantics
	BB_rtls->push_back(rtl);
	auto s = proc->getTheReturnStatement();
	if (!s) {
		s = (ReturnStatement *)rtl->getList().back();
		proc->setTheReturnStatement(s);
		bb = cfg->newBB(BB_rtls, RET);
	} else {
		// We want to replace the *whole* RTL with a branch to THE first return's RTL. There can sometimes be extra
		// semantics associated with a return (e.g. Pentium return adds to the stack pointer before setting %pc and
		// branching). Other semantics (e.g. SPARC returning a value as part of the restore instruction) are assumed to
		// appear in a previous RTL. It is assumed that THE return statement will have the same semantics (NOTE: may
		// not always be valid). To avoid this assumption, we need branches to statements, not just to native addresses
		// (RTLs).
		auto retBB = cfg->findRetNode();
		assert(retBB);
		auto retRTL = retBB->getRTLWithStatement(s);
		assert(retRTL);
		auto retAddr = retRTL->getAddress();
		if (retRTL->getList().size() == 1)
			// ret node has no semantics, clearly we need to keep ours
			rtl->deleteLastStmt();
		else
			rtl->clear();
		rtl->appendStmt(new GotoStatement(retAddr));
		bb = cfg->newBB(BB_rtls, ONEWAY);
		// Visit the return instruction. This will be needed in most cases to split the return BB (if it has other
		// instructions before the return instruction).
		cfg->visit(retAddr, bb);
		cfg->addOutEdge(bb, retAddr);
	}
	return bb;
}

/**
 * Adds the destination of a branch to the queue of address that must be
 * decoded (if this destination has not already been visited).
 *
 * \param dest       The destination being branched to.
 * \param newBB      The new basic block delimited by the branch instruction.
 *                   May be nullptr if this block has been built before.
 * \param cfg        The CFG of the current procedure.
 *
 * \par Side Effect
 * newBB may be changed if the destination of the branch is in the middle of
 * an existing BB.  It will then be changed to point to a new BB beginning
 * with the dest.
 */
void
FrontEnd::handleBranch(ADDRESS dest, BasicBlock *&newBB, Cfg *cfg)
{
	if (dest < pBF->getLimitTextHigh()) {
		cfg->visit(dest, newBB);
		cfg->addOutEdge(newBB, dest);
	} else {
		std::cerr << "Error: branch to " << std::hex << dest << std::dec << " goes beyond section.\n";
	}
}

/**
 * Called when a switch has been identified.  Visits the destinations of the
 * switch, adds out edges to the BB, etc.
 *
 * \note Used to be called as soon as a switch statement is discovered, but
 * this causes decoded but unanalysed BBs (statements not numbered, locations
 * not SSA renamed, etc.) to appear in the CFG.  This caused problems when
 * there were nested switch statements.  Now only called when re-decoding a
 * switch statement.
 *
 * \param proc  Pointer to the UserProc object for this code.
 */
void
FrontEnd::processSwitch(BasicBlock *&newBB, UserProc *proc)
{
	auto lastStmt = (CaseStatement *)newBB->getLastRtl()->getHlStmt();
	SWITCH_INFO *si = lastStmt->getSwitchInfo();

	if (DEBUG_SWITCH) {
		LOG << "processing switch statement type " << si->chForm << " with table at 0x" << std::hex << si->uTable << std::dec << ", ";
		if (si->iNumTable)
			LOG << si->iNumTable << " entries, ";
		LOG << "lo= " << si->iLower << ", hi= " << si->iUpper << "\n";
	}
	// Emit an NWAY BB instead of the COMPJUMP.
	newBB->updateType(NWAY);

	Cfg *cfg = proc->getCFG();
	// Where there are repeated switch cases, we have repeated out-edges from the BB. Example:
	// switch (x) {
	//   case 3: case 5:
	//      do something;
	//      break;
	//   case 4: case 10:
	//      do something else
	// ... }
	// The switch statement is emitted assuming one out-edge for each switch value, which is assumed to be iLower+i
	// for the ith zero-based case. It may be that the code for case 5 above will be a goto to the code for case 3,
	// but a smarter back end could group them
	std::list<ADDRESS> dests;
	int iNum = si->iUpper - si->iLower + 1;
	for (int i = 0; i < iNum; ++i) {
		ADDRESS uSwitch;
		// Get the destination address from the switch table.
		if (si->chForm == 'H') {
			int iValue = pBF->readNative4(si->uTable + i*2);
			if (iValue == -1) continue;
			uSwitch = pBF->readNative4(si->uTable + i*8 + 4);
		} else if (si->chForm == 'F') {
			uSwitch = ((int *)si->uTable)[i];
		} else {
			uSwitch = pBF->readNative4(si->uTable + i*4);
		}
		if (si->chForm == 'O' || si->chForm == 'R' || si->chForm == 'r')
			// Offset: add table address to make a real pointer to code.  For type R, the table is relative to the
			// branch, so take iOffset. For others, iOffset is 0, so no harm
			uSwitch += si->uTable - si->iOffset;
		if (uSwitch < pBF->getLimitTextHigh()) {
			//cfg->visit(uSwitch, newBB);
			cfg->addOutEdge(newBB, uSwitch);
			// Remember to decode the newly discovered switch code arms, if necessary
			// Don't do it right now, in case there are recursive switch statements (e.g. app7win.exe from
			// hackthissite.org)
			dests.push_back(uSwitch);
		} else {
			LOG << "switch table entry branches to past end of text section 0x" << std::hex << uSwitch << std::dec << "\n";
#if 1
			// TMN: If we reached an array entry pointing outside the program text, we can be quite confident the array
			// has ended. Don't try to pull any more data from it.
			LOG << "Assuming the end of the pointer-array has been reached at index " << i << "\n";
			// TODO: Elevate this logic to the code calculating iNumTable, but still leave this code as a safeguard.
			break;
#endif
		}
	}
	// Decode the newly discovered switch code arms, if any, and if not already decoded
	int count = 0;
	for (const auto &dest : dests) {
		char tmp[1024];
		sprintf(tmp, "before decoding fragment %i of %i (%x)", ++count, dests.size(), dest);
		Boomerang::get().alert_decompile_debug_point(proc, tmp);
		decodeFragment(proc, dest);
	}
}
