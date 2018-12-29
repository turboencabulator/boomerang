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
#include "log.h"
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
 * \param pBF   Pointer to the BinaryFile object (loader).
 * \param prog  Program being decoded.
 */
FrontEnd::FrontEnd(BinaryFile *pBF, Prog *prog) :
	pBF(pBF),
	prog(prog)
{
}

FrontEnd::~FrontEnd()
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

#else
	FrontEnd *fe;
	switch (machine) {
	case MACHINE_PENTIUM: fe = new PentiumFrontEnd(bf, prog); break;
	case MACHINE_SPARC:   fe = new   SparcFrontEnd(bf, prog); break;
	case MACHINE_PPC:     fe = new     PPCFrontEnd(bf, prog); break;
	case MACHINE_ST20:    fe = new    ST20FrontEnd(bf, prog); break;
	case MACHINE_MIPS:    fe = new    MIPSFrontEnd(bf, prog); break;
	default:
		std::cerr << "Machine architecture not supported!\n";
		return nullptr;
	}
#endif

	prog->setFrontEnd(fe);
	return fe;
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
 * \brief Returns a symbolic name for a register index.
 */
const char *
FrontEnd::getRegName(int idx)
{
	for (const auto &reg : getDecoder().getRTLDict().RegMap)
		if (reg.second == idx)
			return reg.first.c_str();
	return nullptr;
}

int
FrontEnd::getRegSize(int idx)
{
	const auto &map = getDecoder().getRTLDict().DetRegMap;
	auto it = map.find(idx);
	if (it != map.end())
		return it->second.g_size();
	return 32;
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
FrontEnd::noReturnCallDest(const char *name)
{
	return (strcmp(name, "_exit") == 0
	     || strcmp(name, "exit") == 0
	     || strcmp(name, "ExitProcess") == 0
	     || strcmp(name, "abort") == 0
	     || strcmp(name, "_assert") == 0);
}

/**
 * \brief Read library signatures from a catalog.
 */
void
FrontEnd::readLibraryCatalog(const std::string &sPath)
{
	std::ifstream inf(sPath);
	if (!inf.good()) {
		std::cerr << "can't open `" << sPath << "'\n";
		exit(1);
	}

	while (!inf.eof()) {
		std::string sFile;
		std::getline(inf, sFile);
		std::string::size_type j = sFile.find('#');
		if (j != sFile.npos)
			sFile.erase(j);
		j = sFile.find_last_not_of(" \t\n\v\f\r");
		if (j != sFile.npos)
			sFile.erase(j + 1);
		else
			continue;
		std::string sPath = Boomerang::get()->getProgPath() + "signatures/" + sFile;
		callconv cc = CONV_C;  // Most APIs are C calling convention
		if (sFile == "windows.h") cc = CONV_PASCAL;    // One exception
		if (sFile == "mfc.h")     cc = CONV_THISCALL;  // Another exception
		readLibrarySignatures(sPath, cc);
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
	std::string path = Boomerang::get()->getProgPath() + "signatures/";
	readLibraryCatalog(path + "common.hs");
	readLibraryCatalog(path + Signature::platformName(getFrontEndId()) + ".hs");
	if (isWin32()) {
		readLibraryCatalog(path + "win32.hs");
	}
}

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
 * \brief Decode all undecoded procedures and return a new program containing
 * them.
 */
void
FrontEnd::decode(Prog *prog, bool decodeMain, const char *pname)
{
	if (pname)
		prog->setName(pname);

	if (!decodeMain)
		return;

	Boomerang::get()->alert_start_decode(pBF->getLimitTextLow(), pBF->getLimitTextHigh() - pBF->getLimitTextLow());

	bool gotMain;
	ADDRESS a = getMainEntryPoint(gotMain);
	if (VERBOSE)
		LOG << "start: " << a << " gotmain: " << (gotMain ? "true" : "false") << "\n";
	if (a == NO_ADDRESS) {
		std::vector<ADDRESS> entrypoints = getEntryPoints();
		for (const auto &entrypoint : entrypoints)
			decode(prog, entrypoint);
		return;
	}

	decode(prog, a);
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
						LOG << "no proc found for address " << a << "\n";
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
FrontEnd::decode(Prog *prog, ADDRESS a)
{
	if (a != NO_ADDRESS) {
		prog->setNewProc(a);
		if (VERBOSE)
			LOG << "starting decode at address " << a << "\n";
		if (auto proc = prog->findProc(a)) {
			if (proc->isLib()) {
				LOG << "NOT decoding library proc at address 0x" << a << "\n";
				return;
			}
			auto p = (UserProc *)proc;
			processProc(a, p);
			p->setDecoded();
		} else {
			if (VERBOSE)
				LOG << "no proc found at address " << a << "\n";
			return;
		}

	} else {  // a == NO_ADDRESS
		bool change = true;
		while (change) {
			change = false;
			PROGMAP::const_iterator it;
			for (Proc *pProc = prog->getFirstProc(it); pProc; pProc = prog->getNextProc(it)) {
				if (pProc->isLib()) continue;
				auto p = (UserProc *)pProc;
				if (p->isDecoded()) continue;

				// undecoded userproc.. decode it
				change = true;
				int res = processProc(p->getNativeAddress(), p);
				if (res == 1)
					p->setDecoded();
				else
					break;
				// Break out of the loops if not decoding children
				if (Boomerang::get()->noDecodeChildren)
					break;
			}
			if (Boomerang::get()->noDecodeChildren)
				break;
		}
	}
	prog->wellForm();
}

/**
 * \brief Decode one proc starting at a given address in a given program.
 *
 * \param a  Should be the address of a UserProc.
 */
void
FrontEnd::decodeOnly(Prog *prog, ADDRESS a)
{
	auto proc = prog->setNewProc(a);
	assert(!proc->isLib());
	auto p = (UserProc *)proc;
	if (processProc(p->getNativeAddress(), p))
		p->setDecoded();
	prog->wellForm();
}

/**
 * \brief Decode a fragment of a procedure, e.g. for each destination of a
 * switch statement.
 */
void
FrontEnd::decodeFragment(UserProc *proc, ADDRESS a)
{
	if (Boomerang::get()->traceDecoder)
		LOG << "decoding fragment at 0x" << a << "\n";
	processProc(a, proc, true);
}

DecodeResult &
FrontEnd::decodeInstruction(ADDRESS pc)
{
	if (!pBF->getSectionInfoByAddr(pc)) {
		LOG << "ERROR: attempted to decode outside any known segment " << pc << "\n";
		static DecodeResult invalid;
		invalid.reset();
		invalid.valid = false;
		return invalid;
	}
	return getDecoder().decodeInstruction(pc, pBF);
}

/**
 * \brief Read the library signatures from a file.
 *
 * \param sPath  The file to read from.
 * \param cc     The calling convention assumed.
 */
void
FrontEnd::readLibrarySignatures(const std::string &sPath, callconv cc)
{
	std::ifstream ifs(sPath);
	if (!ifs.good()) {
		std::cerr << "can't open `" << sPath << "'\n";
		exit(1);
	}

	AnsiCParser p(ifs, false);
	platform plat = getFrontEndId();
	p.yyparse(plat, cc);
	ifs.close();

	for (const auto &sig : p.signatures) {
#if 0
		std::cerr << "readLibrarySignatures from " << sPath << ": " << sig->getName() << "\n";
#endif
		librarySignatures[sig->getName()] = sig;
		sig->setSigFile(sPath);
	}
}

/**
 * \brief Return a signature that matches the architecture best.
 */
Signature *
FrontEnd::getDefaultSignature(const char *name)
{
	// Get a default library signature
	if (isWin32())
		return Signature::instantiate(PLAT_PENTIUM, CONV_PASCAL, name);
	else
		return Signature::instantiate(getFrontEndId(), CONV_C, name);
}

/**
 * \brief Lookup a library signature by name.
 */
Signature *
FrontEnd::getLibSignature(const char *name)
{
	Signature *signature;
	// Look up the name in the librarySignatures map
	auto it = librarySignatures.find(name);
	if (it == librarySignatures.end()) {
		LOG << "Unknown library function " << name << "\n";
		signature = getDefaultSignature(name);
	} else {
		// Don't clone here; cloned in CallStatement::setSigArguments
		signature = it->second;
		signature->setUnknown(false);
	}
	return signature;
}

/**
 * \brief Process a procedure, given a native (source machine) address.
 *
 * This is the main function for decoding a procedure.  It is usually
 * overridden in the derived class to do source machine specific things.
 *
 * \param uAddr  The address at which the procedure starts.
 * \param pProc  The procedure object.
 * \param frag   If true, we are decoding only a fragment of a procedure
 *               (e.g. each arm of a switch statement is decoded).
 * \param spec   If true, this is a speculative decode
 *               (so give up on any invalid instruction).
 *
 * \note This is a sort of generic front end.  For many processors, this will
 * be overridden in the FrontEnd derived class, sometimes calling this
 * function to do most of the work.  Sparc is an exception.
 *
 * \returns true on a good decode (no illegal instructions).
 */
bool
FrontEnd::processProc(ADDRESS uAddr, UserProc *pProc, bool frag /* = false */, bool spec /* = false */)
{
	BasicBlock *pBB;  // Pointer to the current basic block

	// just in case you missed it
	Boomerang::get()->alert_new(pProc);

	// We have a set of CallStatement pointers. These may be disregarded if this is a speculative decode
	// that fails (i.e. an illegal instruction is found). If not, this set will be used to add to the set of calls
	// to be analysed in the cfg, and also to call newProc()
	std::list<CallStatement *> callList;

	// Indicates whether or not the next instruction to be decoded is the lexical successor of the current one.
	// Will be true for all NCTs and for CTIs with a fall through branch.
	bool sequentialDecode = true;

	Cfg *pCfg = pProc->getCFG();

	// If this is a speculative decode, the second time we decode the same address, we get no cfg. Else an error.
	if (spec && !pCfg)
		return false;
	assert(pCfg);

	// Initialise the queue of control flow targets that have yet to be decoded.
	targetQueue.initial(uAddr);

	// Clear the pointer used by the caller prologue code to access the last call rtl of this procedure
	//getDecoder().resetLastCall();

	// ADDRESS initAddr = uAddr;
	int nTotalBytes = 0;
	ADDRESS startAddr = uAddr;
	ADDRESS lastAddr = uAddr;

	while ((uAddr = targetQueue.nextAddress(pCfg)) != NO_ADDRESS) {
		// The list of RTLs for the current basic block
		auto BB_rtls = new std::list<RTL *>();

		// Keep decoding sequentially until a CTI without a fall through branch is decoded
		//ADDRESS start = uAddr;
		DecodeResult inst;
		while (sequentialDecode) {

			// Decode and classify the current source instruction
			if (Boomerang::get()->traceDecoder)
				LOG << "*" << uAddr << "\t";

			// Decode the inst at uAddr.
			inst = decodeInstruction(uAddr);

			// If invalid and we are speculating, just exit
			if (spec && !inst.valid)
				return false;

			// Need to construct a new list of RTLs if a basic block has just been finished but decoding is
			// continuing from its lexical successor
			if (!BB_rtls)
				BB_rtls = new std::list<RTL *>();

			RTL *pRtl = inst.rtl;
			if (!inst.valid) {
				// Alert the watchers to the problem
				Boomerang::get()->alert_baddecode(uAddr);

				// An invalid instruction. Most likely because a call did not return (e.g. call _exit()), etc.
				// Best thing is to emit a INVALID BB, and continue with valid instructions
				if (VERBOSE) {
					LOG << "Warning: invalid instruction at " << uAddr << ": ";
					// Emit the next 4 bytes for debugging
					for (int ii = 0; ii < 4; ++ii)
						LOG << (unsigned)(pBF->readNative1(uAddr + ii) & 0xFF) << " ";
					LOG << "\n";
				}
				// Emit the RTL anyway, so we have the address and maybe some other clues
				BB_rtls->push_back(new RTL(uAddr));
				pBB = pCfg->newBB(BB_rtls, INVALID, 0);
				sequentialDecode = false; BB_rtls = nullptr; continue;
			}

			// alert the watchers that we have decoded an instruction
			Boomerang::get()->alert_decode(uAddr, inst.numBytes);
			nTotalBytes += inst.numBytes;

			// Check if this is an already decoded jump instruction (from a previous pass with propagation etc)
			// If so, we throw away the just decoded RTL (but we still may have needed to calculate the number
			// of bytes.. ick.)
			auto ff = previouslyDecoded.find(uAddr);
			if (ff != previouslyDecoded.end())
				pRtl = ff->second;

			if (!pRtl) {
				// This can happen if an instruction is "cancelled", e.g. call to __main in a hppa program
				// Just ignore the whole instruction
				if (inst.numBytes > 0)
					uAddr += inst.numBytes;
				continue;
			}

			// Display RTL representation if asked
			if (Boomerang::get()->printRtl)
				LOG << *pRtl;

			ADDRESS uDest;

			// For each Statement in the RTL
			//std::list<Statement*>& sl = pRtl->getList();
			std::list<Statement *> sl = pRtl->getList();
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
				s->setProc(pProc);  // let's do this really early!
				auto it = refHints.find(pRtl->getAddress());
				if (it != refHints.end()) {
					const char *nam = it->second.c_str();
					ADDRESS gu = prog->getGlobalAddr(nam);
					if (gu != NO_ADDRESS) {
						s->searchAndReplace(new Const((int)gu), new Unary(opAddrOf, Location::global(nam, pProc)));
					}
				}
				s->simplify();
				auto stmt_jump = static_cast<GotoStatement *>(s);

				// Check for a call to an already existing procedure (including self recursive jumps), or to the PLT
				// (note that a LibProc entry for the PLT function may not yet exist)
				if (s->getKind() == STMT_GOTO) {
					ADDRESS dest = stmt_jump->getFixedDest();
					if (dest != NO_ADDRESS) {
						Proc *proc = prog->findProc(dest);
						if (!proc) {
							if (pBF->isDynamicLinkedProc(dest))
								proc = prog->setNewProc(dest);
						}
						if (proc && proc != (Proc *)-1) {
							auto call = new CallStatement(dest);
							s = call;
							call->setDestProc(proc);
							call->setReturnAfterCall(true);
							// also need to change it in the actual RTL
							auto ss1 = ss;
							++ss1;
							assert(ss1 == sl.end());
							pRtl->replaceLastStmt(s);
							*ss = s;
						}
					}
				}

				switch (s->getKind()) {

				case STMT_GOTO:
					{
						uDest = stmt_jump->getFixedDest();

						// Handle one way jumps and computed jumps separately
						if (uDest != NO_ADDRESS) {

							BB_rtls->push_back(pRtl);
							sequentialDecode = false;

							pBB = pCfg->newBB(BB_rtls, ONEWAY, 1);
							BB_rtls = nullptr;  // Clear when make new BB

							// Exit the switch now if the basic block already existed
							if (!pBB) {
								break;
							}

							// Add the out edge if it is to a destination within the
							// procedure
							if (uDest < pBF->getLimitTextHigh()) {
								targetQueue.visit(pCfg, uDest, pBB);
								pCfg->addOutEdge(pBB, uDest, true);
							} else {
								LOG << "Error: Instruction at " << uAddr
								    << " branches beyond end of section, to " << uDest << "\n";
							}
						}
					}
					break;

				case STMT_CASE:
					{
						Exp *pDest = stmt_jump->getDest();
						if (!pDest) {  // Happens if already analysed (now redecoding)
							// SWITCH_INFO *psi = ((CaseStatement *)stmt_jump)->getSwitchInfo();
							BB_rtls->push_back(pRtl);
							pBB = pCfg->newBB(BB_rtls, NWAY, 0);  // processSwitch will update num outedges
							pBB->processSwitch(pProc);      // decode arms, set out edges, etc
							sequentialDecode = false;       // Don't decode after the jump
							BB_rtls = nullptr;              // New RTLList for next BB
							break;                          // Just leave it alone
						}
						// Check for indirect calls to library functions, especially in Win32 programs
						if (pDest->isMemOf()
						 && pDest->getSubExp1()->isIntConst()
						 && pBF->isDynamicLinkedProcPointer(((Const *)pDest->getSubExp1())->getAddr())) {
							if (VERBOSE)
								LOG << "jump to a library function: " << *stmt_jump << ", replacing with a call/ret.\n";
							// jump to a library function
							// replace with a call ret
							std::string func = pBF->getDynamicProcName(((Const *)pDest->getSubExp1())->getAddr());
							auto call = new CallStatement;
							call->setDest(pDest->clone());
							LibProc *lp = pProc->getProg()->getLibraryProc(func.c_str());
							if (!lp)
								LOG << "getLibraryProc returned nullptr, aborting\n";
							assert(lp);
							call->setDestProc(lp);
							BB_rtls->push_back(new RTL(pRtl->getAddress(), call));
							pBB = pCfg->newBB(BB_rtls, CALL, 1);
							appendSyntheticReturn(pBB, pProc, pRtl);
							sequentialDecode = false;
							BB_rtls = nullptr;
							if (pRtl->getAddress() == pProc->getNativeAddress()) {
								// it's a thunk
								// Proc *lp = prog->findProc(func.c_str());
								func = std::string("__imp_") + func;
								pProc->setName(func.c_str());
								//lp->setName(func.c_str());
								Boomerang::get()->alert_update_signature(pProc);
							}
							callList.push_back(call);
							ss = sl.end(); --ss;  // get out of the loop
							break;
						}
						BB_rtls->push_back(pRtl);
						// We create the BB as a COMPJUMP type, then change to an NWAY if it turns out to be a switch stmt
						pBB = pCfg->newBB(BB_rtls, COMPJUMP, 0);
						LOG << "COMPUTED JUMP at " << uAddr << ", pDest = " << *pDest << "\n";
						if (Boomerang::get()->noDecompile) {
							// try some hacks
							if (pDest->isMemOf()
							 && pDest->getSubExp1()->getOper() == opPlus
							 && pDest->getSubExp1()->getSubExp2()->isIntConst()) {
								// assume subExp2 is a jump table
								ADDRESS jmptbl = ((Const *)pDest->getSubExp1()->getSubExp2())->getInt();
								unsigned int i;
								for (i = 0; ; ++i) {
									ADDRESS uDest = pBF->readNative4(jmptbl + i * 4);
									if (pBF->getLimitTextLow() <= uDest && uDest < pBF->getLimitTextHigh()) {
										LOG << "  guessed uDest " << uDest << "\n";
										targetQueue.visit(pCfg, uDest, pBB);
										pCfg->addOutEdge(pBB, uDest, true);
									} else
										break;
								}
								pBB->updateType(NWAY, i);
							}
						}
						sequentialDecode = false;
						BB_rtls = nullptr;  // New RTLList for next BB
					}
					break;

				case STMT_BRANCH:
					{
						uDest = stmt_jump->getFixedDest();
						BB_rtls->push_back(pRtl);
						pBB = pCfg->newBB(BB_rtls, TWOWAY, 2);

						// Stop decoding sequentially if the basic block already existed otherwise complete the basic block
						if (!pBB)
							sequentialDecode = false;
						else {

							// Add the out edge if it is to a destination within the procedure
							if (uDest < pBF->getLimitTextHigh()) {
								targetQueue.visit(pCfg, uDest, pBB);
								pCfg->addOutEdge(pBB, uDest, true);
							} else {
								LOG << "Error: Instruction at " << uAddr
								    << " branches beyond end of section, to " << uDest << "\n";
							}

							// Add the fall-through outedge
							pCfg->addOutEdge(pBB, uAddr + inst.numBytes);
						}

						// Create the list of RTLs for the next basic block and continue with the next instruction.
						BB_rtls = nullptr;
					}
					break;

				case STMT_CALL:
					{
						auto call = static_cast<CallStatement *>(s);

						// Check for a dynamic linked library function
						if (call->getDest()->isMemOf()
						 && call->getDest()->getSubExp1()->isIntConst()
						 && pBF->isDynamicLinkedProcPointer(((Const *)call->getDest()->getSubExp1())->getAddr())) {
							// Dynamic linked proc pointers are treated as static.
							const char *nam = pBF->getDynamicProcName(((Const *)call->getDest()->getSubExp1())->getAddr());
							Proc *p = pProc->getProg()->getLibraryProc(nam);
							call->setDestProc(p);
							call->setIsComputed(false);
						}

						// Is the called function a thunk calling a library function?
						// A "thunk" is a function which only consists of: "GOTO library_function"
						if (call && call->getFixedDest() != NO_ADDRESS) {
							// Get the address of the called function.
							ADDRESS callAddr = call->getFixedDest();
							// It should not be in the PLT either, but getLimitTextHigh() takes this into account
							if (callAddr < pBF->getLimitTextHigh()) {
								// Decode it.
								DecodeResult decoded = decodeInstruction(callAddr);
								if (decoded.valid) { // is the instruction decoded succesfully?
									// Yes, it is. Create a Statement from it.
									auto first_statement = decoded.rtl->getList().front();
									if (first_statement) {
										first_statement->setProc(pProc);
										first_statement->simplify();
										auto stmt_jump = static_cast<GotoStatement *>(first_statement);
										// In fact it's a computed (looked up) jump, so the jump seems to be a case
										// statement.
										if (first_statement->getKind() == STMT_CASE
										 && stmt_jump->getDest()->isMemOf()
										 && stmt_jump->getDest()->getSubExp1()->isIntConst()
										 && pBF->isDynamicLinkedProcPointer(((Const *)stmt_jump->getDest()->getSubExp1())->getAddr())) {  // Is it an "DynamicLinkedProcPointer"?
											// Yes, it's a library function. Look up it's name.
											ADDRESS a = ((Const *)stmt_jump->getDest()->getSubExp1())->getAddr();
											const char *nam = pBF->getDynamicProcName(a);
											// Assign the proc to the call
											Proc *p = pProc->getProg()->getLibraryProc(nam);
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
							BB_rtls->push_back(pRtl);
							pBB = pCfg->newBB(BB_rtls, COMPCALL, 1);

							// Stop decoding sequentially if the basic block already
							// existed otherwise complete the basic block
							if (!pBB)
								sequentialDecode = false;
							else
								pCfg->addOutEdge(pBB, uAddr + inst.numBytes);
							// Add this call to the list of calls to analyse. We won't
							// be able to analyse it's callee(s), of course.
							callList.push_back(call);
						} else {  // Static call
							// Find the address of the callee.
							ADDRESS uNewAddr = call->getFixedDest();

							// Calls with 0 offset (i.e. call the next instruction) are simply pushing the PC to the
							// stack. Treat these as non-control flow instructions and continue.
							if (uNewAddr == uAddr + inst.numBytes)
								break;

							// Call the virtual helper function. If implemented, will check for machine specific funcion
							// calls
							if (helperFunc(*BB_rtls, uAddr, uNewAddr)) {
								// We have already added to BB_rtls
								pRtl = nullptr;  // Discard the call semantics
								break;
							}

							BB_rtls->push_back(pRtl);

							// Add this non computed call site to the set of call sites which need to be analysed later.
							//pCfg->addCall(call);
							callList.push_back(call);

							// Record the called address as the start of a new procedure if it didn't already exist.
							if (uNewAddr && uNewAddr != NO_ADDRESS && !pProc->getProg()->findProc(uNewAddr)) {
								callList.push_back(call);
								//newProc(pProc->getProg(), uNewAddr);
								if (Boomerang::get()->traceDecoder)
									LOG << "p" << uNewAddr << "\t";
							}

							// Check if this is the _exit or exit function. May prevent us from attempting to decode
							// invalid instructions, and getting invalid stack height errors
							const char *name = pBF->getSymbolByAddress(uNewAddr);
							if (!name
							 && call->getDest()->isMemOf()
							 && call->getDest()->getSubExp1()->isIntConst()) {
								ADDRESS a = ((Const *)call->getDest()->getSubExp1())->getInt();
								if (pBF->isDynamicLinkedProcPointer(a))
									name = pBF->getDynamicProcName(a);
							}
							if (name && noReturnCallDest(name)) {
								// Make sure it has a return appended (so there is only one exit from the function)
								//call->setReturnAfterCall(true);  // I think only the Sparc frontend cares
								// Create the new basic block
								pBB = pCfg->newBB(BB_rtls, CALL, 1);
								appendSyntheticReturn(pBB, pProc, pRtl);

								// Stop decoding sequentially
								sequentialDecode = false;
							} else {
								// Create the new basic block
								pBB = pCfg->newBB(BB_rtls, CALL, 1);

								if (call->isReturnAfterCall()) {
									// Constuct the RTLs for the new basic block
									auto rtls = new std::list<RTL *>();
									// The only RTL in the basic block is one with a ReturnStatement
									rtls->push_back(new RTL(pRtl->getAddress() + 1, new ReturnStatement()));

									auto returnBB = pCfg->newBB(rtls, RET, 0);
									// Add out edge from call to return
									pCfg->addOutEdge(pBB, returnBB);
									// Put a label on the return BB (since it's an orphan); a jump will be reqd
									pCfg->setLabel(returnBB);
									pBB->setJumpReqd();
									// Mike: do we need to set return locations?
									// This ends the function
									sequentialDecode = false;
								} else {
									// Add the fall through edge if the block didn't
									// already exist
									if (pBB)
										pCfg->addOutEdge(pBB, uAddr + inst.numBytes);
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
						// Stop decoding sequentially
						sequentialDecode = false;

						pBB = createReturnBlock(pProc, BB_rtls, pRtl);

						// Create the list of RTLs for the next basic block and
						// continue with the next instruction.
						BB_rtls = nullptr;  // New RTLList for next BB
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
			if (BB_rtls && pRtl)
				// If non null, we haven't put this RTL into a the current BB as yet
				BB_rtls->push_back(pRtl);

			if (inst.reDecode)
				// Special case: redecode the last instruction, without advancing uAddr by numBytes
				continue;
			uAddr += inst.numBytes;
			if (uAddr > lastAddr)
				lastAddr = uAddr;

			// If sequentially decoding, check if the next address happens to be the start of an existing BB. If so,
			// finish off the current BB (if any RTLs) as a fallthrough, and no need to decode again (unless it's an
			// incomplete BB, then we do decode it).
			// In fact, mustn't decode twice, because it will muck up the coverage, but also will cause subtle problems
			// like add a call to the list of calls to be processed, then delete the call RTL (e.g. Pentium 134.perl
			// benchmark)
			if (sequentialDecode && pCfg->existsBB(uAddr)) {
				// Create the fallthrough BB, if there are any RTLs at all
				if (BB_rtls) {
					// Add an out edge to this address
					if (auto pBB = pCfg->newBB(BB_rtls, FALL, 1)) {
						pCfg->addOutEdge(pBB, uAddr);
						BB_rtls = nullptr;  // Need new list of RTLs
					}
				}
				// Pick a new address to decode from, if the BB is complete
				if (!pCfg->isIncomplete(uAddr))
					sequentialDecode = false;
			}
		} // while sequentialDecode

		// Add this range to the coverage
		//pProc->addRange(start, uAddr);

		// Must set sequentialDecode back to true
		sequentialDecode = true;

	} // while nextAddress() != NO_ADDRESS

#if 0
	ProgWatcher *w = prog->getWatcher();
	if (w)
		w->alert_done(pProc, initAddr, lastAddr, nTotalBytes);
#endif

	// Add the callees to the set of CallStatements, and also to the Prog object
	for (const auto &call : callList) {
		ADDRESS dest = call->getFixedDest();
		// Don't speculatively decode procs that are outside of the main text section, apart from dynamically
		// linked ones (in the .plt)
		if (pBF->isDynamicLinkedProc(dest) || !spec || (dest < pBF->getLimitTextHigh())) {
			pCfg->addCall(call);
			// Don't visit the destination of a register call
			Proc *np = call->getDestProc();
			if (!np && dest != NO_ADDRESS) {
				//np = newProc(pProc->getProg(), dest);
				np = pProc->getProg()->setNewProc(dest);
			}
			if (np) {
				np->setFirstCaller(pProc);
				pProc->addCallee(np);
			}
		}
	}

	Boomerang::get()->alert_decode(pProc, startAddr, lastAddr, nTotalBytes);

	if (VERBOSE)
		LOG << "finished processing proc " << pProc->getName() << " at address " << pProc->getNativeAddress() << "\n";

	return true;
}

/**
 * \brief Visit a destination as a label, i.e. check whether we need to queue
 * it as a new BB to create later.
 *
 * \note At present, it is important to visit an address BEFORE an out edge is
 * added to that address.  This is because adding an out edge enters the
 * address into the Cfg's BB map, and it looks like the BB has already been
 * visited, and it gets overlooked. It would be better to have a scheme
 * whereby the order of calling these functions (i.e. visit() and
 * addOutEdge()) did not matter.
 *
 * \param pCfg      The enclosing CFG.
 * \param uNewAddr  The address to be checked.
 * \param pNewBB    Set to the lower part of the BB if the address already
 *                  exists as a non explicit label
 *                  (i.e. the BB has to be split).
 */
void
TargetQueue::visit(Cfg *pCfg, ADDRESS uNewAddr, BasicBlock *&pNewBB)
{
	// Find out if we've already parsed the destination
	bool bParsed = pCfg->label(uNewAddr, pNewBB);
	// Add this address to the back of the local queue,
	// if not already processed
	if (!bParsed) {
		targets.push(uNewAddr);
		if (Boomerang::get()->traceDecoder)
			LOG << ">" << uNewAddr << "\t";
	}
}

/**
 * \brief Seed the queue with an initial address.
 *
 * Provide an initial address (can call several times if there are several
 * entry points).
 *
 * \note Can be some targets already in the queue now.
 *
 * \param uAddr  Native address to seed the queue with.
 */
void
TargetQueue::initial(ADDRESS uAddr)
{
	targets.push(uAddr);
}

/**
 * \brief Return the next target from the queue of non-processed targets.
 *
 * \param cfg  The enclosing CFG.
 * \returns    The next address to process,
 *             or NO_ADDRESS if none (queue is empty).
 */
ADDRESS
TargetQueue::nextAddress(Cfg *cfg)
{
	while (!targets.empty()) {
		ADDRESS address = targets.front();
		targets.pop();
		if (Boomerang::get()->traceDecoder)
			LOG << "<" << address << "\t";

		// If no label there at all, or if there is a BB, it's incomplete, then we can parse this address next
		if (!cfg->existsBB(address) || cfg->isIncomplete(address))
			return address;
	}
	return NO_ADDRESS;
}

/**
 * \brief Get a Prog object (mainly for testing and not decoding).
 *
 * \returns Pointer to a Prog object (with pFE and pBF filled in).
 */
Prog *
FrontEnd::getProg() const
{
	return prog;
}

/**
 * \brief Create a Return or a Oneway BB if a return statement already exists.
 *
 * \param pProc    Pointer to enclosing UserProc.
 * \param BB_rtls  List of RTLs for the current BB (not including pRtl).
 * \param pRtl     Pointer to the current RTL with the semantics for the
 *                 return statement (including a ReturnStatement as the last
 *                 statement)
 * \returns        Pointer to the newly created BB.
 */
BasicBlock *
FrontEnd::createReturnBlock(UserProc *pProc, std::list<RTL *> *BB_rtls, RTL *pRtl)
{
	Cfg *pCfg = pProc->getCFG();
	BasicBlock *pBB;
	// Add the RTL to the list; this has the semantics for the return instruction as well as the ReturnStatement
	// The last Statement may get replaced with a GotoStatement
	if (!BB_rtls) BB_rtls = new std::list<RTL *>;  // In case no other semantics
	BB_rtls->push_back(pRtl);
	ADDRESS retAddr = pProc->getTheReturnAddr();
	// LOG << "retAddr = " << retAddr << " rtl = " << pRtl->getAddress() << "\n";
	if (retAddr == NO_ADDRESS) {
		// Create the basic block
		pBB = pCfg->newBB(BB_rtls, RET, 0);
		auto s = pRtl->getList().back();  // The last statement should be the ReturnStatement
		pProc->setTheReturnAddr((ReturnStatement *)s, pRtl->getAddress());
	} else {
		// We want to replace the *whole* RTL with a branch to THE first return's RTL. There can sometimes be extra
		// semantics associated with a return (e.g. Pentium return adds to the stack pointer before setting %pc and
		// branching). Other semantics (e.g. SPARC returning a value as part of the restore instruction) are assumed to
		// appear in a previous RTL. It is assumed that THE return statement will have the same semantics (NOTE: may
		// not always be valid). To avoid this assumption, we need branches to statements, not just to native addresses
		// (RTLs).
		BasicBlock *retBB = pProc->getCFG()->findRetNode();
		assert(retBB);
		if (retBB->getFirstStmt()->isReturn())
			// ret node has no semantics, clearly we need to keep ours
			pRtl->deleteLastStmt();
		else
			pRtl->clear();
		pRtl->appendStmt(new GotoStatement(retAddr));
		try {
			pBB = pCfg->newBB(BB_rtls, ONEWAY, 1);
			// if BB already exists but is incomplete, exception is thrown
			pCfg->addOutEdge(pBB, retAddr, true);
			// Visit the return instruction. This will be needed in most cases to split the return BB (if it has other
			// instructions before the return instruction).
			targetQueue.visit(pCfg, retAddr, pBB);
		} catch (Cfg::BBAlreadyExistsError &) {
			if (VERBOSE)
				LOG << "not visiting " << retAddr << " due to exception\n";
		}
	}
	return pBB;
}

/*
 * \brief Add a synthetic return instruction and basic block (or a branch to
 * the existing return instruction).
 *
 * \param pCallBB  A pointer to the call BB that will be followed by the
 *                 return or jump.
 * \param pProc    Pointer to the enclosing UserProc.
 * \param pRtl     Pointer to the current RTL with the call instruction.
 *
 * \note The call BB should be created with one out edge (the return or branch
 * BB).
 */
void
FrontEnd::appendSyntheticReturn(BasicBlock *pCallBB, UserProc *pProc, RTL *pRtl)
{
	auto ret_rtls = new std::list<RTL *>();
	BasicBlock *pret = createReturnBlock(pProc, ret_rtls, new RTL(pRtl->getAddress() + 1, new ReturnStatement()));
	pret->addInEdge(pCallBB);
	pCallBB->setOutEdge(0, pret);
}
