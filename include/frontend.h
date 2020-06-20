/**
 * \file
 * \brief Contains the definition for the FrontEnd class
 *
 * \authors
 * Copyright (C) 1998-2005, The University of Queensland
 * \authors
 * Copyright (C) 2000-2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef FRONTEND_H
#define FRONTEND_H

#include "BinaryFile.h"
#include "sigenum.h"   // For enums platform and cc
#include "types.h"

#include <list>
#include <map>
#include <string>
#include <vector>

class BasicBlock;
class CallStatement;
class Cfg;
struct DecodeResult;
class Exp;
class NJMCDecoder;
class Prog;
class RTL;
class Signature;
class UserProc;


#if 0 // Cruft?
/**
 * \brief Control flow types.
 */
enum INSTTYPE {
	I_UNCOND,       ///< unconditional branch
	I_COND,         ///< conditional branch
	I_N_COND,       ///< case branch
	I_CALL,         ///< procedure call
	I_RET,          ///< return
	I_COMPJUMP,     ///< computed jump
	I_COMPCALL      ///< computed call
};
#endif


/**
 * The FrontEnd class implements the source independent parts of the front
 * end:  Decoding machine instructions into a control flow graph populated
 * with low- and high-level RTLs.
 */
class FrontEnd {
protected:
	FrontEnd(BinaryFile *, Prog *);
	virtual ~FrontEnd() = default;
public:
	static FrontEnd *open(const char *, Prog *);
	static FrontEnd *open(BinaryFile *, Prog *);
	static void close(FrontEnd *);

#ifdef DYNAMIC
private:
	// Needed by FrontEnd::close to destroy an instance and unload its library.
	typedef FrontEnd *(*constructFcn)(BinaryFile *, Prog *);
	typedef void (*destructFcn)(FrontEnd *);
	void *dlHandle = nullptr;
	destructFcn destruct = nullptr;
#endif

protected:
	//const int NOP_SIZE;         ///< Size of a no-op instruction (in bytes)
	//const int NOP_INST;         ///< No-op pattern
	BinaryFile *pBF;            ///< The binary file.
	Prog *prog;                 ///< The Prog object.

	/// Public map from function name (string) to signature.
	std::map<std::string, Signature *> librarySignatures;
	/// Map from address to meaningful name.
	std::map<ADDRESS, std::string> refHints;
	/// Map from address to previously decoded RTLs for decoded indirect control transfer instructions.
	std::map<ADDRESS, RTL *> previouslyDecoded;

public:
	/// Add a symbol to the loader.
	void addSymbol(ADDRESS addr, const std::string &nam) { pBF->addSymbol(addr, nam); }

	/// Add a "hint" that an instruction at the given address references a named global.
	void addRefHint(ADDRESS addr, const std::string &nam) { refHints[addr] = nam; }

	/**
	 * Add an RTL to the map from native address to
	 * previously-decoded-RTLs.  Used to restore case statements and
	 * decoded indirect call statements in a new decode following analysis
	 * of such instructions.  The CFG is incomplete in these cases, and
	 * needs to be restarted from scratch.
	 */
	void addDecodedRtl(ADDRESS a, RTL *rtl) { previouslyDecoded[a] = rtl; }

	BinaryFile *getBinaryFile() const { return pBF; }

	bool isWin32() const;

	/// Returns an enum identifer for this frontend's platform.
	virtual platform getFrontEndId() const = 0;
	//virtual std::vector<Exp *> &getDefaultParams() = 0;
	//virtual std::vector<Exp *> &getDefaultReturns() = 0;
	const char *getRegName(int);
	int         getRegSize(int);

	void readLibrarySignatures(const std::string &, callconv);
	void readLibraryCatalog(const std::string &);
	void readLibraryCatalog();

	Signature *getLibSignature(const std::string &) const;
	Signature *getDefaultSignature(const std::string &) const;
	static bool noReturnCallDest(const std::string &);

	virtual ADDRESS getMainEntryPoint(bool &);
	std::vector<ADDRESS> getEntryPoints();

	virtual DecodeResult &decodeInstruction(ADDRESS);

	void decode();
	void decode(ADDRESS);
	//void decodeOnly(ADDRESS);
	void decodeFragment(UserProc *, ADDRESS);

	virtual bool processProc(ADDRESS, UserProc *, bool = false, bool = false);

private:
	/// Accessor function to get the decoder.
	virtual NJMCDecoder &getDecoder() = 0;

	/**
	 * Given the dest of a call, determine if this is a machine specific
	 * helper function with special semantics.  If so, return true and set
	 * the semantics in lrtl.  addr is the native address of the call
	 * instruction.
	 */
	virtual bool helperFunc(std::list<RTL *> &rtls, ADDRESS addr, ADDRESS dest) { return false; }

	virtual void extraProcessCall(CallStatement *call, std::list<RTL *> *BB_rtls) { }

	void appendSyntheticReturn(BasicBlock *, UserProc *);

protected:
	BasicBlock *createReturnBlock(UserProc *, std::list<RTL *> *, RTL *);

	void handleBranch(ADDRESS, BasicBlock *&, Cfg *);
	void processSwitch(BasicBlock *&, UserProc *);
};


#if 0 // Cruft?
/*==============================================================================
 * These functions are the machine specific parts of the front end. They consist
 * of those that actually drive the decoding and analysis of the procedures of
 * the program being translated.
 * These functions are implemented in the files front<XXX> where XXX is a
 * platform name such as sparc or pentium.
 *============================================================================*/

/*
 * Intialise the procedure decoder and analyser.
 */
void initFront();

/*
 * This decodes a given procedure. It performs the analysis to recover switch statements, call
 * parameters and return types etc.
 * If keep is false, discard the decoded procedure (only need this to find code other than main that is
 * reachable from _start, for coverage and speculative decoding)
 * If spec is true, then we are speculatively decoding (i.e. if there is an illegal instruction, we just bail
 * out)
 */
bool decodeProc(ADDRESS uAddr, FrontEnd &fe, bool keep = true, bool spec = false);
#endif

#endif
