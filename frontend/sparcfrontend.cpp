/**
 * \file
 * \brief Contains routines to manage the decoding of sparc instructions and
 *        the instantiation to RTLs, removing sparc dependent features such as
 *        delay slots in the process.
 *
 * These functions replace frontend.cpp for decoding sparc instructions.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2000-2001, Sun Microsystems, Inc
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

#include "sparcfrontend.h"

#include "boomerang.h"
#include "cfg.h"
#include "exp.h"
#include "proc.h"
#include "prog.h"
#include "rtl.h"

#include <iomanip>      // For std::setw
#include <sstream>

#include <cstring>
#include <cassert>

SparcFrontEnd::SparcFrontEnd(BinaryFile *pBF, Prog *prog) :
	FrontEnd(pBF, prog),
	decoder(prog)
{
	nop_inst.numBytes = 0;  // So won't disturb coverage
	nop_inst.type = NOP;
	nop_inst.valid = true;
	nop_inst.rtl = new RTL();
}

#if 0 // Cruft?
/**
 * \brief Emit a warning when encountering a DCTI couple.
 *
 * \param at    The address of the couple.
 * \param dest  The address of the first DCTI in the couple.
 */
void
SparcFrontEnd::warnDCTcouple(ADDRESS at, ADDRESS dest)
{
	std::cerr << "Error: DCTI couple at " << std::hex << at
	          << " points to delayed branch at " << dest << std::dec << "...\n"
	          << "Decompilation will likely be incorrect\n";
}
#endif

/**
 * Determines if a delay instruction is exactly the same as the instruction
 * immediately preceding the destination of a CTI; i.e. has been copied from
 * the real destination to the delay slot as an optimisation.
 *
 * \param src   The logical source address of a CTI.
 * \param dest  The logical destination address of the CTI.
 *
 * \par Side Effect
 * Optionally displays an error message if the target of the branch is the
 * delay slot of another delayed CTI.
 *
 * \returns Can optimise away the delay instruction.
 */
bool
SparcFrontEnd::optimise_DelayCopy(ADDRESS src, ADDRESS dest) const
{
	// Check that the destination is within the main test section; may not be when we speculatively decode junk
	if ((dest - 4) >= pBF->getLimitTextHigh())
		return false;
	auto delay_inst = pBF->readNative4(src + 4);
	auto inst_before_dest = pBF->readNative4(dest - 4);
	return (delay_inst == inst_before_dest);
}

/**
 * Determines if the given call and delay instruction consitute a call where
 * callee returns to the caller's caller.  That is:
 *
 *     ProcA:               ProcB:                ProcC:
 *      ...                  ...                   ...
 *      call ProcB           call ProcC            ret
 *      ...                  restore               ...
 *
 * The restore instruction in ProcB will effectively set %o7 to be %i7, the
 * address to which ProcB will return.  So in effect ProcC will return to
 * ProcA at the position just after the call to ProcB.  This is equivalent to
 * ProcC returning to ProcB which then immediately returns to ProcA.
 *
 * \note We don't set a label at the return, because we also have to force a
 * jump at the call BB, and in some cases we don't have the call BB as yet.
 * So these two are up to the caller.
 *
 * \note The name of this function is now somewhat irrelevant.  The whole
 * function is somewhat irrelevant; it dates from the times when you would
 * always find an actual restore in the delay slot.  With some patterns, this
 * is no longer true.
 *
 * \param call   The RTL for the caller (e.g. "call ProcC" above).
 * \param rtl    Pointer to the RTL for the call instruction.
 * \param delay  The RTL for the delay instruction (e.g. "restore").
 * \param cfg    The CFG of the procedure.
 *
 * \returns The basic block containing the single return instruction if this
 * optimisation applies, nullptr otherwise.
 */
BasicBlock *
SparcFrontEnd::optimise_CallReturn(CallStatement *call, RTL *rtl, RTL *delay, UserProc *pProc)
{
	if (call->isReturnAfterCall()) {
		// Constuct the RTLs for the new basic block
		auto rtls = new std::list<RTL *>();

		// The only RTL in the basic block is a ReturnStatement
		auto ls = std::list<Statement *>();
		// If the delay slot is a single assignment to %o7, we want to see the semantics for it, so that preservation
		// or otherwise of %o7 is correct
		const auto &stmts = delay->getList();
		if (stmts.size() == 1)
			if (auto as = dynamic_cast<Assign *>(stmts.front()))
				if (as->getLeft()->isRegN(15))
					ls.push_back(as);
		ls.push_back(new ReturnStatement);
		auto r = new RTL(rtl->getAddress() + 1);
		r->splice(ls);
#if 0
		rtls->push_back(r);
		Cfg *cfg = pProc->getCFG();
		auto returnBB = cfg->newBB(rtls, RET, 0);
#endif
		BasicBlock *returnBB = createReturnBlock(pProc, rtls, r);
		return returnBB;
	} else
		// May want to put code here that checks whether or not the delay instruction redefines %o7
		return nullptr;
}

/**
 * Adds the destination of a branch to the queue of address that must be
 * decoded (if this destination has not already been visited).
 *
 * \param dest       The destination being branched to.
 * \param newBB      The new basic block delimited by the branch instruction.
 *                   May be nullptr if this block has been built before.
 * \param cfg        The CFG of the current procedure.
 * \param tq         Object managing the target queue.
 *
 * \par Side Effect
 * newBB may be changed if the destination of the branch is in the middle of
 * an existing BB.  It will then be changed to point to a new BB beginning
 * with the dest.
 */
void
SparcFrontEnd::handleBranch(ADDRESS dest, BasicBlock *&newBB, Cfg *cfg, TargetQueue &tq)
{
	if (!newBB)
		return;

	if (dest < pBF->getLimitTextHigh()) {
		tq.visit(cfg, dest, newBB);
		cfg->addOutEdge(newBB, dest);
	} else
		std::cerr << "Error: branch to " << std::hex << dest << std::dec << " goes beyond section.\n";
}

/**
 * Records the fact that there is a procedure at a given address.  Also adds
 * the out edge to the lexical successor of the call site (taking into
 * consideration the delay slot and possible UNIMP instruction).
 *
 * \param dest     The address of the callee.
 * \param callBB   The basic block delimited by the call.
 * \param cfg      CFG of the enclosing procedure.
 * \param address  The address of the call instruction.
 * \param offset   the offset from the call instruction to which an outedge
 *                 must be added.  A value of 0 means no edge is to be added.
 */
void
SparcFrontEnd::handleCall(UserProc *proc, ADDRESS dest, BasicBlock *callBB, Cfg *cfg, ADDRESS address, int offset/* = 0*/)
{
	if (!callBB)
		return;

	// If the destination address is the same as this very instruction, we have a call with iDisp30 == 0. Don't treat
	// this as the start of a real procedure.
	if ((dest != address) && !proc->getProg()->findProc(dest)) {
		// We don't want to call prog.visitProc just yet, in case this is a speculative decode that failed. Instead, we
		// use the set of CallStatements (not in this procedure) that is needed by CSR
		if (Boomerang::get()->traceDecoder)
			std::cout << "p" << std::hex << dest << std::dec << "\t";
	}

	// Add the out edge if required
	if (offset != 0)
		cfg->addOutEdge(callBB, address + offset);
}

/**
 * This is the stub for cases of DCTI couples that we haven't written analysis
 * code for yet.  It simply displays an informative warning and returns.
 *
 * \param addr  The address of the first CTI in the couple.
 */
void
SparcFrontEnd::case_unhandled_stub(ADDRESS addr)
{
	std::cerr << "Error: DCTI couple at " << std::hex << addr << std::dec << std::endl;
}

/**
 * Handles a call instruction.
 *
 * \param address     The native address of the call instruction.
 * \param inst        The info summaries when decoding the call instruction.
 * \param delay_inst  The info summaries when decoding the delay instruction.
 * \param BB_rtls     The list of RTLs currently built
 *                    for the BB under construction.
 * \param proc        The enclosing procedure.
 * \param callList    A list of pointers to CallStatements
 *                    for procs yet to be processed.
 * \param isPattern   true if the call is an idiomatic pattern
 *                    (e.g. a move_call_move pattern).
 *
 * \par Side Effects
 * address may change; BB_rtls may be appended to or set to nullptr.
 *
 * \returns true if next instruction is to be fetched sequentially from this
 * one.
 */
bool
SparcFrontEnd::case_CALL(ADDRESS &address, DecodeResult &inst,
                         DecodeResult &delay_inst, std::list<RTL *> *&BB_rtls,
                         UserProc *proc, std::list<CallStatement *> &callList, bool isPattern)
{
	// Aliases for the call and delay RTLs
	auto call_stmt = (CallStatement *)inst.rtl->getList().back();
	RTL *delay_rtl = delay_inst.rtl;

	// Emit the delay instruction, unless the delay instruction is a nop, or we have a pattern, or are followed by a
	// restore
	if ((delay_inst.type != NOP) && !call_stmt->isReturnAfterCall()) {
		delay_rtl->setAddress(address);
		BB_rtls->push_back(delay_rtl);
		if (Boomerang::get()->printRtl)
			LOG << *delay_rtl;
	}

	// Get the new return basic block for the special case where the delay instruction is a restore
	BasicBlock *returnBB = optimise_CallReturn(call_stmt, inst.rtl, delay_rtl, proc);

	int disp30 = (call_stmt->getFixedDest() - address) >> 2;
	// Don't test for small offsets if part of a move_call_move pattern.
	// These patterns assign to %o7 in the delay slot, and so can't possibly be used to copy %pc to %o7
	// Similarly if followed by a restore
	if (!isPattern && !returnBB && (disp30 == 2 || disp30 == 3)) {
		// This is the idiomatic case where the destination is 1 or 2 instructions after the delayed instruction.
		// Only emit the side effect of the call (%o7 := %pc) in this case.  Note that the side effect occurs before the
		// delay slot instruction (which may use the value of %o7)
		emitCopyPC(*BB_rtls, address);
		address += disp30 << 2;
		return true;
	} else {
		assert(disp30 != 1);

		// First check for helper functions
		ADDRESS dest = call_stmt->getFixedDest();
		// Special check for calls to weird PLT entries which don't have symbols
		if (pBF->isDynamicLinkedProc(dest) && !pBF->getSymbolByAddress(dest)) {
			// This is one of those. Flag this as an invalid instruction
			inst.valid = false;
		}
		if (helperFunc(*BB_rtls, address, dest)) {
			address += 8;  // Skip call, delay slot
			return true;
		}

		// Emit the call
		BB_rtls->push_back(inst.rtl);

		// End the current basic block
		Cfg *cfg = proc->getCFG();
		auto callBB = cfg->newBB(BB_rtls, CALL, 1);
		if (!callBB)
			return false;

		// Add this call site to the set of call sites which need to be analysed later.
		// This set will be used later to call prog.visitProc (so the proc will get decoded)
		callList.push_back((CallStatement *)inst.rtl->getList().back());

		if (returnBB) {
			// Handle the call but don't add any outedges from it just yet.
			handleCall(proc, call_stmt->getFixedDest(), callBB, cfg, address);

			// Now add the out edge
			cfg->addOutEdge(callBB, returnBB);

			address += inst.numBytes;  // For coverage
			// This is a CTI block that doesn't fall through and so must
			// stop sequentially decoding
			return false;
		} else {
			// Else no restore after this call.  An outedge may be added to the lexical successor of the call which
			// will be 8 bytes ahead or in the case where the callee returns a struct, 12 bytes ahead. But the problem
			// is: how do you know this function returns a struct at decode time?
			// If forceOutEdge is set, set offset to 0 and no out-edge will be added yet
			int offset = inst.forceOutEdge ? 0 :
			             //(call_stmt->returnsStruct() ? 12 : 8);
			             // MVE: FIXME!
			             8;

			bool ret = true;
			// Check for _exit; probably should check for other "never return" functions
			const char *name = pBF->getSymbolByAddress(dest);
			if (name && strcmp(name, "_exit") == 0) {
				// Don't keep decoding after this call
				ret = false;
				// Also don't add an out-edge; setting offset to 0 will do this
				offset = 0;
				// But we have already set the number of out-edges to 1
				callBB->updateType(CALL, 0);
			}

			// Handle the call (register the destination as a proc) and possibly set the outedge.
			handleCall(proc, dest, callBB, cfg, address, offset);

			if (inst.forceOutEdge) {
				// There is no need to force a goto to the new out-edge, since we will continue decoding from there.
				// If other edges exist to the outedge, they will generate the required label
				cfg->addOutEdge(callBB, inst.forceOutEdge);
				address = inst.forceOutEdge;
			} else {
				// Continue decoding from the lexical successor
				address += offset;
			}
			BB_rtls = nullptr;

			return ret;
		}
	}
}

/**
 * Handles a non-call, static delayed (SD) instruction.
 *
 * \param address     The native address of the SD.
 * \param inst        The info summaries when decoding the SD instruction.
 * \param delay_inst  The info summaries when decoding the delay instruction.
 * \param BB_rtls     The list of RTLs currently built
 *                    for the BB under construction.
 * \param cfg         The CFG of the enclosing procedure.
 * \param tq          Object managing the target queue.
 *
 * \par Side Effects
 * address may change; BB_rtls may be appended to or set to nullptr.
 */
void
SparcFrontEnd::case_SD(ADDRESS &address, DecodeResult &inst,
                       DecodeResult &delay_inst, std::list<RTL *> *&BB_rtls,
                       Cfg *cfg, TargetQueue &tq)
{
	// Aliases for the SD and delay RTLs
	auto SD_stmt = static_cast<GotoStatement *>(inst.rtl->getList().back());
	RTL *delay_rtl = delay_inst.rtl;

	// Try the "delay instruction has been copied" optimisation, emitting the delay instruction now if the optimisation
	// won't apply
	if (delay_inst.type != NOP) {
		if (optimise_DelayCopy(address, SD_stmt->getFixedDest()))
			SD_stmt->adjustFixedDest(-4);
		else {
			// Move the delay instruction before the SD. Must update the address in case there is a branch to the SD
			delay_rtl->setAddress(address);
			BB_rtls->push_back(delay_rtl);
			// Display RTL representation if asked
			//if (progOptions.rtl)
			if (0)  // SETTINGS!
				LOG << *delay_rtl;
		}
	}

	// Update the address (for coverage)
	address += 8;

	// Add the SD
	BB_rtls->push_back(inst.rtl);

	// Add the one-way branch BB
	auto pBB = cfg->newBB(BB_rtls, ONEWAY, 1);
	if (!pBB) { BB_rtls = nullptr; return; }

	// Visit the destination, and add the out-edge
	ADDRESS uDest = SD_stmt->getFixedDest();
	handleBranch(uDest, pBB, cfg, tq);
	BB_rtls = nullptr;
}


/**
 * Handles all dynamic delayed jumps (jmpl, also dynamic calls).
 *
 * \param address     The native address of the DD.
 * \param inst        The info summaries when decoding the SD instruction.
 * \param delay_inst  The info summaries when decoding the delay instruction.
 * \param BB_rtls     The list of RTLs currently built
 *                    for the BB under construction.
 * \param tq          Object managing the target queue.
 * \param proc        Pointer to the current Proc object.
 * \param callList    A set of pointers to CallStatements
 *                    for procs yet to be processed.
 *
 * \par Side Effects
 * address may change; BB_rtls may be appended to or set to nullptr.
 *
 * \returns true if next instruction is to be fetched sequentially from this
 * one.
 */
bool
SparcFrontEnd::case_DD(ADDRESS &address, DecodeResult &inst,
                       DecodeResult &delay_inst, std::list<RTL *> *&BB_rtls,
                       TargetQueue &tq, UserProc *proc, std::list<CallStatement *> &callList)
{
	Cfg *cfg = proc->getCFG();

	if (delay_inst.type != NOP) {
		// Emit the delayed instruction, unless a NOP
		delay_inst.rtl->setAddress(address);
		BB_rtls->push_back(delay_inst.rtl);
	}

	// Set address past this instruction and delay slot (may be changed later).  This in so that we cover the
	// jmp/call and delay slot instruction, in case we return false
	address += 8;

	BasicBlock *newBB;
	bool bRet = true;
	auto lastStmt = inst.rtl->getList().back();
	switch (lastStmt->getKind()) {
	case STMT_CALL:
		// Will be a computed call
		BB_rtls->push_back(inst.rtl);
		newBB = cfg->newBB(BB_rtls, COMPCALL, 1);
		break;
	case STMT_RET:
#if 0
		newBB = cfg->newBB(BB_rtls, RET, 0);
		proc->setTheReturnAddr((ReturnStatement *)inst.rtl->getList().back(), inst.rtl->getAddress());
#endif
		newBB = createReturnBlock(proc, BB_rtls, inst.rtl);
		bRet = false;
		break;
	case STMT_CASE:
		{
			BB_rtls->push_back(inst.rtl);
			newBB = cfg->newBB(BB_rtls, COMPJUMP, 0);
			bRet = false;
			Exp *pDest = ((CaseStatement *)lastStmt)->getDest();
			if (!pDest) {  // Happens if already analysed (we are now redecoding)
				//SWITCH_INFO *psi = ((CaseStatement *)lastStmt)->getSwitchInfo();
				// processSwitch will update the BB type and number of outedges, decode arms, set out edges, etc
				newBB->processSwitch(proc);
			}
		}
		break;
	default:
		newBB = nullptr;
		break;
	}
	if (!newBB) return false;

	auto last = inst.rtl->getList().back();
	// Do extra processing for for special types of DD
	if (auto call_stmt = dynamic_cast<CallStatement *>(last)) {

		// Attempt to add a return BB if the delay instruction is a RESTORE
		BasicBlock *returnBB = optimise_CallReturn(call_stmt, inst.rtl, delay_inst.rtl, proc);
		if (returnBB) {
			cfg->addOutEdge(newBB, returnBB);

			// We have to set the epilogue for the enclosing procedure (all proc's must have an
			// epilogue) and remove the RESTORE in the delay slot that has just been pushed to the list of RTLs
			//proc->setEpilogue(new CalleeEpilogue("__dummy",std::list<std::string>()));
			// Set the return location; this is now always %o0
			//setReturnLocations(proc->getEpilogue(), 8 /* %o0 */);
			newBB->getRTLs()->remove(delay_inst.rtl);

			// Add this call to the list of calls to analyse. We won't be able to analyse its callee(s), of course.
			callList.push_back(call_stmt);

			return false;
		} else {
			// Instead, add the standard out edge to original address+8 (now just address)
			cfg->addOutEdge(newBB, address);
		}
		// Add this call to the list of calls to analyse. We won't be able to analyse its callee(s), of course.
		callList.push_back(call_stmt);
	}

	// Set the address of the lexical successor of the call that is to be decoded next and create a new list of
	// RTLs for the next basic block.
	BB_rtls = nullptr;
	return bRet;
}

/**
 * Handles all Static Conditional Delayed non-anulled branches.
 *
 * \param address     The native address of the DD.
 * \param inst        The info summaries when decoding the SD instruction.
 * \param delay_inst  The info summaries when decoding the delay instruction.
 * \param BB_rtls     The list of RTLs currently built
 *                    for the BB under construction.
 * \param cfg         The CFG of the enclosing procedure.
 * \param tq          Object managing the target queue.
 *
 * \par Side Effects
 * address may change; BB_rtls may be appended to or set to nullptr.
 *
 * \returns true if next instruction is to be fetched sequentially from this
 * one.
 */
bool
SparcFrontEnd::case_SCD(ADDRESS &address, DecodeResult &inst,
                        DecodeResult &delay_inst, std::list<RTL *> *&BB_rtls,
                        Cfg *cfg, TargetQueue &tq)
{
	auto stmt_jump = static_cast<GotoStatement *>(inst.rtl->getList().back());
	ADDRESS uDest = stmt_jump->getFixedDest();

	// Assume that if we find a call in the delay slot, it's actually a pattern such as move/call/move
// MVE: Check this! Only needed for HP PA/RISC
	bool delayPattern = delay_inst.rtl->isCall();

	if (delayPattern) {
		// Just emit the branch, and decode the instruction immediately following next.
		// Assumes the first instruction of the pattern is not used in the true leg
		BB_rtls->push_back(inst.rtl);
		auto pBB = cfg->newBB(BB_rtls, TWOWAY, 2);
		if (!pBB) return false;
		handleBranch(uDest, pBB, cfg, tq);
		// Add the "false" leg
		cfg->addOutEdge(pBB, address + 4);
		address += 4;  // Skip the SCD only
		// Start a new list of RTLs for the next BB
		BB_rtls = nullptr;
		std::cerr << "Warning: instruction at " << std::hex << address << std::dec
		          << " not copied to true leg of preceeding branch\n";
		return true;
	}

	if (!delay_inst.rtl->areFlagsAffected()) {
		// SCD; flags not affected. Put delay inst first
		if (delay_inst.type != NOP) {
			// Emit delay instr
			BB_rtls->push_back(delay_inst.rtl);
			// This is in case we have an in-edge to the branch. If the BB is split, we want the split to happen
			// here, so this delay instruction is active on this path
			delay_inst.rtl->setAddress(address);
		}
		// Now emit the branch
		BB_rtls->push_back(inst.rtl);
		auto pBB = cfg->newBB(BB_rtls, TWOWAY, 2);
		if (!pBB) return false;
		handleBranch(uDest, pBB, cfg, tq);
		// Add the "false" leg; skips the NCT
		cfg->addOutEdge(pBB, address + 8);
		// Skip the NCT/NOP instruction
		address += 8;
	} else if (optimise_DelayCopy(address, uDest)) {
		// We can just branch to the instr before uDest. Adjust the destination of the branch
		stmt_jump->adjustFixedDest(-4);
		// Now emit the branch
		BB_rtls->push_back(inst.rtl);
		auto pBB = cfg->newBB(BB_rtls, TWOWAY, 2);
		if (!pBB) return false;
		handleBranch(uDest - 4, pBB, cfg, tq);
		// Add the "false" leg: point to the delay inst
		cfg->addOutEdge(pBB, address + 4);
		address += 4;  // Skip branch but not delay
	} else { // The CCs are affected, and we can't use the copy delay slot trick
		// SCD, must copy delay instr to orphan
		// Copy the delay instruction to the dest of the branch, as an orphan. First add the branch.
		BB_rtls->push_back(inst.rtl);
		// Make a BB for the current list of RTLs. We want to do this first, else ordering can go silly
		auto pBB = cfg->newBB(BB_rtls, TWOWAY, 2);
		if (!pBB) return false;
		// Visit the target of the branch
		tq.visit(cfg, uDest, pBB);
		auto pOrphan = new std::list<RTL *>;
		pOrphan->push_back(delay_inst.rtl);
		// Change the address to 0, since this code has no source address (else we may branch to here when we want
		// to branch to the real BB with this instruction).
		// Note that you can't use an address that is a fixed function of the destination addr, because there can
		// be several jumps to the same destination that all require an orphan. The instruction in the orphan will
		// often but not necessarily be the same, so we can't use the same orphan BB. newBB knows to consider BBs
		// with address 0 as being in the map, so several BBs can exist with address 0
		delay_inst.rtl->setAddress(0);
		// Add a branch from the orphan instruction to the dest of the branch. Again, we can't even give the jumps
		// a special address like 1, since then the BB would have this getLowAddr.
		pOrphan->push_back(new RTL(0, new GotoStatement(uDest)));
		auto pOrBB = cfg->newBB(pOrphan, ONEWAY, 1);
		// Add an out edge from the orphan as well
		cfg->addOutEdge(pOrBB, uDest);
		// Add an out edge from the current RTL to the orphan.
		cfg->addOutEdge(pBB, pOrBB);
		// Add the "false" leg to the NCT
		cfg->addOutEdge(pBB, address + 4);
		// Don't skip the delay instruction, so it will be decoded next.
		address += 4;
	}
	// Start a new list of RTLs for the next BB
	BB_rtls = nullptr;
	return true;
}

/**
 * Handles all static conditional delayed anulled branches followed by an NCT
 * (but not NOP) instruction.
 *
 * \param address     The native address of the DD.
 * \param inst        The info summaries when decoding the SD instruction.
 * \param delay_inst  The info summaries when decoding the delay instruction.
 * \param BB_rtls     The list of RTLs currently built
 *                    for the BB under construction.
 * \param cfg         The CFG of the enclosing procedure.
 * \param tq          Object managing the target queue.
 *
 * \par Side Effects
 * address may change; BB_rtls may be appended to or set to nullptr.
 *
 * \returns true if next instruction is to be fetched sequentially from this
 * one.
 */
bool
SparcFrontEnd::case_SCDAN(ADDRESS &address, DecodeResult &inst,
                          DecodeResult &delay_inst, std::list<RTL *> *&BB_rtls,
                          Cfg *cfg, TargetQueue &tq)
{
	// We may have to move the delay instruction to an orphan BB, which then branches to the target of the jump.
	// Instead of moving the delay instruction to an orphan BB, we may have a duplicate of the delay instruction just
	// before the target; if so, we can branch to that and not need the orphan. We do just a binary comparison; that
	// may fail to make this optimisation if the instr has relative fields.
	auto stmt_jump = static_cast<GotoStatement *>(inst.rtl->getList().back());
	ADDRESS uDest = stmt_jump->getFixedDest();
	BasicBlock *pBB;
	if (optimise_DelayCopy(address, uDest)) {
		// Adjust the destination of the branch
		stmt_jump->adjustFixedDest(-4);
		// Now emit the branch
		BB_rtls->push_back(inst.rtl);
		pBB = cfg->newBB(BB_rtls, TWOWAY, 2);
		if (!pBB) return false;
		handleBranch(uDest - 4, pBB, cfg, tq);
	} else {  // SCDAN; must move delay instr to orphan. Assume it's not a NOP (though if it is, no harm done)
		// Move the delay instruction to the dest of the branch, as an orphan. First add the branch.
		BB_rtls->push_back(inst.rtl);
		// Make a BB for the current list of RTLs.  We want to do this first, else ordering can go silly
		pBB = cfg->newBB(BB_rtls, TWOWAY, 2);
		if (!pBB) return false;
		// Visit the target of the branch
		tq.visit(cfg, uDest, pBB);
		auto pOrphan = new std::list<RTL *>;
		pOrphan->push_back(delay_inst.rtl);
		// Change the address to 0, since this code has no source address (else we may branch to here when we want to
		// branch to the real BB with this instruction).
		delay_inst.rtl->setAddress(0);
		// Add a branch from the orphan instruction to the dest of the branch
		pOrphan->push_back(new RTL(0, new GotoStatement(uDest)));
		auto pOrBB = cfg->newBB(pOrphan, ONEWAY, 1);
		// Add an out edge from the orphan as well.
		cfg->addOutEdge(pOrBB, uDest);
		// Add an out edge from the current RTL to the orphan.
		cfg->addOutEdge(pBB, pOrBB);
	}
	// Both cases (orphan or not)
	// Add the "false" leg: point past delay inst.
	cfg->addOutEdge(pBB, address + 8);
	address += 8;       // Skip branch and delay
	BB_rtls = nullptr;  // Start new BB return true;
	return true;
}

std::vector<Exp *> &
SparcFrontEnd::getDefaultParams()
{
	static std::vector<Exp *> params;
	if (params.empty()) {
		// init arguments and return set to be all 31 machine registers
		// Important: because o registers are save in i registers, and
		// i registers have higher register numbers (e.g. i1=r25, o1=r9)
		// it helps the prover to process higher register numbers first!
		// But do r30 first (%i6, saves %o6, the stack pointer)
		params.push_back(Location::regOf(30));
		params.push_back(Location::regOf(31));
		for (int r = 29; r > 0; --r) {
			params.push_back(Location::regOf(r));
		}
	}
	return params;
}

std::vector<Exp *> &
SparcFrontEnd::getDefaultReturns()
{
	static std::vector<Exp *> returns;
	if (returns.empty()) {
		returns.push_back(Location::regOf(30));
		returns.push_back(Location::regOf(31));
		for (int r = 29; r > 0; --r) {
			returns.push_back(Location::regOf(r));
		}
	}
	return returns;
}

/**
 * Builds the CFG for a procedure out of the RTLs constructed during decoding.
 * The semantics of delayed CTIs are transformed into CTIs that aren't
 * delayed.
 *
 * \note This function overrides (and replaces) the function with the same
 * name in class FrontEnd.  The required actions are so different that the
 * base class implementation can't be re-used.
 */
bool
SparcFrontEnd::processProc(ADDRESS address, UserProc *proc, bool fragment, bool spec)
{
	// Declare an object to manage the queue of targets not yet processed yet.
	// This has to be individual to the procedure! (so not a global)
	TargetQueue targetQueue;

	// Similarly, we have a set of CallStatement pointers. These may be
	// disregarded if this is a speculative decode that fails (i.e. an illegal
	// instruction is found). If not, this set will be used to add to the set
	// of calls to be analysed in the cfg, and also to call prog.visitProc()
	std::list<CallStatement *> callList;

	// Indicates whether or not the next instruction to be decoded is the
	// lexical successor of the current one. Will be true for all NCTs and for
	// CTIs with a fall through branch.
	bool sequentialDecode = true;

	// The control flow graph of the current procedure
	Cfg *cfg = proc->getCFG();

	// If this is a speculative decode, the second time we decode the same
	// address, we get no cfg. Else an error.
	if (spec && !cfg)
		return false;
	assert(cfg);

	// Initialise the queue of control flow targets that have yet to be decoded.
	targetQueue.initial(address);

	// Get the next address from which to continue decoding and go from
	// there. Exit the loop if there are no more addresses or they all
	// correspond to locations that have been decoded.
	while ((address = targetQueue.nextAddress(cfg)) != NO_ADDRESS) {

		// The list of RTLs for the current basic block
		auto BB_rtls = new std::list<RTL *>();

		// Keep decoding sequentially until a CTI without a fall through branch
		// is decoded
		//ADDRESS start = address;
		DecodeResult inst;
		while (sequentialDecode) {

			if (Boomerang::get()->traceDecoder)
				LOG << "*0x" << std::hex << address << std::dec << "\t";

			// Check if this is an already decoded jump instruction (from a previous pass with propagation etc)
			// If so, we don't need to decode this instruction
			auto ff = previouslyDecoded.find(address);
			if (ff != previouslyDecoded.end()) {
				inst.rtl = ff->second;
				inst.valid = true;
				inst.type = DD;  // E.g. decode the delay slot instruction
			} else
				inst = decodeInstruction(address);

			// If invalid and we are speculating, just exit
			if (spec && !inst.valid)
				return false;

			// Check for invalid instructions
			if (!inst.valid) {
				std::cerr << "Invalid instruction at " << std::hex << address << ":";
				auto fill = std::cerr.fill('0');
				for (int j = 0; j < inst.numBytes; ++j)
					std::cerr << " " << std::setw(2) << pBF->readNative1(address + j);
				std::cerr << "\n" << std::dec;
				std::cerr.fill(fill);
				return false;
			}

			// Don't display the RTL here; do it after the switch statement in case the delay slot instruction is moved
			// before this one

			// Need to construct a new list of RTLs if a basic block has just been finished but decoding is continuing
			// from its lexical successor
			if (!BB_rtls)
				BB_rtls = new std::list<RTL *>();

			// Define aliases to the RTLs so that they can be treated as a high level types where appropriate.
			RTL *rtl = inst.rtl;
			GotoStatement *stmt_jump = nullptr;
			Statement *last = nullptr;
			const auto &stmts = rtl->getList();
			if (!stmts.empty()) {
				last = stmts.back();
				stmt_jump = static_cast<GotoStatement *>(last);
			}

#define BRANCH_DS_ERROR 0   // If set, a branch to the delay slot of a delayed
                            // CTI instruction is flagged as an error
#if BRANCH_DS_ERROR
			if ((last->getKind() == JUMP_RTL)
			 || (last->getKind() == STMT_CALL)
			 || (last->getKind() == JCOND_RTL)
			 || (last->getKind() == STMT_RET)) {
				ADDRESS dest = stmt_jump->getFixedDest();
				if ((dest != NO_ADDRESS) && (dest < pBF->getLimitTextHigh())) {
					unsigned inst_before_dest = pBF->readNative4(dest - 4);

					unsigned bits31_30 = inst_before_dest >> 30;
					unsigned bits23_22 = (inst_before_dest >> 22) & 3;
					unsigned bits24_19 = (inst_before_dest >> 19) & 0x3f;
					unsigned bits29_25 = (inst_before_dest >> 25) & 0x1f;
					if ((bits31_30 == 0x01) // Call
					 || ((bits31_30 == 0x02) && (bits24_19 == 0x38)) // Jmpl
					 || ((bits31_30 == 0x00) && (bits23_22 == 0x02) && (bits29_25 != 0x18))) { // Branch, but not (f)ba,a
						// The above test includes floating point branches
						std::cerr << "Target of branch at " << std::hex << rtl->getAddress()
						          << " is delay slot of CTI at " << dest - 4 << std::dec << std::endl;
					}
				}
			}
#endif

			switch (inst.type) {
			case NOP:
				// Always put the NOP into the BB. It may be needed if it is the
				// the destinsation of a branch. Even if not the start of a BB,
				// some other branch may be discovered to it later.
				BB_rtls->push_back(rtl);

				// Then increment the native address pointer
				address = address + 4;
				break;

			case NCT:
				// Ordinary instruction. Add it to the list of RTLs this BB
				BB_rtls->push_back(rtl);
				address += inst.numBytes;
				// Ret/restore epilogues are handled as ordinary RTLs now
				if (dynamic_cast<ReturnStatement *>(last))
					sequentialDecode = false;
				break;

			case SKIP:
				{
					// We can't simply ignore the skipped delay instruction as there
					// will most likely be a branch to it so we simply set the jump
					// to go to one past the skipped instruction.
					stmt_jump->setDest(address + 8);
					BB_rtls->push_back(rtl);

					// Construct the new basic block and save its destination
					// address if it hasn't been visited already
					auto pBB = cfg->newBB(BB_rtls, ONEWAY, 1);
					handleBranch(address + 8, pBB, cfg, targetQueue);

					// There is no fall through branch.
					sequentialDecode = false;
					address += 8;  // Update address for coverage
				}
				break;

			case SU:
				{
					// Ordinary, non-delay branch.
					BB_rtls->push_back(inst.rtl);

					auto pBB = cfg->newBB(BB_rtls, ONEWAY, 1);
					handleBranch(stmt_jump->getFixedDest(), pBB, cfg, targetQueue);

					// There is no fall through branch.
					sequentialDecode = false;
					address += 8;  // Update address for coverage
				}
				break;

			case SD:
				{
					// This includes "call" and "ba". If a "call", it might be a move_call_move idiom, or a call to .stret4
					DecodeResult delay_inst = decodeInstruction(address + 4);
					if (Boomerang::get()->traceDecoder)
						LOG << "*0x" << std::hex << address + 4 << std::dec << "\t\n";
					if (auto call = dynamic_cast<CallStatement *>(last)) {
						// Check the delay slot of this call. First case of interest is when the instruction is a restore,
						// e.g.
						// 142c8:  40 00 5b 91        call         exit
						// 142cc:  91 e8 3f ff        restore      %g0, -1, %o0
						if (decoder.isRestore(address + 4, pBF)) {
							// Give the address of the call; I think that this is actually important, if faintly annoying
							delay_inst.rtl->setAddress(address);
							BB_rtls->push_back(delay_inst.rtl);
							// The restore means it is effectively followed by a return (since the resore semantics chop
							// off one level of return address)
							call->setReturnAfterCall(true);
							sequentialDecode = false;
							case_CALL(address, inst, nop_inst, BB_rtls, proc, callList, true);
							break;
						}
						// Next class of interest is if it assigns to %o7 (could be a move, add, and possibly others). E.g.:
						// 14e4c:  82 10 00 0f        mov          %o7, %g1
						// 14e50:  7f ff ff 60        call         blah
						// 14e54:  9e 10 00 01        mov          %g1, %o7
						// Note there could be an unrelated instruction between the first move and the call
						// (move/x/call/move in UQBT terms).  In boomerang, we leave the semantics of the moves there
						// (to be likely removed by dataflow analysis) and merely insert a return BB after the call
						// Note that if an add, there may be an assignment to a temp register first. So look at last RT
						const auto &stmts = delay_inst.rtl->getList();
						if (!stmts.empty()) {
							if (auto as = dynamic_cast<Assign *>(stmts.back())) {
								if (as->getLeft()->isRegN(15)) {  // %o7 is r[15]
									// If it's an add, this is special. Example:
									//   call foo
									//   add %o7, K, %o7
									// is equivalent to call foo / ba .+K
									Exp *rhs = as->getRight();
									Location *o7 = Location::regOf(15);
									if (rhs->getOper() == opPlus
									 && (((Binary *)rhs)->getSubExp2()->isIntConst())
									 && (*((Binary *)rhs)->getSubExp1() == *o7)) {
										// Get the constant
										int K = ((Const *)((Binary *)rhs)->getSubExp2())->getInt();
										case_CALL(address, inst, delay_inst, BB_rtls, proc, callList, true);
										// We don't generate a goto; instead, we just decode from the new address
										// Note: the call to case_CALL has already incremented address by 8, so don't do again
										address += K;
										break;
									} else {
										// We assume this is some sort of move/x/call/move pattern. The overall effect is to
										// pop one return address, we we emit a return after this call
										call->setReturnAfterCall(true);
										sequentialDecode = false;
										case_CALL(address, inst, delay_inst, BB_rtls, proc, callList, true);
										break;
									}
								}
							}
						}
					}
					RTL *delay_rtl = delay_inst.rtl;

					switch (delay_inst.type) {
					case NOP:
					case NCT:
						{
							// Ordinary delayed instruction. Since NCT's can't affect unconditional jumps, we put the delay
							// instruction before the jump or call
							if (dynamic_cast<CallStatement *>(last)) {

								// This is a call followed by an NCT/NOP
								sequentialDecode = case_CALL(address, inst, delay_inst, BB_rtls, proc, callList);
							} else {
								// This is a non-call followed by an NCT/NOP
								case_SD(address, inst, delay_inst, BB_rtls, cfg, targetQueue);

								// There is no fall through branch.
								sequentialDecode = false;
							}
							if (spec && !inst.valid)
								return false;
						}
						break;

					case SKIP:
						case_unhandled_stub(address);
						address += 8;
						break;

					case SU:
						{
							// SD/SU.
							// This will be either BA or CALL followed by BA,A. Our interpretation is that it is as if the SD
							// (i.e. the BA or CALL) now takes the destination of the SU (i.e. the BA,A). For example:
							//     call 1000, ba,a 2000
							// is really like:
							//     call 2000.

							// Just so that we can check that our interpretation is correct the first time we hit this case...
							case_unhandled_stub(address);

							// Adjust the destination of the SD and emit it.
							auto delay_jump = static_cast<GotoStatement *>(delay_rtl->getList().back());
							int dest = 4 + address + delay_jump->getFixedDest();
							stmt_jump->setDest(dest);
							BB_rtls->push_back(inst.rtl);

							// Create the appropriate BB
							if (dynamic_cast<CallStatement *>(last)) {
								handleCall(proc, dest, cfg->newBB(BB_rtls, CALL, 1), cfg, address, 8);

								// Set the address of the lexical successor of the call that is to be decoded next. Set RTLs
								// to nullptr so that a new list of RTLs will be created for the next BB.
								BB_rtls = nullptr;
								address = address + 8;

								// Add this call site to the set of call sites which need to be analysed later.
								callList.push_back((CallStatement *)inst.rtl->getList().back());
							} else {
								auto pBB = cfg->newBB(BB_rtls, ONEWAY, 1);
								handleBranch(dest, pBB, cfg, targetQueue);

								// There is no fall through branch.
								sequentialDecode = false;
							}
						}
						break;

					default:
						case_unhandled_stub(address);
						address += 8;  // Skip the pair
						break;
					}
				}
				break;

			case DD:
				{
					DecodeResult delay_inst;
					if (inst.numBytes == 4) {
						// Ordinary instruction. Look at the delay slot
						delay_inst = decodeInstruction(address + 4);
					} else {
						// Must be a prologue or epilogue or something.
						delay_inst = nop_inst;
						// Should be no need to adjust the coverage; the number of bytes should take care of it
					}

					RTL *delay_rtl = delay_inst.rtl;

					// Display RTL representation if asked
					if (Boomerang::get()->printRtl && delay_rtl)
						LOG << *delay_rtl;

					switch (delay_inst.type) {
					case NOP:
					case NCT:
						{
							sequentialDecode = case_DD(address, inst, delay_inst, BB_rtls, targetQueue, proc, callList);
						}
						break;
					default:
						case_unhandled_stub(address);
						break;
					}
				}
				break;

			case SCD:
				{
					// Always execute the delay instr, and branch if condition is met.
					// Normally, the delayed instruction moves in front of the branch. But if it affects the condition
					// codes, we may have to duplicate it as an orphan in the true leg of the branch, and fall through to
					// the delay instruction in the "false" leg.
					// Instead of moving the delay instruction to an orphan BB, we may have a duplicate of the delay
					// instruction just before the target; if so, we can branch to that and not need the orphan.  We do
					// just a binary comparison; that may fail to make this optimisation if the instr has relative fields.

					DecodeResult delay_inst = decodeInstruction(address + 4);
					RTL *delay_rtl = delay_inst.rtl;

					// Display low level RTL representation if asked
					if (Boomerang::get()->printRtl && delay_rtl)
						LOG << *delay_rtl;

					switch (delay_inst.type) {
					case NOP:
					case NCT:
						{
							sequentialDecode = case_SCD(address, inst, delay_inst, BB_rtls, cfg, targetQueue);
						}
						break;
					default:
						if (dynamic_cast<CallStatement *>(delay_inst.rtl->getList().back())) {
							// Assume it's the move/call/move pattern
							sequentialDecode = case_SCD(address, inst, delay_inst, BB_rtls, cfg, targetQueue);
							break;
						}
						case_unhandled_stub(address);
						break;
					}
				}
				break;

			case SCDAN:
				{
					// Execute the delay instruction if the branch is taken; skip (anull) the delay instruction if branch
					// not taken.
					DecodeResult delay_inst = decodeInstruction(address + 4);
					RTL *delay_rtl = delay_inst.rtl;

					// Display RTL representation if asked
					if (Boomerang::get()->printRtl && delay_rtl)
						LOG << *delay_rtl;

					switch (delay_inst.type) {
					case NOP:
						{
							// This is an ordinary two-way branch.  Add the branch to the list of RTLs for this BB
							BB_rtls->push_back(rtl);
							// Create the BB and add it to the CFG
							auto pBB = cfg->newBB(BB_rtls, TWOWAY, 2);
							if (!pBB) {
								sequentialDecode = false;
								break;
							}
							// Visit the destination of the branch; add "true" leg
							ADDRESS uDest = stmt_jump->getFixedDest();
							handleBranch(uDest, pBB, cfg, targetQueue);
							// Add the "false" leg: point past the delay inst
							cfg->addOutEdge(pBB, address + 8);
							address += 8;       // Skip branch and delay
							BB_rtls = nullptr;  // Start new BB
						}
						break;

					case NCT:
						{
							sequentialDecode = case_SCDAN(address, inst, delay_inst, BB_rtls, cfg, targetQueue);
						}
						break;

					default:
						case_unhandled_stub(address);
						address = address + 8;
						break;
					}
				}
				break;

			default:  // Others are non sparc cases
				break;
			}

			// If sequentially decoding, check if the next address happens to be the start of an existing BB. If so,
			// finish off the current BB (if any RTLs) as a fallthrough, and no need to decode again (unless it's an
			// incomplete BB, then we do decode it).  In fact, mustn't decode twice, because it will muck up the
			// coverage, but also will cause subtle problems like add a call to the list of calls to be processed, then
			// delete the call RTL (e.g. Pentium 134.perl benchmark)
			if (sequentialDecode && cfg->existsBB(address)) {
				// Create the fallthrough BB, if there are any RTLs at all
				if (BB_rtls) {
					auto pBB = cfg->newBB(BB_rtls, FALL, 1);
					// Add an out edge to this address
					if (pBB) {
						cfg->addOutEdge(pBB, address);
						BB_rtls = nullptr;  // Need new list of RTLs
					}
				}
				// Pick a new address to decode from, if the BB is complete
				if (!cfg->isIncomplete(address))
					sequentialDecode = false;
			}

		} // while (sequentialDecode)

		// Add this range to the coverage
		//proc->addRange(start, address);

		// Must set sequentialDecode back to true
		sequentialDecode = true;
	} // End huge while loop


	// Add the callees to the set of CallStatements to proces for parameter recovery, and also to the Prog object
	for (const auto &call : callList) {
		ADDRESS dest = call->getFixedDest();
		// Don't speculatively decode procs that are outside of the main text section, apart from dynamically linked
		// ones (in the .plt)
		if (pBF->isDynamicLinkedProc(dest) || !spec || (dest < pBF->getLimitTextHigh())) {
			// Don't visit the destination of a register call
			//if (dest != NO_ADDRESS) newProc(proc->getProg(), dest);
			if (dest != NO_ADDRESS) proc->getProg()->setNewProc(dest);
		}
	}

	// MVE: Not 100% sure this is the right place for this
	proc->setEntryBB();

	return true;
}

#if 0 // Cruft?
/**
 * Emit a null RTL with the given address.
 *
 * \param rtls  List of RTLs to append this instruction to.
 * \param addr  Native address of this instruction.
 */
void
SparcFrontEnd::emitNop(std::list<RTL *> &rtls, ADDRESS addr)
{
	// Emit a null RTL with the given address. Required to cope with
	// SKIP instructions. Yes, they really happen, e.g. /usr/bin/vi 2.5
	rtls.push_back(new RTL(addr));
}
#endif

/**
 * Emit the RTL for a call $+8 instruction, which is merely %o7 = %pc.
 *
 * \note Assumes that the delay slot RTL has already been pushed; we must push
 * the semantics BEFORE that RTL, since the delay slot instruction may use
 * %o7.  Example:
 *
 *     CALL $+8            ! This code is common in startup code
 *     ADD  %o7, 20, %o0
 *
 * \param rtls  List of RTLs to append to.
 * \param addr  Native address for the RTL.
 */
void
SparcFrontEnd::emitCopyPC(std::list<RTL *> &rtls, ADDRESS addr)
{
	// Emit %o7 = %pc
	auto a = new Assign(Location::regOf(15),  // %o7 == r[15]
	                    new Terminal(opPC));
	// Add the Exp to an RTL
	auto rtl = new RTL(addr, a);
	// Add the RTL to the list of RTLs, but to the second last position
	rtls.insert(--rtls.end(), rtl);
}

/**
 * \brief Append one assignment to a list of RTLs.
 */
void
SparcFrontEnd::appendAssignment(std::list<RTL *> &rtls, ADDRESS addr, Type *type, Exp *lhs, Exp *rhs)
{
	auto a = new Assign(type, lhs, rhs);
	// Create an RTL with this one Statement
	auto rtl = new RTL(addr, a);
	// Append this RTL to the list of RTLs for this BB
	rtls.push_back(rtl);
}

/**
 * Small helper function to build an expression with
 *
 *     *128* m[m[r[14]+64]] = m[r[8]] OP m[r[9]]
 */
void
SparcFrontEnd::quadOperation(std::list<RTL *> &rtls, ADDRESS addr, OPER op)
{
	auto lhs = Location::memOf(Location::memOf(new Binary(opPlus,
	                                                      Location::regOf(14),
	                                                      new Const(64))));
	auto rhs = new Binary(op,
	                      Location::memOf(Location::regOf(8)),
	                      Location::memOf(Location::regOf(9)));
	appendAssignment(rtls, addr, new FloatType(128), lhs, rhs);
}

/**
 * \brief Checks for sparc specific helper functions like .urem, which have
 * specific semantics.
 *
 * Determine if this is a helper function, e.g. .mul.
 * If so, append the appropriate RTLs to rtls, and return true.
 *
 * \note This needs to be handled in a resourcable way.
 *
 * \param rtls  List of RTL* for current BB.
 * \param addr  Address of current instruction (native addr).
 * \param dest  Destination of the call (native address).
 *
 * \returns true if a helper function was found and handled; false otherwise.
 */
bool
SparcFrontEnd::helperFunc(std::list<RTL *> &rtls, ADDRESS addr, ADDRESS dest)
{
	if (!pBF->isDynamicLinkedProc(dest)) return false;
	const char *p = pBF->getSymbolByAddress(dest);
	if (!p) {
		std::cerr << "Error: Can't find symbol for PLT address " << std::hex << dest << std::dec << std::endl;
		return false;
	}
	auto name = std::string(p);
	//if (!progOptions.fastInstr)
	if (0)  // SETTINGS!
		return helperFuncLong(rtls, addr, name);
	Exp *rhs;
	if (name == ".umul") {
		// %o0 * %o1
		rhs = new Binary(opMult,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".mul") {
		// %o0 *! %o1
		rhs = new Binary(opMults,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".udiv") {
		// %o0 / %o1
		rhs = new Binary(opDiv,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".div") {
		// %o0 /! %o1
		rhs = new Binary(opDivs,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".urem") {
		// %o0 % %o1
		rhs = new Binary(opMod,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".rem") {
		// %o0 %! %o1
		rhs = new Binary(opMods,
		                 Location::regOf(8),
		                 Location::regOf(9));
#if 0
	} else if (name.substr(0, 6) == ".stret") {
		// No operation. Just use %o0
		rhs->push(idRegOf); rhs->push(idIntConst); rhs->push(8);
#endif
	} else if (name == "_Q_mul") {
		// Pointers to args are in %o0 and %o1; ptr to result at [%sp+64]
		// So semantics is m[m[r[14]] = m[r[8]] *f m[r[9]]
		quadOperation(rtls, addr, opFMult);
		return true;
	} else if (name == "_Q_div") {
		quadOperation(rtls, addr, opFDiv);
		return true;
	} else if (name == "_Q_add") {
		quadOperation(rtls, addr, opFPlus);
		return true;
	} else if (name == "_Q_sub") {
		quadOperation(rtls, addr, opFMinus);
		return true;
	} else {
		// Not a (known) helper function
		return false;
	}
	// Need to make an RTAssgn with %o0 = rhs
	auto lhs = Location::regOf(8);
	auto a = new Assign(lhs, rhs);
	// Create an RTL with this one Exp
	auto rtl = new RTL(addr, a);
	// Append this RTL to the list of RTLs for this BB
	rtls.push_back(rtl);
	return true;
}

/**
 * Another small helper function to generate either (for V9):
 *
 *     *64* tmp[tmpl] = sgnex(32, 64, r8) op sgnex(32, 64, r9)
 *     *32* r8 = truncs(64, 32, tmp[tmpl])
 *     *32* r9 = r[tmpl]@[32:63]
 *
 * or for v8:
 *
 *     *32* r[tmp] = r8 op r9
 *     *32* r8 = r[tmp]
 *     *32* r9 = %Y
 */
void
SparcFrontEnd::gen32op32gives64(std::list<RTL *> &rtls, ADDRESS addr, OPER op)
{
	auto ls = std::list<Statement *>();
#ifdef V9_ONLY
	// tmp[tmpl] = sgnex(32, 64, r8) op sgnex(32, 64, r9)
	ls.push_back(new Assign(64,
	                        Location::tempOf(new Const("tmpl")),
	                        new Binary(op,  // opMult or opMults
	                                   new Ternary(opSgnEx, Const(32), Const(64), Location::regOf(8)),
	                                   new Ternary(opSgnEx, Const(32), Const(64), Location::regOf(9)))));
	// r8 = truncs(64, 32, tmp[tmpl]);
	ls.push_back(new Assign(32,
	                        Location::regOf(8),
	                        new Ternary(opTruncs, new Const(64), new Const(32), Location::tempOf(new Const("tmpl")))));
	// r9 = r[tmpl]@[32:63];
	ls.push_back(new Assign(32,
	                        Location::regOf(9),
	                        new Ternary(opAt, Location::tempOf(new Const("tmpl")), new Const(32), new Const(63))));
#else
	// BTL: The .umul and .mul support routines are used in V7 code. We implsment these using the V8 UMUL and SMUL
	// instructions.
	// BTL: In SPARC V8, UMUL and SMUL perform 32x32 -> 64 bit multiplies.
	//      The 32 high-order bits are written to %Y and the 32 low-order bits are written to r[rd]. This is also true
	//      on V9 although the high-order bits are also written into the 32 high-order bits of the 64 bit r[rd].

	// r[tmp] = r8 op r9
	ls.push_back(new Assign(Location::tempOf(new Const("tmp")),
	                        new Binary(op,  // opMult or opMults
	                                   Location::regOf(8),
	                                   Location::regOf(9))));
	// r8 = r[tmp];  /* low-order bits */
	ls.push_back(new Assign(Location::regOf(8),
	                        Location::tempOf(new Const("tmp"))));
	// r9 = %Y;      /* high-order bits */
	ls.push_back(new Assign(Location::regOf(8),
	                        new Unary(opMachFtr, new Const("%Y"))));
#endif
	auto rtl = new RTL(addr);
	rtl->splice(ls);
	rtls.push_back(rtl);
}

/**
 * This is the long version of helperFunc (i.e. -f not used).  This does the
 * complete 64 bit semantics.
 */
bool
SparcFrontEnd::helperFuncLong(std::list<RTL *> &rtls, ADDRESS addr, const std::string &name)
{
	Exp *rhs;
	if (name == ".umul") {
		gen32op32gives64(rtls, addr, opMult);
		return true;
	} else if (name == ".mul") {
		gen32op32gives64(rtls, addr, opMults);
		return true;
	} else if (name == ".udiv") {
		// %o0 / %o1
		rhs = new Binary(opDiv,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".div") {
		// %o0 /! %o1
		rhs = new Binary(opDivs,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".urem") {
		// %o0 % %o1
		rhs = new Binary(opMod,
		                 Location::regOf(8),
		                 Location::regOf(9));
	} else if (name == ".rem") {
		// %o0 %! %o1
		rhs = new Binary(opMods,
		                 Location::regOf(8),
		                 Location::regOf(9));
#if 0
	} else if (name.substr(0, 6) == ".stret") {
		// No operation. Just use %o0
		rhs->push(idRegOf); rhs->push(idIntConst); rhs->push(8);
#endif
	} else if (name == "_Q_mul") {
		// Pointers to args are in %o0 and %o1; ptr to result at [%sp+64]
		// So semantics is m[m[r[14]] = m[r[8]] *f m[r[9]]
		quadOperation(rtls, addr, opFMult);
		return true;
	} else if (name == "_Q_div") {
		quadOperation(rtls, addr, opFDiv);
		return true;
	} else if (name == "_Q_add") {
		quadOperation(rtls, addr, opFPlus);
		return true;
	} else if (name == "_Q_sub") {
		quadOperation(rtls, addr, opFMinus);
		return true;
	} else {
		// Not a (known) helper function
		return false;
	}
	// Need to make an RTAssgn with %o0 = rhs
	auto lhs = Location::regOf(8);
	appendAssignment(rtls, addr, new IntegerType(32), lhs, rhs);
	return true;
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
	return new SparcFrontEnd(bf, prog);
}
extern "C" void
destruct(FrontEnd *fe)
{
	delete (SparcFrontEnd *)fe;
}
#endif
