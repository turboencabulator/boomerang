/**
 * \file
 * \brief Contains routines to manage the decoding of pentium instructions and
 *        the instantiation to RTLs.
 *
 * These functions replace frontend.cpp for decoding pentium instructions.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
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

#include "pentiumfrontend.h"

#include "boomerang.h"
#include "cfg.h"
#include "exp.h"
#include "log.h"
#include "proc.h"
#include "prog.h"
#include "rtl.h"
#include "signature.h"
#include "type.h"
#include "types.h"

#include <cstring>
#include <cassert>

#define FSW 40  // Numeric registers
#define AH 12

PentiumFrontEnd::PentiumFrontEnd(BinaryFile *pBF, Prog *prog) :
	FrontEnd(pBF, prog),
	decoder(prog)
{
}

PentiumFrontEnd::~PentiumFrontEnd()
{
}

/**
 * Return true if the given Statement is an assignment that stores the FSW
 * (Floating point Status Word) reg.
 *
 * \param s  Ptr to the given Statement.
 * \returns  true if it is.
 */
bool
PentiumFrontEnd::isStoreFsw(Statement *s)
{
	if (!s->isAssign()) return false;
	Exp *rhs = ((Assign *)s)->getRight();
	Exp *result;
	bool res = rhs->search(Location::regOf(FSW), result);
	return res;
}

/**
 * Return true if the given RTL is a decrement of register AH.
 *
 * \param r  Ptr to the given RTL.
 * \returns  true if it is.
 */
bool
PentiumFrontEnd::isDecAh(RTL *r)
{
	// Check for decrement; RHS of middle Exp will be r[12]{8} - 1
	if (r->getList().size() != 3) return false;
	auto mid = *(++r->getList().begin());
	if (!mid->isAssign()) return false;
	Assign *asgn = (Assign *)mid;
	Exp *rhs = asgn->getRight();
	Binary ahm1(opMinus,
	            new Binary(opSize,
	                       new Const(8),
	                       Location::regOf(12)),
	            new Const(1));
	return *rhs == ahm1;
}

/**
 * Return true if the given Statement is a setX instruction.
 *
 * \param s  Ptr to the given Statement.
 * \returns  true if it is.
 */
bool
PentiumFrontEnd::isSetX(Statement *s)
{
	// Check for SETX, i.e. <exp> ? 1 : 0
	// i.e. ?: <exp> Const 1 Const 0
	if (!s->isAssign()) return false;
	Assign *asgn = (Assign *)s;
	Exp *lhs = asgn->getLeft();
	// LHS must be a register
	if (!lhs->isRegOf()) return false;
	Exp *rhs = asgn->getRight();
	if (rhs->getOper() != opTern) return false;
	Exp *s2 = ((Ternary *)rhs)->getSubExp2();
	Exp *s3 = ((Ternary *)rhs)->getSubExp3();
	if (!s2->isIntConst() || s3->isIntConst()) return false;
	return ((Const *)s2)->getInt() == 1 && ((Const *)s3)->getInt() == 0;
}

/**
 * Return true if the given Statement is an expression whose RHS is a
 * ?: ternary.
 *
 * \param s  Ptr to the given Statement.
 * \returns  true if it is.
 */
bool
PentiumFrontEnd::isAssignFromTern(Statement *s)
{
	if (!s->isAssign()) return false;
	Assign *asgn = (Assign *)s;
	Exp *rhs = asgn->getRight();
	return rhs->getOper() == opTern;
}

/**
 * Finds a subexpression within this expression of the form
 * r[ int x] where min <= x <= max, and replaces it with
 * r[ int y] where y = min + (x - min + delta & mask).
 *
 * \param e      Expression to modify.
 * \param min    Minimum register number before any change is considered.
 * \param max    Maximum register number before any change is considered.
 * \param delta  Amount to bump up the register number by.
 * \param mask   See above.
 *
 * \par Application
 * Used to "flatten" stack floating point arithmetic (e.g. Pentium floating
 * point code).  If registers are not replaced "all at once" like this, there
 * can be subtle errors from re-replacing already replaced registers.
 */
void
PentiumFrontEnd::bumpRegisterAll(Exp *e, int min, int max, int delta, int mask)
{
	std::list<Exp **> li;
	Exp *exp = e;
	// Use doSearch, which is normally an internal method of Exp, to avoid problems of replacing the wrong
	// subexpression (in some odd cases)
	Exp::doSearch(Location::regOf(new Terminal(opWild)), exp, li, false);
	for (const auto &sub : li) {
		int reg = ((Const *)((Unary *)*sub)->getSubExp1())->getInt();
		if ((min <= reg) && (reg <= max)) {
			// Replace the K in r[ K] with a new K
			// *sub is a reg[K]
			Const *K = (Const *)((Unary *)*sub)->getSubExp1();
			K->setInt(min + ((reg - min + delta) & mask));
		}
	}
}

bool
PentiumFrontEnd::processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag /* = false */, bool spec /* = false */)
{
	// Call the base class to do most of the work
	if (!FrontEnd::processProc(uAddr, pProc, os, frag, spec))
		return false;

	// Need a post-cfg pass to remove the FPUSH and FPOP instructions, and to transform various code after floating
	// point compares to generate floating point branches.
	// processFloatCode() will recurse to process its out-edge BBs (if not already processed)
	Cfg *pCfg = pProc->getCFG();
	pCfg->unTraverse();  // Reset all the "traversed" flags (needed soon)
	// This will get done twice; no harm
	pProc->setEntryBB();

	processFloatCode(pCfg);

	int tos = 0;
	processFloatCode(pProc->getEntryBB(), tos, pCfg);

	// Process away %rpt and %skip
	processStringInst(pProc);

	// Process code for side effects of overlapped registers
	processOverlapped(pProc);

	return true;
}

std::vector<Exp *> &
PentiumFrontEnd::getDefaultParams()
{
	static std::vector<Exp *> params;
	if (params.empty()) {
		params.push_back(Location::regOf(24/*eax*/));
		params.push_back(Location::regOf(25/*ecx*/));
		params.push_back(Location::regOf(26/*edx*/));
		params.push_back(Location::regOf(27/*ebx*/));
		params.push_back(Location::regOf(28/*esp*/));
		params.push_back(Location::regOf(29/*ebp*/));
		params.push_back(Location::regOf(30/*esi*/));
		params.push_back(Location::regOf(31/*edi*/));
		params.push_back(Location::memOf(Location::regOf(28)));
	}
	return params;
}

std::vector<Exp *> &
PentiumFrontEnd::getDefaultReturns()
{
	static std::vector<Exp *> returns;
	if (returns.empty()) {
		returns.push_back(Location::regOf(24/*eax*/));
		returns.push_back(Location::regOf(25/*ecx*/));
		returns.push_back(Location::regOf(26/*edx*/));
		returns.push_back(Location::regOf(27/*ebx*/));
		returns.push_back(Location::regOf(28/*esp*/));
		returns.push_back(Location::regOf(29/*ebp*/));
		returns.push_back(Location::regOf(30/*esi*/));
		returns.push_back(Location::regOf(31/*edi*/));
		returns.push_back(Location::regOf(32/*st0*/));
		returns.push_back(Location::regOf(33/*st1*/));
		returns.push_back(Location::regOf(34/*st2*/));
		returns.push_back(Location::regOf(35/*st3*/));
		returns.push_back(Location::regOf(36/*st4*/));
		returns.push_back(Location::regOf(37/*st5*/));
		returns.push_back(Location::regOf(38/*st6*/));
		returns.push_back(Location::regOf(39/*st7*/));
		returns.push_back(new Terminal(opPC));
	}
	return returns;
}

/**
 * Little simpler, just replaces FPUSH and FPOP with more complex semantics.
 */
void
PentiumFrontEnd::processFloatCode(Cfg *pCfg)
{
	for (const auto &pBB : *pCfg) {
		Statement *st;

		// Loop through each RTL this BB
		std::list<RTL *> *BB_rtls = pBB->getRTLs();
		if (!BB_rtls) {
			// For example, incomplete BB
			return;
		}
		auto rit = BB_rtls->begin();
		while (rit != BB_rtls->end()) {
			for (int i = 0; i < (*rit)->getNumStmt(); ++i) {
				// Get the current Exp
				st = (*rit)->elementAt(i);
				if (st->isFpush()) {
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::tempOf(new Const("tmpD9")),
					                              Location::regOf(39)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(39),
					                              Location::regOf(38)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(38),
					                              Location::regOf(37)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(37),
					                              Location::regOf(36)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(36),
					                              Location::regOf(35)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(35),
					                              Location::regOf(34)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(34),
					                              Location::regOf(33)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(33),
					                              Location::regOf(32)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(32),
					                              Location::tempOf(new Const("tmpD9"))), i++);
					// Remove the FPUSH
					(*rit)->deleteStmt(i);
					--i;
					continue;
				} else if (st->isFpop()) {
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::tempOf(new Const("tmpD9")),
					                              Location::regOf(32)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(32),
					                              Location::regOf(33)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(33),
					                              Location::regOf(34)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(34),
					                              Location::regOf(35)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(35),
					                              Location::regOf(36)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(36),
					                              Location::regOf(37)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(37),
					                              Location::regOf(38)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(38),
					                              Location::regOf(39)), i++);
					(*rit)->insertStmt(new Assign(new FloatType(80),
					                              Location::regOf(39),
					                              Location::tempOf(new Const("tmpD9"))), i++);
					// Remove the FPOP
					(*rit)->deleteStmt(i);
					--i;
					continue;
				}
			}
			++rit;
		}
	}
}

/**
 * Process a basic block, and all its successors, for floating point code.
 * Remove FPUSH/FPOP, instead decrementing or incrementing respectively the
 * tos value to be used from here down.
 *
 * \note tos has to be a parameter, not a global, to get the right value at
 * any point in the call tree.
 *
 * \param pBB  Pointer to the current BB.
 * \param tos  Reference to the value of the "top of stack" pointer currently.
 *             Starts at zero, and is decremented to 7 with the first load, so
 *             r[39] should be used first, then r[38] etc.  However, it is
 *             reset to 0 for calls, so that if a function returns a float,
 *             then it will always appear in r[32].
 */
void
PentiumFrontEnd::processFloatCode(BasicBlock *pBB, int &tos, Cfg *pCfg)
{
	Statement *st;

	// Loop through each RTL this BB
	std::list<RTL *> *BB_rtls = pBB->getRTLs();
	if (!BB_rtls) {
		// For example, incomplete BB
		return;
	}
	auto rit = BB_rtls->begin();
	while (rit != BB_rtls->end()) {
		// Check for call.
		if ((*rit)->isCall()) {
			// Reset the "top of stack" index. If this is not done, then after a sequence of calls to functions
			// returning floats, the value will appear to be returned in registers r[32], then r[33], etc.
			tos = 0;
		}
		if ((*rit)->getList().empty()) { ++rit; continue; }
#if PROCESS_FNSTSW
		// Check for f(n)stsw
		if (isStoreFsw((*rit)->getList().front())) {
			// Check the register - at present we only handle AX
			Exp *lhs = ((Assign *)(*rit)->getList().front())->getLeft();
			Exp *ax = Location::regOf(0);
			assert(*lhs == *ax);
			delete ax;

			// Process it
			if (processStsw(rit, BB_rtls, pBB, pCfg)) {
				// If returned true, must abandon this BB.
				break;
			}
			// Else we have already skiped past the stsw, and/or any instructions that replace it, so process rest of
			// this BB
			continue;
		}
#endif
		for (int i = 0; i < (*rit)->getNumStmt(); ++i) {
			// Get the current Exp
			st = (*rit)->elementAt(i);
			if (!st->isFlagAssgn()) {
				// We are interested in either FPUSH/FPOP, or r[32..39] appearing in either the left or right hand
				// sides, or calls
				if (st->isFpush()) {
					tos = (tos - 1) & 7;
					// Remove the FPUSH
					(*rit)->deleteStmt(i);
					--i;  // Adjust the index
					continue;
				} else if (st->isFpop()) {
					tos = (tos + 1) & 7;
					// Remove the FPOP
					(*rit)->deleteStmt(i);
					--i;  // Adjust the index
					continue;
				} else if (st->isAssign()) {
					Assign *asgn = (Assign *)st;
					Exp *lhs = asgn->getLeft();
					Exp *rhs = asgn->getRight();
					if (tos != 0) {
						// Substitute all occurrences of r[x] (where 32 <= x <= 39) with r[y] where
						// y = 32 + (x + tos) & 7
						bumpRegisterAll(lhs, 32, 39, tos, 7);
						bumpRegisterAll(rhs, 32, 39, tos, 7);
					}
				}
			} else {
				// st is a flagcall
				// We are interested in any register parameters in the range 32 - 39
				Binary *cur;
				for (cur = (Binary *)((Assign *)st)->getRight(); !cur->isNil(); cur = (Binary *)cur->getSubExp2()) {
					// I dont understand why we want typed exps in the flag calls so much. If we're going to replace opSize with TypedExps
					// then we need to do it for everything, not just the flag calls.. so that should be in the sslparser.  If that is the
					// case then we cant assume that opLists of flag calls will always contain TypedExps, so this code is wrong.
					// - trent 9/6/2002
					//TypedExp *te = (TypedExp *)cur->getSubExp1();
					Exp *s = cur->getSubExp1();
					if (s->isRegOfK()) {
						Const *c = (Const *)((Unary *)s)->getSubExp1();
						int K = c->getInt();  // Old register number
						// Change to new register number, if in range
						if ((K >= 32) && (K <= 39))
							s->setSubExp1(new Const(32 + ((K - 32 + tos) & 7)));
					}
				}
			}
		}
		++rit;
	}
	pBB->setTraversed(true);

	// Now recurse to process my out edges, if not already processed
	const std::vector<BasicBlock *> &outs = pBB->getOutEdges();
	unsigned n;
	do {
		n = outs.size();
		for (unsigned o = 0; o < n; ++o) {
			BasicBlock *anOut = outs[o];
			if (!anOut->isTraversed()) {
				processFloatCode(anOut, tos, pCfg);
				if (outs.size() != n)
					// During the processing, we have added or more likely deleted a BB, and the vector of out edges
					// has changed.  It's safe to just start the inner for loop again
					break;
			}
		}
	} while (outs.size() != n);
}

/**
 * \brief Emit a set instruction.
 *
 * Emit Rtl of the form *8* lhs = [cond ? 1 : 0].
 * Insert before rit.
 */
void
PentiumFrontEnd::emitSet(std::list<RTL *> *BB_rtls, std::list<RTL *>::iterator &rit, ADDRESS uAddr, Exp *lhs, Exp *cond)
{
	Statement *asgn = new Assign(lhs,
	                             new Ternary(opTern,
	                                         cond,
	                                         new Const(1),
	                                         new Const(0)));
	auto pRtl = new RTL(uAddr, asgn);
	//std::cout << "Emit "; pRtl->print(); std::cout << std::endl;
	// Insert the new RTL before rit
	BB_rtls->insert(rit, pRtl);
}

/**
 * \brief Checks for pentium specific helper functions like __xtol which have
 * specific sematics.
 *
 * Check a HLCall for a helper function, and replace with appropriate
 * semantics if possible.
 *
 * \note This needs to be handled in a resourcable way.
 *
 * \param dest  The native destination of this call.
 * \param addr  The native address of this call instruction.
 * \param lrtl  Pointer to a list of RTL pointers for this BB.
 *
 * \returns true if a helper function is converted; false otherwise.
 */
bool
PentiumFrontEnd::helperFunc(ADDRESS dest, ADDRESS addr, std::list<RTL *> *lrtl)
{
	if (dest == NO_ADDRESS) return false;

	const char *p = pBF->getSymbolByAddress(dest);
	if (!p) return false;
	std::string name(p);
	// I believe that __xtol is for gcc, _ftol for earlier MSVC compilers, _ftol2 for MSVC V7
	if (name == "__xtol" || name == "_ftol" || name == "_ftol2") {
		// This appears to pop the top of stack, and converts the result to a 64 bit integer in edx:eax.
		// Truncates towards zero
		// r[tmpl] = ftoi(80, 64, r[32])
		// r[24] = trunc(64, 32, r[tmpl])
		// r[26] = r[tmpl] >> 32
		auto pRtl = new RTL(addr);
		Statement *a = new Assign(new IntegerType(64),
		                          Location::tempOf(new Const("tmpl")),
		                          new Ternary(opFtoi,
		                                      new Const(64),
		                                      new Const(32),
		                                      Location::regOf(32)));
		pRtl->appendStmt(a);
		a = new Assign(Location::regOf(24),
		               new Ternary(opTruncs,
		                           new Const(64),
		                           new Const(32),
		                           Location::tempOf(new Const("tmpl"))));
		pRtl->appendStmt(a);
		a = new Assign(Location::regOf(26),
		               new Binary(opShiftR,
		                          Location::tempOf(new Const("tmpl")),
		                          new Const(32)));
		pRtl->appendStmt(a);
		// Append this RTL to the list of RTLs for this BB
		lrtl->push_back(pRtl);
		// Return true, so the caller knows not to create a HLCall
		return true;

	} else if (name == "__mingw_allocstack") {
		auto pRtl = new RTL(addr);
		Statement *a = new Assign(Location::regOf(28),
		                          new Binary(opMinus,
		                                     Location::regOf(28),
		                                     Location::regOf(24)));
		pRtl->appendStmt(a);
		lrtl->push_back(pRtl);
		prog->removeProc(name.c_str());
		return true;
	} else if (name == "__mingw_frame_init" || name == "__mingw_cleanup_setup" || name == "__mingw_frame_end") {
		LOG << "found removable call to static lib proc " << name << " at " << addr << "\n";
		prog->removeProc(name.c_str());
		return true;
	} else {
		// Will be other cases in future
	}
	return false;
}

ADDRESS
PentiumFrontEnd::getMainEntryPoint(bool &gotMain)
{
	ADDRESS start = pBF->getMainEntryPoint();
	if (start != NO_ADDRESS) {
		gotMain = true;
		return start;
	}

	gotMain = false;
	start = pBF->getEntryPoint();
	if (start == 0 || start == NO_ADDRESS)
		return NO_ADDRESS;

	int instCount = 100;
	int conseq = 0;
	ADDRESS addr = start;

	// Look for 3 calls in a row in the first 100 instructions, with no other instructions between them.
	// This is the "windows" pattern. Another windows pattern: call to GetModuleHandleA followed by
	// a push of eax and then the call to main.  Or a call to __libc_start_main
	ADDRESS dest;
	do {
		DecodeResult inst = decodeInstruction(addr);
		if (!inst.rtl)
			// Must have gotten out of step
			break;
		CallStatement *cs = nullptr;
		if (!inst.rtl->getList().empty())
			cs = (CallStatement *)(inst.rtl->getList().back());
		if (cs
		 && cs->getKind() == STMT_CALL
		 && cs->getDest()->isMemOf()
		 && cs->getDest()->getSubExp1()->isIntConst()
		 && pBF->isDynamicLinkedProcPointer(((Const *)cs->getDest()->getSubExp1())->getAddr())
		 && !strcmp(pBF->getDynamicProcName(((Const *)cs->getDest()->getSubExp1())->getAddr()), "GetModuleHandleA")) {
#if 0
			std::cerr << "consider " << std::hex << addr << " "
			          << pBF->getDynamicProcName(((Const *)cs->getDest()->getSubExp1())->getAddr()) << std::endl;
#endif
			int oNumBytes = inst.numBytes;
			inst = decodeInstruction(addr + oNumBytes);
			if (inst.valid && inst.rtl->getList().size() == 2) {
				auto a = dynamic_cast<Assign *>(inst.rtl->getList().back());
				if (a && *a->getRight() == *Location::regOf(24)) {
#if 0
					std::cerr << "is followed by push eax.. " << "good" << std::endl;
#endif
					inst = decodeInstruction(addr + oNumBytes + inst.numBytes);
					if (!inst.rtl->getList().empty()) {
						auto toMain = dynamic_cast<CallStatement *>(inst.rtl->getList().back());
						if (toMain && toMain->getFixedDest() != NO_ADDRESS) {
							pBF->addSymbol(toMain->getFixedDest(), "WinMain");
							gotMain = true;
							return toMain->getFixedDest();
						}
					}
				}
			}
		}
		if ((cs && cs->getKind() == STMT_CALL) && ((dest = (cs->getFixedDest())) != NO_ADDRESS)) {
			if (++conseq == 3 && 0) { // FIXME: this isn't working!
				// Success. Return the target of the last call
				gotMain = true;
				return cs->getFixedDest();
			}
			if (pBF->getSymbolByAddress(dest)
			 && strcmp(pBF->getSymbolByAddress(dest), "__libc_start_main") == 0) {
				// This is a gcc 3 pattern. The first parameter will be a pointer to main.
				// Assume it's the 5 byte push immediately preceeding this instruction
				// Note: the RTL changed recently from esp = esp-4; m[esp] = K tp m[esp-4] = K; esp = esp-4
				inst = decodeInstruction(addr - 5);
				assert(inst.valid);
				assert(inst.rtl->getList().size() == 2);
				auto a = (Assign *)inst.rtl->getList().front();  // Get m[esp-4] = K
				Exp *rhs = a->getRight();
				assert(rhs->isIntConst());
				gotMain = true;
				return (ADDRESS)((Const *)rhs)->getInt();
			}
		} else
			conseq = 0;  // Must be consequitive
		GotoStatement *gs = (GotoStatement *)cs;
		if (gs && gs->getKind() == STMT_GOTO)
			// Example: Borland often starts with a branch around some debug
			// info
			addr = gs->getFixedDest();
		else
			addr += inst.numBytes;
	} while (--instCount);

	// Last chance check: look for _main (e.g. Borland programs)
	ADDRESS umain = pBF->getAddressByName("_main");
	if (umain != NO_ADDRESS) return umain;

	// Not ideal; we must return start
	std::cerr << "main function not found\n";

	this->addSymbol(start, "_start");

	return start;
}

static void
toBranches(ADDRESS a, bool lastRtl, Cfg *cfg, RTL *rtl, BasicBlock *bb, Cfg::iterator &it)
{
	auto br1 = new BranchStatement;
	assert(rtl->getList().size() >= 4);  // They vary; at least 5 or 6
	auto s1 = rtl->getList().front();
	auto s6 = rtl->getList().back();
	if (s1->isAssign())
		br1->setCondExpr(((Assign *)s1)->getRight());
	else
		br1->setCondExpr(nullptr);
	br1->setDest(a + 2);
	auto br2 = new BranchStatement;
	if (s6->isAssign())
		br2->setCondExpr(((Assign *)s6)->getRight());
	else
		br2->setCondExpr(nullptr);
	br2->setDest(a);
	cfg->splitForBranch(bb, rtl, br1, br2, it);
}

/**
 * \brief Process away %rpt and %skip in string instructions.
 */
void
PentiumFrontEnd::processStringInst(UserProc *proc)
{
	Cfg *cfg = proc->getCFG();
	// For each BB this proc
	for (auto it = cfg->begin(); it != cfg->end(); /* no increment! */) {
		bool noinc = false;
		BasicBlock *bb = *it;
		std::list<RTL *> *rtls = bb->getRTLs();
		if (!rtls)
			break;
		ADDRESS addr = 0;
		bool lastRtl = true;
		// For each RTL this BB
		for (const auto &rtl : *rtls) {
			//ADDRESS prev = addr;
			addr = rtl->getAddress();
			if (!rtl->getList().empty()) {
				auto firstStmt = rtl->getList().front();
				if (firstStmt->isAssign()) {
					Exp *lhs = ((Assign *)firstStmt)->getLeft();
					if (lhs->isMachFtr()) {
						Const *sub = (Const *)((Unary *)lhs)->getSubExp1();
						const char *str = sub->getStr();
						if (strncmp(str, "%SKIP", 5) == 0) {
							toBranches(addr, lastRtl, cfg, rtl, bb, it);
							noinc = true;  // toBranches inc's it
							// Abandon this BB; if there are other string instr this BB, they will appear in new BBs
							// near the end of the list
							break;
						} else
							LOG << "Unhandled machine feature " << str << "\n";
					}
				}
			}
			lastRtl = false;
		}
		if (!noinc) ++it;
	}
}

/**
 * \brief Process for overlapped registers.
 */
void
PentiumFrontEnd::processOverlapped(UserProc *proc)
{
	// first, lets look for any uses of the registers
	std::set<int> usedRegs;
	StatementList stmts;
	proc->getStatements(stmts);
	for (const auto &stmt : stmts) {
		LocationSet locs;
		stmt->addUsedLocs(locs);
		for (const auto &loc : locs) {
			if (!loc->isRegOfK())
				continue;
			int n = ((Const *)loc->getSubExp1())->getInt();
			usedRegs.insert(n);
		}
	}

	std::set<BasicBlock *> bbs;

	// For each statement, we are looking for assignments to registers in
	//   these ranges:
	// eax - ebx (24-27) (eax, ecx, edx, ebx)
	//  ax -  bx ( 0- 3) ( ax,  cx,  dx,  bx)
	//  al -  bl ( 8-11) ( al,  cl,  dl,  bl)
	//  ah -  bh (12-15) ( ah,  ch,  dh,  bh)
	// if found we want to generate assignments to the overlapping registers,
	// but only if they are used in this procedure.
	//
	// TMN: 2006-007-31. This code had been completely forgotten about:
	// esi/si, edi/di and ebp/bp. For now, let's hope we never encounter esp/sp. :-)
	// ebp (29)  bp (5)
	// esi (30)  si (6)
	// edi (31)  di (7)
	for (const auto &stmt : stmts) {
		if (stmt->getBB()->overlappedRegProcessingDone)   // never redo processing
			continue;
		bbs.insert(stmt->getBB());
		if (!stmt->isAssignment()) continue;
		Exp *lhs = ((Assignment *)stmt)->getLeft();
		if (!lhs->isRegOf()) continue;
		Const *c = (Const *)((Location *)lhs)->getSubExp1();
		assert(c->isIntConst());
		int r = c->getInt();
		int off = r & 3;        // Offset into the array of 4 registers
		int off_mod8 = r & 7;   // Offset into the array of 8 registers; for ebp, esi, edi
		Assign *a;
		switch (r) {
		case 24: case 25: case 26: case 27:
		//  eax      ecx      edx      ebx
			// Emit *16* r<off> := trunc(32, 16, r<24+off>)
			if (usedRegs.count(off)) {
				a = new Assign(new IntegerType(16),
				               Location::regOf(off),
				               new Ternary(opTruncu,
				                           new Const(32),
				                           new Const(16),
				                           Location::regOf(24 + off)));
				proc->insertStatementAfter(stmt, a);
			}

			// Emit *8* r<8+off> := trunc(32, 8, r<24+off>)
			if (usedRegs.count(8 + off)) {
				a = new Assign(new IntegerType(8),
				               Location::regOf(8 + off),
				               new Ternary(opTruncu,
				                           new Const(32),
				                           new Const(8),
				                           Location::regOf(24 + off)));
				proc->insertStatementAfter(stmt, a);
			}

			// Emit *8* r<12+off> := r<24+off>@[15:8]
			if (usedRegs.count(12 + off)) {
				a = new Assign(new IntegerType(8),
				               Location::regOf(12 + off),
				               new Ternary(opAt,
				                           Location::regOf(24 + off),
				                           new Const(15),
				                           new Const(8)));
				proc->insertStatementAfter(stmt, a);
			}
			break;

		case 0: case 1: case 2: case 3:
		//  ax      cx      dx      bx
			// Emit *32* r<24+off> := r<24+off>@[31:16] | zfill(16, 32, r<off>)
			if (usedRegs.count(24 + off)) {
				a = new Assign(new IntegerType(32),
				               Location::regOf(24 + off),
				               new Binary(opBitOr,
				                          new Ternary(opAt,
				                                      Location::regOf(24 + off),
				                                      new Const(31),
				                                      new Const(16)),
				                          new Ternary(opZfill,
				                                      new Const(16),
				                                      new Const(32),
				                                      Location::regOf(off))));
				proc->insertStatementAfter(stmt, a);
			}

			// Emit *8* r<8+off> := trunc(16, 8, r<off>)
			if (usedRegs.count(8 + off)) {
				a = new Assign(new IntegerType(8),
				               Location::regOf(8 + off),
				               new Ternary(opTruncu,
				                           new Const(16),
				                           new Const(8),
				                           Location::regOf(24 + off)));
				proc->insertStatementAfter(stmt, a);
			}

			// Emit *8* r<12+off> := r<off>@[15:8]
			if (usedRegs.count(12 + off)) {
				a = new Assign(new IntegerType(8),
				               Location::regOf(12 + off),
				               new Ternary(opAt,
				                           Location::regOf(off),
				                           new Const(15),
				                           new Const(8)));
				proc->insertStatementAfter(stmt, a);
			}
			break;

		case 8: case 9: case 10: case 11:
		//  al      cl       dl       bl
			// Emit *32* r<24+off> := r<24+off>@[31:8] | zfill(8, 32, r<8+off>)
			if (usedRegs.count(24 + off)) {
				a = new Assign(new IntegerType(32),
				               Location::regOf(24 + off),
				               new Binary(opBitOr,
				                          new Ternary(opAt,
				                                      Location::regOf(24 + off),
				                                      new Const(31),
				                                      new Const(8)),
				                          new Ternary(opZfill,
				                                      new Const(8),
				                                      new Const(32),
				                                      Location::regOf(8 + off))));
				proc->insertStatementAfter(stmt, a);
			}

			// Emit *16* r<off> := r<off>@[15:8] | zfill(8, 16, r<8+off>)
			if (usedRegs.count(off)) {
				a = new Assign(new IntegerType(16),
				               Location::regOf(off),
				               new Binary(opBitOr,
				                          new Ternary(opAt,
				                                      Location::regOf(off),
				                                      new Const(15),
				                                      new Const(8)),
				                          new Ternary(opZfill,
				                                      new Const(8),
				                                      new Const(16),
				                                      Location::regOf(8 + off))));
				proc->insertStatementAfter(stmt, a);
			}
			break;

		case 12: case 13: case 14: case 15:
		//   ah       ch       dh       bh
			// Emit *32* r<24+off> := r<24+off> & 0xFFFF00FF
			//      *32* r<24+off> := r<24+off> | r<12+off> << 8
			if (usedRegs.count(24 + off)) {
				a = new Assign(new IntegerType(32),
				               Location::regOf(24 + off),
				               new Binary(opBitOr,
				                          Location::regOf(24 + off),
				                          new Binary(opShiftL,
				                                     Location::regOf(12 + off),
				                                     new Const(8))));
				proc->insertStatementAfter(stmt, a);
				a = new Assign(new IntegerType(32),
				               Location::regOf(24 + off),
				               new Binary(opBitAnd,
				                          Location::regOf(24 + off),
				                          new Const(0xFFFF00FF)));
				proc->insertStatementAfter(stmt, a);
			}

			// Emit *16* r<off> := r<off> & 0x00FF
			//      *16* r<off> := r<off> | r<12+off> << 8
			if (usedRegs.count(off)) {
				a = new Assign(new IntegerType(16),
				               Location::regOf(off),
				               new Binary(opBitOr,
				                          Location::regOf(off),
				                          new Binary(opShiftL,
				                                     Location::regOf(12 + off),
				                                     new Const(8))));
				proc->insertStatementAfter(stmt, a);
				a = new Assign(new IntegerType(16),
				               Location::regOf(off),
				               new Binary(opBitAnd,
				                          Location::regOf(off),
				                          new Const(0x00FF)));
				proc->insertStatementAfter(stmt, a);
			}
			break;

		case 5: case 6: case 7:
		//  bp      si      di
			// Emit *32* r<24+off_mod8> := r<24+off_mod8>@[31:16] | zfill(16, 32, r<off_mod8>)
			if (usedRegs.count(24 + off_mod8)) {
				a = new Assign(new IntegerType(32),
				               Location::regOf(24 + off_mod8),
				               new Binary(opBitOr,
				                          new Ternary(opAt,
				                                      Location::regOf(24 + off_mod8),
				                                      new Const(31),
				                                      new Const(16)),
				                          new Ternary(opZfill,
				                                      new Const(16),
				                                      new Const(32),
				                                      Location::regOf(off_mod8))));
				proc->insertStatementAfter(stmt, a);
			}
			break;

		case 29: case 30: case 31:
		//  ebp      esi      edi
			// Emit *16* r<off_mod8> := trunc(32, 16, r<24+off_mod8>)
			if (usedRegs.count(off_mod8)) {
				a = new Assign(new IntegerType(16),
				               Location::regOf(off_mod8),
				               new Ternary(opTruncu,
				                           new Const(32),
				                           new Const(16),
				                           Location::regOf(24 + off_mod8)));
				proc->insertStatementAfter(stmt, a);
			}
			break;
		}
	}

	// set a flag for every BB we've processed so we don't do them again
	for (const auto &bb : bbs)
		bb->overlappedRegProcessingDone = true;
}

DecodeResult &
PentiumFrontEnd::decodeInstruction(ADDRESS pc)
{
	int n = pBF->readNative1(pc);
	if (n == (int)(char)0xee) {
		// out dx, al
		Exp *dx = Location::regOf(decoder.getRTLDict().RegMap["%dx"]);
		Exp *al = Location::regOf(decoder.getRTLDict().RegMap["%al"]);
		auto call = new CallStatement();
		call->setDestProc(prog->getLibraryProc("outp"));
		call->setArgumentExp(0, dx);
		call->setArgumentExp(1, al);
		static DecodeResult r;
		r.reset();
		r.numBytes = 1;
		r.rtl = new RTL(pc, call);
		return r;
	}
	if (n == (int)(char)0x0f && pBF->readNative1(pc + 1) == (int)(char)0x0b) {
		auto call = new CallStatement();
		call->setDestProc(prog->getLibraryProc("invalid_opcode"));
		static DecodeResult r;
		r.reset();
		r.numBytes = 2;
		r.rtl = new RTL(pc, call);
		return r;
	}
	return FrontEnd::decodeInstruction(pc);
}

/**
 * \par EXPERIMENTAL
 * Can we find function pointers in arguments to calls this early?
 */
void
PentiumFrontEnd::extraProcessCall(CallStatement *call, std::list<RTL *> *BB_rtls)
{
	if (call->getDestProc()) {

		// looking for function pointers
		Signature *calledSig = call->getDestProc()->getSignature();
		for (unsigned int i = 0; i < calledSig->getNumParams(); ++i) {
			// check param type
			Type *paramType = calledSig->getParamType(i);
			Type *points_to;
			CompoundType *compound = nullptr;
			bool paramIsFuncPointer = false, paramIsCompoundWithFuncPointers = false;
			if (paramType->resolvesToPointer()) {
				points_to = paramType->asPointer()->getPointsTo();
				if (points_to->resolvesToFunc())
					paramIsFuncPointer = true;
				else if (points_to->resolvesToCompound()) {
					compound = points_to->asCompound();
					for (unsigned int n = 0; n < compound->getNumTypes(); ++n) {
						if (compound->getType(n)->resolvesToPointer()
						 && compound->getType(n)->asPointer()->getPointsTo()->resolvesToFunc())
							paramIsCompoundWithFuncPointers = true;
					}
				}
			}
			if (!paramIsFuncPointer && !paramIsCompoundWithFuncPointers)
				continue;

			// count pushes backwards to find arg
			Exp *found = nullptr;
			unsigned int pushcount = 0;
			for (auto rrit = BB_rtls->crbegin(); rrit != BB_rtls->crend() && !found; ++rrit) {
				const auto &stmts = (*rrit)->getList();
				for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
					auto stmt = *srit;
					if (stmt->isAssign()) {
						auto asgn = (Assign *)stmt;
						if (asgn->getLeft()->isRegN(28) && asgn->getRight()->getOper() == opMinus)
							++pushcount;
						else if (pushcount == i + 2
						      && asgn->getLeft()->isMemOf()
						      && asgn->getLeft()->getSubExp1()->getOper() == opMinus
						      && asgn->getLeft()->getSubExp1()->getSubExp1()->isRegN(28)
						      && asgn->getLeft()->getSubExp1()->getSubExp2()->isIntConst()) {
							found = asgn->getRight();
							break;
						}
					}
				}
			}
			if (!found)
				continue;

			ADDRESS a;
			if (found->isIntConst())
				a = ((Const *)found)->getInt();
			else if (found->isAddrOf() && found->getSubExp1()->isGlobal()) {
				const char *name = ((Const *)found->getSubExp1()->getSubExp1())->getStr();
				if (!prog->getGlobal(name))
					continue;
				a = prog->getGlobalAddr(name);
			} else
				continue;

			// found one.
			if (paramIsFuncPointer) {
				if (VERBOSE)
					LOG << "found a new procedure at address " << a
					    << " from inspecting parameters of call to " << call->getDestProc()->getName() << ".\n";
				Proc *proc = prog->setNewProc(a);
				Signature *sig = paramType->asPointer()->getPointsTo()->asFunc()->getSignature()->clone();
				sig->setName(proc->getName());
				sig->setForced(true);
				proc->setSignature(sig);
				continue;
			}

#if 0
			// linkers putting rodata in data sections is a continual annoyance
			// we just have to assume the pointers don't change before we pass them at least once.
			if (!prog->isReadOnly(a))
				continue;
#endif

			for (unsigned int n = 0; n < compound->getNumTypes(); ++n) {
				if (compound->getType(n)->resolvesToPointer()
				 && compound->getType(n)->asPointer()->getPointsTo()->resolvesToFunc()) {
					ADDRESS d = pBF->readNative4(a);
					if (VERBOSE)
						LOG << "found a new procedure at address " << d
						    << " from inspecting parameters of call to " << call->getDestProc()->getName() << ".\n";
					Proc *proc = prog->setNewProc(d);
					Signature *sig = compound->getType(n)->asPointer()->getPointsTo()->asFunc()->getSignature()->clone();
					sig->setName(proc->getName());
					sig->setForced(true);
					proc->setSignature(sig);
				}
				a += compound->getType(n)->getSize() / 8;
			}
		}

		// some pentium specific ellipsis processing
		if (calledSig->hasEllipsis()) {
			// count pushes backwards to find a push of 0
			bool found = false;
			int pushcount = 0;
			for (auto rrit = BB_rtls->crbegin(); rrit != BB_rtls->crend() && !found; ++rrit) {
				const auto &stmts = (*rrit)->getList();
				for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
					auto stmt = *srit;
					if (stmt->isAssign()) {
						auto asgn = (Assign *)stmt;
						if (asgn->getLeft()->isRegN(28) && asgn->getRight()->getOper() == opMinus)
							++pushcount;
						else if (asgn->getLeft()->isMemOf()
						      && asgn->getLeft()->getSubExp1()->getOper() == opMinus
						      && asgn->getLeft()->getSubExp1()->getSubExp1()->isRegN(28)
						      && asgn->getLeft()->getSubExp1()->getSubExp2()->isIntConst()) {
							if (asgn->getRight()->isIntConst()) {
								int n = ((Const *)asgn->getRight())->getInt();
								if (n == 0) {
									found = true;
									break;
								}
							}
						}
					}
				}
			}
			if (found && pushcount > 1) {
				call->setSigArguments();
				call->setNumArguments(pushcount - 1);
			}
		}
	}
}

#ifdef DYNAMIC
/**
 * This function is called via dlopen/dlsym; it returns a new FrontEnd
 * derived concrete object.  After this object is returned, the virtual
 * function call mechanism will call the rest of the code in this library.
 * It needs to be C linkage so that its name is not mangled.
 */
extern "C" FrontEnd *
construct(BinaryFile *bf, Prog *prog)
{
	return new PentiumFrontEnd(bf, prog);
}
extern "C" void
destruct(FrontEnd *fe)
{
	delete (PentiumFrontEnd *)fe;
}
#endif
