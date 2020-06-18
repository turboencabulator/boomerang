/*
 * Copyright (C) 2001, The University of Queensland
 * Copyright (C) 2001, Sun Microsystems, Inc
 *
 * See the file "LICENSE.TERMS" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 *
 */

/*==============================================================================
 * FILE:       fronthppa.cc
 * OVERVIEW:   This file contains routines to manage the decoding of HP pc-risc
 *             instructions and the instantiation to RTLs. These functions
 *             replace Frontend.cc for decoding hppa instructions.
 *============================================================================*/

/*==============================================================================
 * Dependencies.
 *============================================================================*/

#include "global.h"
#include "ss.h"
#include "rtl.h"
#include "cfg.h"
#include "proc.h"
#include "prog.h"
#include "options.h"
#include "csr.h"
#include "frontend.h"
#include "decoder.h"
#include "BinaryFile.h"

/*==============================================================================
 * Globals and enumerated types used for decoding.
 *============================================================================*/

// These are indexes to the most common control/status registers.
int idNPC = -1;
int idCWP = -1;
int idTmp = -1;

// This struct represents a single nop instruction. Used as a substitute
// delay slot instruction
static DecodeResult nop_inst;


/*==============================================================================
 * Forward declarations.
 *============================================================================*/
void emitNop(HRTLList &, ADDRESS);
void emitCopyPC(HRTLList &, ADDRESS);
void initCti();             // Imp in ctisparc.cc
void setReturnLocations(CalleeEpilogue *, int);
bool helperFunc(HRTLList &, ADDRESS, ADDRESS);

/*==============================================================================
 * FUNCTION:        initFront
 * OVERVIEW:        Initialise any globals used by the front end.
 * PARAMETERS:      <none>
 * RETURNS:         <nothing>
 *============================================================================*/
// Initialise the front end
void
initFront()
{
	// We need the 2nd parameter to findItem because we may or may not have
	// parsed any instructions that involve %npc etc (depending on whether
	// we want the low level RTLs or not). This prevents needless error messages
	idNPC = theSemTable.findItem("%npc", false);
	idTmp = theSemTable.findItem("tmp", false);

	// This struct represents a single nop instruction. Used as a substitute
	// delay slot instruction
	nop_inst.numBytes = 0;          // So won't disturb coverage
	nop_inst.type = NOP;
	nop_inst.valid = true;
	nop_inst.rtl = new RTL();
}

/*==============================================================================
 * FUNCTION:         warnDCTcouple
 * OVERVIEW:         Emit a warning when encountering a DCTI couple.
 * PARAMETERS:       at - the address of the couple
 *                   dest - the address of the first DCTI in the couple
 * RETURNS:          <nothing>
 *============================================================================*/
void
warnDCTcouple(ADDRESS at, ADDRESS dest)
{
	ostrstream ost;
	ost << "DCTI couple at " << hex;
	ost << at << " points to delayed branch at " << dest << "...\n";
	ost << "Translation will likely be incorrect";
	error(str(ost));
}

/*==============================================================================
 * FUNCTION:         interferes
 * OVERVIEW:         Return true if the delay slot instruction interferes with
 *                      a register used by the main instruction
 * PARAMETERS:       delayRtl: pointer to the HRTL for the delay slot instr
 *                   mainRtl: pointer to the HRTL for the main instruction
 * RETURNS:          true if interference detected
 *============================================================================*/
bool
interferes(HRTL *delayRtl, HRTL *mainRtl)
{
	if (!delayRtl) return false;
	int n = delayRtl->getNumRT();
	int m = mainRtl->getNumRT();
	for (int i = 0; i < n; i++) {
		RTAssgn *rta = (RTAssgn *)delayRtl->elementAt(i);
		if (rta->getKind() != RTASSGN) continue;
		SemStr *lhs = rta->getLHS();
		// Assume that only registers will interfere
		if (lhs->getFirstIdx() != idRegOf) continue;
		if (lhs->getSecondIdx() != idIntConst) continue;
		for (int j = 0; j < m; j++) {
			rta = (RTAssgn *)delayRtl->elementAt(j);
			if (rta->getKind() != RTASSGN) continue;
			SemStr *rhs = rta->getRHS();
			SemStr result;
			if (rhs->search(*lhs, result))
				return true;
		}
	}
	return false;
}

/*==============================================================================
 * FUNCTION:        optimise_DelayCopy
 * OVERVIEW:        Determines if a delay instruction is exactly the same as the
 *                  instruction immediately preceding the destination of a CTI;
 *                  i.e. has been copied from the real destination to the delay
 *                  slot as an optimisation
 * PARAMETERS:      src - the logical source address of a CTI
 *                  dest - the logical destination address of the CTI
 *                  delta - used to convert logical to real addresses
 *                  upper - first address past the end of the main text
 *                    section
 * SIDE EFFECT:     Optionally displays an error message if the target of the
 *                    branch is the delay slot of another delayed CTI
 * RETURNS:         can optimise away the delay instruction
 *============================================================================*/
bool
optimise_DelayCopy(ADDRESS src, ADDRESS dest, int delta, ADDRESS upper)
{
	// Check that the destination is within the main test section; may not be
	// when we speculatively decode junk
	if ((dest - 4) > upper)
		return false;
	unsigned delay_inst = *((unsigned *)(src + 4 + delta));
	unsigned inst_before_dest = *((unsigned *)(dest - 4 + delta));
	return (delay_inst == inst_before_dest);
}

/*==============================================================================
 * FUNCTION:        handleBranch
 * OVERVIEW:        Adds the destination of a branch to the queue of address
 *                  that must be decoded (if this destination has not already
 *                  been visited).
 * PARAMETERS:      newBB - the new basic block delimited by the branch
 *                    instruction. May be null if this block has been built
 *                    before.
 *                  dest - the destination being branched to
 *                  upper - the last address in the current procedure
 *                  cfg - the CFG of the current procedure
 * RETURNS:         <nothing>, but newBB may be changed if the destination of
 *                  the branch is in the middle of an existing BB. It will then
 *                  be changed to point to a new BB beginning with the dest
 *============================================================================*/
void
handleBranch(ADDRESS dest, ADDRESS upper, BasicBlock *&newBB, Cfg *cfg)
{
	if (dest < upper) {
		cfg->visit(dest, newBB);
		cfg->addOutEdge(newBB, dest);
	} else {
		ostrstream ost;
		ost << "branch to " << hex << dest << " goes beyond section.";
		error(str(ost));
	}
}

/*==============================================================================
 * FUNCTION:          handleCall
 * OVERVIEW:          Records the fact that there is a procedure at a given
 *                    address. Also adds the out edge to the
 *                    lexical successor of the call site (taking into
 *                    consideration the delay slot and possible UNIMP
 *                    instruction).
 * PARAMETERS:        dest - the address of the callee
 *                    callBB - the basic block delimited by the call
 *                    cfg - CFG of the enclosing procedure
 *                    addr - the address of the call instruction
 *                    offset - the offset from the call instruction to which an
 *                      outedge must be added. A value of 0 means no edge is to
 *                      be added.
 * RETURNS:           <nothing>
 *============================================================================*/
void
handleCall(ADDRESS dest, BasicBlock *callBB, Cfg *cfg, ADDRESS addr, int offset = 0)
{
	// If the destination address is the same as this very instruction,
	// we have a call with offset == 0. Don't treat this as the start
	// of a real procedure.
	if ((dest != addr) && prog.findProc(dest) == 0) {
		// We don't want to call prog.visitProc just yet, in case this is
		// a speculative decode that failed. Instead, we use the set of
		// HLCalls (not in this procedure) that is needed by CSR
		if (progOptions.trace)
			cout << "p" << hex << dest << "\t";
	}

	// Add the out edge if required
	if (offset != 0)
		cfg->addOutEdge(callBB, addr + offset);
}

/*==============================================================================
 * FUNCTION:         case_unhandled_stub
 * OVERVIEW:         This is the stub for cases of DCTI couples that we haven't
 *                   written analysis code for yet. It simply displays an
 *                   informative warning and returns.
 * PARAMETERS:       addr - the address of the first CTI in the couple
 * RETURNS:          <nothing>
 *============================================================================*/
void
case_unhandled_stub(ADDRESS addr)
{
	ostrstream ost;
	ost << "DCTI couple at " << hex << addr << endl;
	error(str(ost));
}

/*==============================================================================
 * FUNCTION:         case_CALL_NCT
 * OVERVIEW:         Handles a call instruction followed by an NCT or NOP
 *                   instruction.
 * PARAMETERS:       addr - the native address of the call instruction
 *                   inst - the info summaries when decoding the call
 *                     instruction
 *                   delay_inst - the info summaries when decoding the delay
 *                     instruction
 *                   BB_rtls - the list of RTLs currently built for the BB under
 *                     construction
 *                   proc - the enclosing procedure
 *                   callSet - a set of pointers to HLCalls for procs yet to
 *                     be processed
 *                   isPattern - true if the call is an idiomatic pattern (e.g.
 *                      a move_call_move pattern)
 * SIDE EFFECTS:     addr may change; BB_rtls may be appended to or set to null
 * RETURNS:          true if next instruction is to be fetched sequentially from
 *                      this one
 *============================================================================*/
bool
case_CALL_NCT(ADDRESS &addr, const DecodeResult &inst,
              const DecodeResult &delay_inst, list<HRTL *> *&BB_rtls,
              UserProc *proc, std::list<CallStatement *> &callList, bool isPattern = false)
{
	auto call_rtl = static_cast<HLCall *>(inst.rtl);

	Cfg *cfg = proc->getCFG();

	// Assume that if we find a call in the delay slot, it's actually a pattern
	// such as move/call/move
	bool delayPattern = delay_inst.rtl->getKind() == CALL_HRTL;

	// Emit the delay instruction, unless a the delay instruction is a nop,
	// or we have a pattern, or are followed by a restore
	if ((delay_inst.type != NOP)
	 && !delayPattern
	 && !call_rtl->isReturnAfterCall()) {
		delay_inst.rtl->setAddress(addr);
		BB_rtls->push_back(delay_inst.rtl);
		if (progOptions.rtl)
			LOG << *delay_inst.rtl;
	}

	{
		auto dest = call_rtl->getFixedDest();
		// First check for helper functions
		if (helperFunc(*BB_rtls, addr, dest)) {
			addr += 8;           // Skip call, delay slot
			return true;
		}

		// Emit the call
		BB_rtls->push_back(call_rtl);

		// End the current basic block
		auto callBB = cfg->newBB(BB_rtls, CALL, 1);

		// Add this call site to the set of call sites which
		// need to be analysed later.
		// This set will be used later to call prog.visitProc (so the proc
		// will get decoded)
		callList.push_back((HLCall *)inst.rtl);

		if (call_rtl->isReturnAfterCall()) {
			// Handle the call but don't add any outedges from it just yet.
			handleCall(call_rtl->getFixedDest(), callBB, cfg, addr);
			appendSyntheticReturn(callBB, proc);

			// Note that we get here for certain types of patterns as well as
			// for call/return pairs. This could all go away if we could
			// specify that some sparc Logues are caller-prologue and also
			// callee-epilogues!
			// This is a hack until we figure out how to match these
			// patterns using a .pat file. We have to set the epilogue
			// for the enclosing procedure (all proc's must have an
			// epilogue).
			proc->setEpilogue(new CalleeEpilogue("__dummy", list<string>()));
			// Set the return location; this is now always %r28
			setReturnLocations(proc->getEpilogue(), 28);

			addr += inst.numBytes;       // For coverage
			// This is a CTI block that doesn't fall through and so must
			// stop sequentially decoding
			return false;
		} else {
			// Else no restore after this call.
			// An outedge may be added to the lexical
			// successor of the call which will be 8 bytes
			// ahead or in the case where the callee returns
			// a struct, 12 bytes head
			// If forceOutEdge is set, set offset to 0 and no out-edge will be
			// added yet
			int offset = inst.forceOutEdge ? 0 : 8;

			bool ret = true;
			// Check for "never return" functions
			const char *name = prog.pBF->getSymbolByAddress(dest);
			if (name && noReturnCallDest(name)) {
				// Don't keep decoding after this call
				ret = false;
				// Also don't add an out-edge; setting offset to 0 will do this
				offset = 0;
				// But we have already set the number of out-edges to 1
				callBB->updateType(CALL, 0);
			}

			// Handle the call (register the destination as a proc)
			// and possibly set the outedge.
			handleCall(dest, callBB, cfg, addr, offset);

			if (inst.forceOutEdge) {
				// There is no need to force a goto to the new out-edge, since
				// we will continue decoding from there. If other edges exist
				// to the outedge, they will generate the required label
				cfg->addOutEdge(callBB, inst.forceOutEdge);
				addr = inst.forceOutEdge;
			} else {
				// Continue decoding from the lexical successor
				addr += offset;
			}
			BB_rtls = nullptr;

			return ret;
		}
	}
}

/*==============================================================================
 * FUNCTION:         case_SD_NCT
 * OVERVIEW:         Handles a non-call, static delayed (SD) instruction
 *                   followed by an NCT or NOP instruction.
 * PARAMETERS:       addr - the native address of the SD
 *                   delta - the offset of the above address from the logical
 *                     address at which the procedure starts (i.e. the one
 *                     given by dis)
 *                   inst - the info summaries when decoding the SD
 *                     instruction
 *                   delay_inst - the info summaries when decoding the delay
 *                     instruction
 *                   BB_rtls - the list of RTLs currently built for the BB under
 *                     construction
 *                   cfg - the CFG of the enclosing procedure
 * SIDE EFFECTS:     addr may change; BB_rtls may be appended to or set to null
 * RETURNS:          <nothing>
 *============================================================================*/
void
case_SD_NCT(ADDRESS &addr, int delta, ADDRESS upper,
            const DecodeResult &inst, const DecodeResult &delay_inst, list<HRTL *> *&BB_rtls,
            Cfg *cfg)
{
	auto SD_rtl = static_cast<HLJump *>(inst.rtl);

	// Try the "delay instruction has been copied" optimisation, emitting the
	// delay instruction now if the optimisation won't apply
	if (delay_inst.type != NOP) {
		if (optimise_DelayCopy(addr, SD_rtl->getFixedDest(), delta, upper)) {
			SD_rtl->adjustFixedDest(-4);
		} else {
			// Move the delay instruction before the SD. Must update the address
			// in case there is a branch to the SD
			delay_inst.rtl->setAddress(addr);
			BB_rtls->push_back(delay_inst.rtl);
			// Display RTL representation if asked
			if (progOptions.rtl)
				LOG << *delay_inst.rtl;
		}
	}

	// Update the address (for coverage)
	addr += 8;

	// Add the SD
	BB_rtls->push_back(SD_rtl);

	// Add the one-way branch BB
	auto bb = cfg->newBB(BB_rtls, ONEWAY, 1);

	// Visit the destination, and add the out-edge
	handleBranch(SD_rtl->getFixedDest(), upper, bb, cfg);
	BB_rtls = nullptr;
}


/*==============================================================================
 * FUNCTION:         case_DD_NCT
 * OVERVIEW:         Handles all dynamic delayed jumps (jmpl, also dynamic
 *                    calls) followed by an NCT or NOP instruction.
 * NOTE:             Also handles DU, if delay_inst is assigned nop_inst
 * PARAMETERS:       addr - the native address of the DD
 *                   delta - the offset of the above address from the logical
 *                     address at which the procedure starts (i.e. the one
 *                     given by dis)
 *                   inst - the info summaries when decoding the SD
 *                     instruction
 *                   delay_inst - the info summaries when decoding the delay
 *                     instruction
 *                   BB_rtls - the list of RTLs currently built for the BB under
 *                     construction
 *                   cfg - the CFG of the enclosing procedure
 *                   proc: pointer to the current Proc object
 *                   callSet - a set of pointers to HLCalls for procs yet to
 *                     be processed
 *                   size: size of this instruction (8 if delay slot)
 * SIDE EFFECTS:     addr may change; BB_rtls may be appended to or set to null
 * RETURNS:          true if next instruction is to be fetched sequentially from
 *                      this one
 *============================================================================*/
bool
case_DD_NCT(ADDRESS &addr, int delta, const DecodeResult &inst,
            const DecodeResult &delay_inst, list<HRTL *> *&BB_rtls, Cfg *cfg,
            UserProc *proc, std::list<CallStatement *> &callList, int size)
{
	// Assume that if we find a call in the delay slot, it's actually a pattern
	// such as move/call/move
	bool delayPattern = delay_inst.rtl->getKind() == CALL_HRTL;

	if ((delay_inst.type != NOP) && !delayPattern) {
		// Emit the delayed instruction, unless a pattern
		delay_inst.rtl->setAddress(addr);
		BB_rtls->push_back(delay_inst.rtl);
	}

	// Set addr past this instruction and delay slot (if any).
	// This is so that we cover the jmp/call and delay slot instruction, in
	// case we return false
	addr += size;

	// Emit the DD and end the current BB
	BB_rtls->push_back(inst.rtl);
	BasicBlock *newBB;
	bool bRet = true;
	switch (inst.rtl->getKind()) {
	case CALL_HRTL:
		// Will be a computed call
		newBB = cfg->newBB(BB_rtls, COMPCALL, 1);
		break;
	case RET_HRTL:
		newBB = cfg->newBB(BB_rtls, RET, 0);
		bRet = false;
		break;
	case NWAYJUMP_HRTL:
		newBB = cfg->newBB(BB_rtls, COMPJUMP, 0);
		bRet = false;
		break;
	default:
		return false;
	}

	// Do extra processing for for special types of DD
	if (inst.rtl->getKind() == CALL_HRTL) {

		// Attempt to add a return BB if the delay
		// instruction is a RESTORE
		auto rtl_call = static_cast<HLCall *>(inst.rtl);
#if 0  // Sparc specific code, but we may need something similar
		auto returnBB = optimise_CallReturn(rtl_call, delay_inst.rtl, cfg);
		if (returnBB) {
			cfg->addOutEdge(newBB, returnBB);

			// We have to set the epilogue
			// for the enclosing procedure (all proc's must have an
			// epilogue) and remove the RESTORE in the delay slot that
			// has just been pushed to the list of RTLs
			proc->setEpilogue(new CalleeEpilogue("__dummy", list<string>()));
			// Set the return location; this is now always %r28
			setReturnLocations(proc->getEpilogue(), 28);
			newBB->getHRTLs()->remove(delay_inst.rtl);

			// Add this call to the list of calls to analyse. We won't be able
			// to analyse it's callee(s), of course.
			callList.push_back(rtl_call);

			return false;
		} else {
#else
		{
#endif
			// Instead, add the standard out edge to original address+8 (now
			// just addr)
			cfg->addOutEdge(newBB, addr);
		}
		// Add this call to the list of calls to analyse. We won't be able
		// to analyse its callee(s), of course.
		callList.push_back(rtl_call);
	} else if (inst.rtl->getKind() == NWAYJUMP_HRTL) {

		// Attempt to process this jmpl as a switch statement.
		// NOTE: the isSwitch and processSwitch methods should
		// really be merged into one
		auto rtl_jump = static_cast<HLNwayJump *>(inst.rtl);
		if (isSwitch(newBB, rtl_jump->getDest(), proc, pBF)) {
			processSwitch(newBB, delta, cfg, pBF);
		} else {
			ostrstream os;
			os << "COMPUTED JUMP at " << hex << addr - 8;
			warning(str(os));
		}
	}

	// Set the address of the lexical successor of the call
	// that is to be decoded next and create a new list of
	// RTLs for the next basic block.
	// Except if we had a pattern in the delay slot; then don't skip
	// Remember that we have already bumped addr by 8
	if (delayPattern) addr -= 4;
	BB_rtls = nullptr;
	return bRet;
}

/*==============================================================================
 * FUNCTION:         case_SCD_NCT
 * OVERVIEW:         Handles all static conditional delayed non-anulled branches
 *                     followed by an NCT or NOP instruction.
 * PARAMETERS:       addr - the native address of the DD
 *                   delta - the offset of the above address from the logical
 *                     address at which the procedure starts (i.e. the one
 *                     given by dis)
                     upper - first address outside this code section
 *                   inst - the info summaries when decoding the SD
 *                     instruction
 *                   delay_inst - the info summaries when decoding the delay
 *                     instruction
 *                   BB_rtls - the list of RTLs currently built for the BB under
 *                     construction
 *                   cfg - the CFG of the enclosing procedure
 * SIDE EFFECTS:     addr may change; BB_rtls may be appended to or set to null
 * RETURNS:          true if next instruction is to be fetched sequentially from
 *                      this one
 *============================================================================*/
bool
case_SCD_NCT(ADDRESS &addr, int delta, ADDRESS upper,
             const DecodeResult &inst, const DecodeResult &delay_inst, list<HRTL *> *&BB_rtls,
             Cfg *cfg)
{
	auto rtl_jump = static_cast<HLJump *>(inst.rtl);
	auto dest = rtl_jump->getFixedDest();

#if 0
	// Assume that if we find a call in the delay slot, it's actually a pattern
	// such as move/call/move
	bool delayPattern = delay_inst.rtl->getKind() == CALL_HRTL;

	if (delayPattern) {
		// Just emit the branch, and decode the instruction immediately
		// following next. Assumes the first instruction of the pattern is
		// not used in the true leg
		BB_rtls->push_back(inst.rtl);
		auto bb = cfg->newBB(BB_rtls, TWOWAY, 2);
		handleBranch(dest, upper, bb, cfg);
		// Add the "false" leg
		cfg->addOutEdge(bb, addr + 4);
		addr += 4;           // Skip the SCD only
		// Start a new list of RTLs for the next BB
		BB_rtls = nullptr;
		ostrstream ost;
		ost << "instruction at " << hex << addr;
		ost << " not copied to true leg of preceeding branch";
		warning(str(ost));
		return true;
	}
#endif

	if (!interferes(delay_inst.rtl, inst.rtl)) {
		// SCD; no interference. Put delay inst first
		if (delay_inst.type != NOP) {
			// Emit delay instr
			BB_rtls->push_back(delay_inst.rtl);
			// This is in case we have an in-edge to the branch. If the BB
			// is split, we want the split to happen here, so this delay
			// instruction is active on this path
			delay_inst.rtl->setAddress(addr);
		}
		// Now emit the branch
		BB_rtls->push_back(inst.rtl);
		auto bb = cfg->newBB(BB_rtls, TWOWAY, 2);
		handleBranch(dest, upper, bb, cfg);
		// Add the "false" leg; skips the NCT
		cfg->addOutEdge(bb, addr + 8);
		// Skip the NCT/NOP instruction
		addr += 8;
	} else if (optimise_DelayCopy(addr, dest, delta, upper)) {
		// We can just branch to the instr before dest.
		// Adjust the destination of the branch
		rtl_jump->adjustFixedDest(-4);
		// Now emit the branch
		BB_rtls->push_back(inst.rtl);
		auto bb = cfg->newBB(BB_rtls, TWOWAY, 2);
		handleBranch(dest - 4, upper, bb, cfg);
		// Add the "false" leg: point to the delay inst
		cfg->addOutEdge(bb, addr + 4);
		addr += 4;           // Skip branch but not delay
	} else { // There is interference, and we can't use the copy delay slot trick
		// SCD, must copy delay instr to orphan
		// Copy the delay instruction to the dest of the branch, as an orphan
		// First add the branch.
		BB_rtls->push_back(inst.rtl);
		// Make a BB for the current list of RTLs
		// We want to do this first, else ordering can go silly
		auto bb = cfg->newBB(BB_rtls, TWOWAY, 2);
		// Visit the target of the branch
		cfg->visit(dest, bb);
		auto pOrphan = new HRTLList;
		pOrphan->push_back(delay_inst.rtl);
		// Change the address to 0, since this code has no source address
		// (else we may branch to here when we want to branch to the real
		// BB with this instruction).
		// Note that you can't use an address that is a fixed function of the
		// destination addr, because there can be several jumps to the same
		// destination that all require an orphan. The instruction in the
		// orphan will often but not necessarily be the same, so we can't use
		// the same orphan BB. newBB knows to consider BBs with address 0 as
		// being in the map, so several BBs can exist with address 0
		delay_inst.rtl->setAddress(0);
		// Add a branch from the orphan instruction to the dest of the branch
		// Again, we can't even give the jumps a special address like 1, since
		// then the BB would have this getLowAddr.
		pOrphan->push_back(new HLJump(0, dest));
		auto pOrBB = cfg->newBB(pOrphan, ONEWAY, 1);
		// Add an out edge from the orphan as well
		cfg->addOutEdge(pOrBB, dest);
		// Add an out edge from the current RTL to the orphan.
		cfg->addOutEdge(bb, pOrBB);
		// Add the "false" leg to the NCT
		cfg->addOutEdge(bb, addr + 4);
		// Don't skip the delay instruction, so it will
		// be decoded next.
		addr += 4;
	}

	// Start a new list of RTLs for the next BB
	BB_rtls = nullptr;
	return true;
}

/*==============================================================================
 * FUNCTION:         case_SCDAN_NCT
 * OVERVIEW:         Handles all static conditional delayed anulled branches
 *                     followed by an NCT (but not NOP) instruction.
 * PARAMETERS:       addr - the native address of the DD
 *                   delta - the offset of the above address from the logical
 *                     address at which the procedure starts (i.e. the one
 *                     given by dis)
                     upper - first address outside this code section
 *                   inst - the info summaries when decoding the SD
 *                     instruction
 *                   delay_inst - the info summaries when decoding the delay
 *                     instruction
 *                   BB_rtls - the list of RTLs currently built for the BB under
 *                     construction
 *                   cfg - the CFG of the enclosing procedure
 * SIDE EFFECTS:     addr may change; BB_rtls may be appended to or set to null
 * RETURNS:          true if next instruction is to be fetched sequentially from
 *                      this one
 *============================================================================*/
bool
case_SCDAN_NCT(ADDRESS &addr, int delta, ADDRESS upper,
               const DecodeResult &inst, const DecodeResult &delay_inst, list<HRTL *> *&BB_rtls,
               Cfg *cfg)
{
	// We may have to move the delay instruction to an orphan
	// BB, which then branches to the target of the jump.
	// Instead of moving the delay instruction to an orphan BB,
	// we may have a duplicate of the delay instruction just
	// before the target; if so, we can branch to that and not
	// need the orphan. We do just a binary comparison; that
	// may fail to make this optimisation if the instr has
	// relative fields.
	auto rtl_jump = static_cast<HLJump *>(inst.rtl);
	auto dest = rtl_jump->getFixedDest();
	BasicBlock *bb;
	if (optimise_DelayCopy(addr, dest, delta, upper)) {
		// Adjust the destination of the branch
		rtl_jump->adjustFixedDest(-4);
		// Now emit the branch
		BB_rtls->push_back(inst.rtl);
		bb = cfg->newBB(BB_rtls, TWOWAY, 2);
		handleBranch(dest - 4, upper, bb, cfg);
	} else { // SCDAN; must move delay instr to orphan. Assume it's not a NOP (though if it is, no harm done)
		// Move the delay instruction to the dest of the branch, as an orphan
		// First add the branch.
		BB_rtls->push_back(inst.rtl);
		// Make a BB for the current list of RTLs
		// We want to do this first, else ordering can go silly
		bb = cfg->newBB(BB_rtls, TWOWAY, 2);
		// Visit the target of the branch
		cfg->visit(dest, bb);
		auto pOrphan = new HRTLList;
		pOrphan->push_back(delay_inst.rtl);
		// Change the address to 0, since this code has no source address
		// (else we may branch to here when we want to branch to the real
		// BB with this instruction).
		delay_inst.rtl->setAddress(0);
		// Add a branch from the orphan instruction to the dest of the branch
		pOrphan->push_back(new HLJump(0, dest));
		auto pOrBB = cfg->newBB(pOrphan, ONEWAY, 1);
		// Add an out edge from the orphan as well.
		cfg->addOutEdge(pOrBB, dest);
		// Add an out edge from the current RTL to the orphan.
		cfg->addOutEdge(bb, pOrBB);
	}
	// Both cases (orphan or not)
	// Add the "false" leg: point past delay inst.
	cfg->addOutEdge(bb, addr + 8);
	addr += 8;              // Skip branch and delay
	BB_rtls = nullptr;      // Start new BB return true;
	return true;
}


/*==============================================================================
 * FUNCTION:         FrontEndSrc::processProc
 * OVERVIEW:         Builds the CFG for a procedure out of the RTLs constructed
 *                   during decoding. The semantics of delayed CTIs are
 *                   transformed into CTIs that aren't delayed.
 * NOTE:             This function overrides (and replaces) the function with
 *                     the same name in class FrontEnd. The required actions
 *                     are so different that the base class implementation
 *                     can't be re-used
 * PARAMETERS:       addr - the address at which the procedure starts
 *                   proc - the procedure object
 *                   spec - if true, this is a speculative decode
 * RETURNS:          True if a good decode
 *============================================================================*/
bool
FrontEndSrc::processProc(ADDRESS addr, UserProc *proc, bool spec)
{
	// Similarly, we have a set of HLCall pointers. These may be disregarded
	// if this is a speculative decode that fails (i.e. an illegal instruction
	// is found). If not, this set will be used to add to the set of calls to
	// be analysed in the cfg, and also to call prog.visitProc()
	std::list<CallStatement *> callList;

	// The control flow graph of the current procedure
	Cfg *cfg = proc->getCFG();

	// If this is a speculative decode, the second time we decode the same
	// address, we get no cfg. Else an error.
	if (spec && !cfg)
		return false;
	assert(cfg);

	// Initialise the queue of control flow targets that have yet to be decoded.
	cfg->enqueue(addr);

	// Get the next address from which to continue decoding and go from
	// there. Exit the loop if there are no more addresses or they all
	// correspond to locations that have been decoded.
	while ((addr = cfg->dequeue()) != NO_ADDRESS) {

		// The list of RTLs for the current basic block
		auto BB_rtls = (list<HRTL *> *)nullptr;

		// Indicates whether or not the next instruction to be decoded is the lexical successor of the current one.
		// Will be true for all NCTs and for CTIs with a fall through branch.
		// Keep decoding sequentially until a CTI without a fall through branch is decoded
		bool sequentialDecode = true;
		while (sequentialDecode) {

			if (progOptions.trace)
				cout << "*" << hex << addr << "\t" << flush;

			auto inst = decoder.decodeInstruction(addr, delta, proc);

			// If invalid and we are speculating, just exit
			if (spec && !inst.valid)
				return false;

			// If it's a cancelled instruction (e.g. call to __main), just
			// ignore it
			if (!inst.rtl) {
				addr += 4;           // Advance to next instr
				continue;
			}
			// Don't display the RTL here; do it after the switch statement
			// in case the delay slot instruction is moved before this one

			// Need to construct a new list of RTLs if a basic block has just
			// been finished but decoding is continuing from its lexical
			// successor
			if (!BB_rtls)
				BB_rtls = new list<HRTL *>();

			// Define aliases to the RTLs so that they can be treated as a high
			// level types where appropriate.
			HRTL *rtl = inst.rtl;
			auto rtl_jump = static_cast<HLJump *>(rtl);

			// Update the number of bytes (for coverage)
			rtl->updateNumBytes(inst.numBytes);

#define BRANCH_DS_ERROR 0   // If set, a branch to the delay slot of a delayed
                            // CTI instruction is flagged as an error
#if BRANCH_DS_ERROR
			if ((rtl->getKind() == JUMP_HRTL)
			 || (rtl->getKind() == CALL_HRTL)
			 || (rtl->getKind() == JCOND_HRTL)
			 || (rtl->getKind() == RET_HRTL)) {
				auto dest = rtl_jump->getFixedDest();
				if ((dest != NO_ADDRESS) && (dest < upper)) {
					unsigned inst_before_dest = *((unsigned *)(dest - 4 + delta));

					// FIXME! This is sarc specific
					unsigned bits31_30 = inst_before_dest >> 30;
					unsigned bits23_22 = (inst_before_dest >> 22) & 3;
					unsigned bits24_19 = (inst_before_dest >> 19) & 0x3f;
					unsigned bits29_25 = (inst_before_dest >> 25) & 0x1f;
					if ((bits31_30 == 0x01) // Call
					 || ((bits31_30 == 0x02) && (bits24_19 == 0x38)) // Jmpl
					 || ((bits31_30 == 0x00) && (bits23_22 == 0x02) && (bits29_25 != 0x18))) {// Branch, but not (f)ba,a
						// The above test includes floating point branches
						ostrstream ost;
						ost << "Target of branch at " << hex << rtl->getAddress()
						    << " is delay slot of CTI at " << dest - 4;
						error(str(ost));
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
				addr += 4;
				break;

			case NCT:
				// Ordinary instruction. Add it to the list of RTLs this BB
				BB_rtls->push_back(rtl);
				addr += inst.numBytes;
				// Ret/restore epilogues are handled as ordinary RTLs now
				if (rtl->getKind() == RET_HRTL)
					sequentialDecode = false;
				break;

			case SKIP:
				{
					// We can't simply ignore the skipped delay instruction as there
					// will most likely be a branch to it so we simply set the jump
					// to go to one past the skipped instruction.
					rtl_jump->setDest(addr + 8);
					BB_rtls->push_back(rtl_jump);

					// Construct the new basic block and save its destination
					// address if it hasn't been visited already
					auto bb = cfg->newBB(BB_rtls, ONEWAY, 1);
					handleBranch(addr + 8, upper, bb, cfg);

					// There is no fall through branch.
					sequentialDecode = false;
					addr += 8;       // Update address for coverage
				}
				break;

			case SU:
				// Ordinary, non-delay branch or call/return
				if (rtl->getKind() == CALL_HRTL) {
					// This is a call followed by a return, e.g. a BL to printf
					case_CALL_NCT(addr, inst, nop_inst, BB_rtls, proc, callList);
				} else {
					BB_rtls->push_back(rtl_jump);
					auto bb = cfg->newBB(BB_rtls, ONEWAY, 1);
					handleBranch(rtl_jump->getFixedDest(), upper, bb, cfg);
					addr += inst.numBytes;    // Update address for coverage
				}

				// There is no fall through branch, either way
				sequentialDecode = false;
				break;

			case SD:    // This includes cases where the link register is 2
			            // (i.e. a call)
				{
					auto delay_inst = decoder.decodeInstruction(addr + 4, delta, proc);
					delay_inst.rtl->updateNumBytes(delay_inst.numBytes);

					switch (delay_inst.type) {
					case NOP:
					case NCT:
						// Ordinary delayed instruction. Since NCT's can't
						// affect unconditional jumps, we put the delay
						// instruction before the jump or call
						if (rtl->getKind() == CALL_HRTL) {
							// This is a call followed by an NCT/NOP
							sequentialDecode = case_CALL_NCT(addr, inst, delay_inst, BB_rtls, proc, callList);
						} else {
							// This is a non-call followed by an NCT/NOP
							case_SD_NCT(addr, delta, upper, inst, delay_inst, BB_rtls, cfg);

							// There is no fall through branch.
							sequentialDecode = false;
						}
						break;

					case SKIP:
						case_unhandled_stub(addr);
						addr += 8;
						break;

					case SU:
						{
							// SD/SU.
							// This will be B.l (call or branch) followed by B.l.n. Our
							// interpretation is that it is as if the SD (i.e. the
							// B.l) now takes the destination of the SU
							// (i.e. the B.l.n). For example:
							//     B.l 1000,2 ;  B.l.n 2000
							// is really like:
							//     call 2000.

							// Just so that we can check that our interpretation is
							// correct the first time we hit this case...
							case_unhandled_stub(addr);

							// Adjust the destination of the SD and emit it.
							auto delay_jump = static_cast<HLJump *>(delay_inst.rtl);
							auto dest = delay_jump->getFixedDest();
							rtl_jump->setDest(dest);
							BB_rtls->push_back(rtl_jump);

							// Create the appropriate BB
							if (rtl->getKind() == CALL_HRTL) {
								handleCall(dest, cfg->newBB(BB_rtls, CALL, 1), cfg, addr, 8);

								// Set the address of the lexical successor of the
								// call that is to be decoded next. Set RTLs to
								// null so that a new list of RTLs will be created
								// for the next BB.
								BB_rtls = nullptr;
								addr += 8;

								// Add this call site to the set of call sites which
								// need to be analysed later.
								callList.push_back((HLCall *)inst.rtl);
							} else {
								auto bb = cfg->newBB(BB_rtls, ONEWAY, 1);
								handleBranch(dest, upper, bb, cfg);

								// There is no fall through branch.
								sequentialDecode = false;
							}
						}
						break;
					default:
						case_unhandled_stub(addr);
						addr += 8;       // Skip the pair
						break;
					}
				}
				break;

			case DD:
				{
					DecodeResult delay_inst;
					if (inst.numBytes == 4) {
						// Ordinary instruction. Look at the delay slot
						delay_inst = decoder.decodeInstruction(addr + 4, delta, proc);
						delay_inst.rtl->updateNumBytes(delay_inst.numBytes);
					} else {
						// Must be a prologue or epilogue or something.
						delay_inst = nop_inst;
						// Should be no need to adjust the coverage; the number of
						// bytes should take care of it
					}

					// Display RTL representation if asked
					if (progOptions.rtl && delay_inst.rtl)
						LOG << *delay_inst.rtl;

					switch (delay_inst.type) {
					case NOP:
					case NCT:
						sequentialDecode = case_DD_NCT(addr, delta, inst, delay_inst, BB_rtls, cfg, proc, callList, 8);
						break;
					default:
						case_unhandled_stub(addr);
						break;
					}
				}
				break;

			case DU:
				{
					// Same as DD case, but no delay slot to worry about
					DecodeResult delay_inst = nop_inst;

					sequentialDecode = case_DD_NCT(addr, delta, inst, delay_inst, BB_rtls, cfg, proc, callList, 4);
				}
				break;

			case SCD:
				{
					// Always execute the delay instr, and branch if
					// condition is met.
					// Normally, the delayed instruction moves in front
					// of the branch. But if it affects a register being
					// used in the SCD, we may have to duplicate it as an orphan
					// in the true leg of the branch, and fall through to the
					// delay instruction in the "false" leg.
					// Instead of moving the delay instruction to an orphan BB, we
					// may have a duplicate of the delay instruction just before the
					// target; if so, we can branch to that and not need the orphan
					// We do just a binary comparison; that may fail to make this
					// optimisation if the instr has relative fields.

					auto delay_inst = decoder.decodeInstruction(addr + 4, delta, proc);
					delay_inst.rtl->updateNumBytes(delay_inst.numBytes);

					// Display low level RTL representation if asked
					if (progOptions.rtl && delay_inst.rtl)
						LOG << *delay_inst.rtl;

					switch (delay_inst.type) {
					case NOP:
					case NCT:
						sequentialDecode = case_SCD_NCT(addr, delta, upper, inst, delay_inst, BB_rtls, cfg);
						break;
					default:
						case_unhandled_stub(addr);
						break;
					}
				}
				break;

			case SCDAN:
				{
					// Execute the delay instruction if the branch is taken;
					// skip (anull) the delay instruction if branch not taken.
					auto delay_inst = decoder.decodeInstruction(addr + 4, delta, proc);
					delay_inst.rtl->updateNumBytes(delay_inst.numBytes);

					// Display RTL representation if asked
					if (progOptions.rtl && delay_inst.rtl)
						LOG << *delay_inst.rtl;

					switch (delay_inst.type) {
					case NOP:
						{
							// This is an ordinary two-way branch.
							// Add the branch to the list of RTLs for this BB
							BB_rtls->push_back(rtl);
							// Create the BB and add it to the CFG
							auto bb = cfg->newBB(BB_rtls, TWOWAY, 2);
							// Visit the destination of the branch; add "true" leg
							auto dest = rtl_jump->getFixedDest();
							handleBranch(dest, upper, bb, cfg);
							// Add the "false" leg: point past the delay inst
							cfg->addOutEdge(bb, addr + 8);
							addr += 8;          // Skip branch and delay
							BB_rtls = nullptr;  // Start new BB
						}
						break;

					case NCT:
						sequentialDecode = case_SCDAN_NCT(addr, delta, upper, inst, delay_inst, BB_rtls, cfg);
						break;

					default:
						case_unhandled_stub(addr);
						addr += 8;
						break;
					}
				}
				break;

			case SCDAT:
				{
					// Static Conditional Delayed, Anulled if Taken
					// Basically, like an ordinary undelayed jump, but has two
					// out-edges
					BB_rtls->push_back(rtl);        // Add the jump
					auto dest = ((HLJump *)rtl)->getFixedDest();
					auto bb = cfg->newBB(BB_rtls, TWOWAY, 2);
					handleBranch(dest, upper, bb, cfg);
					addr += 4;          // "Delay slot" instruction is next
					cfg->addOutEdge(bb, addr);  // False leg
					BB_rtls = nullptr;  // Start new list of RTLs for next BB
				}
				break;

			case NCTA:
				{
					// These instructions have been identified as anulling the
					// following instruction. First we decode the following instr
					BB_rtls->push_back(rtl);        // Add the jump
					DecodeResult follow_inst = decoder.decodeInstruction(addr + 4, delta, proc);
					HRTL *follow_rtl = follow_inst.rtl;
					follow_rtl->updateNumBytes(follow_inst.numBytes);

					int n = follow_rtl->getNumRT();
					for (int i = 0; i < n; i++) {
						RTAssgn *rt = (RTAssgn *)follow_rtl->elementAt(i);
						if (rt->getKind() == RTASSGN) {
							auto notNull = new SemStr;
							// We want L! r[ tpmNul ]
							*notNull << idLNot << idRegOf << idTemp << idTmpNul;
							rt->addGuard(notNull);
							delete notNull;
						}
					}

					BB_rtls->push_back(follow_rtl);         // Add the follow instr
					// Display low level RTL representation if asked
					if (progOptions.rtl && follow_rtl)
						LOG << *follow_rtl;

					addr += 8;           // Skip NCTA and following instr
				}

			}   // switch inst.type

			// Display RTL representation if asked
			if (progOptions.rtl && inst.rtl)
				LOG << *inst.rtl;

			// If sequentially decoding, check if the next address happens to
			// be the start of an existing BB. If so, finish off the current BB
			// (if any RTLs) as a fallthrough, and  no need to decode again
			// (unless it's an incomplete BB, then we do decode it).
			// In fact, mustn't decode twice, because it will muck up the
			// coverage, but also will cause subtle problems like add a call
			// to the list of calls to be processed, then delete the call RTL
			// (e.g. Pentium 134.perl benchmark)
			if (sequentialDecode && cfg->existsBB(addr)) {
				// Create the fallthrough BB, if there are any RTLs at all
				if (BB_rtls) {
					// Add an out edge to this address
					auto bb = cfg->newBB(BB_rtls, FALL, 1);
					cfg->addOutEdge(bb, addr);
					BB_rtls = nullptr;      // Need new list of RTLs
				}
				// Pick a new address to decode from, if the BB is complete
				if (!cfg->isIncomplete(addr))
					sequentialDecode = false;
			}

		}       // while (sequentialDecode)
	}

	// Add the callees to the set of HLCalls to proces for CSR, and also
	// to the Prog object
	for (auto it = callList.begin(); it != callList.end(); ++it) {
		auto dest = (*it)->getFixedDest();
		// Don't speculatively decode procs that are outside of the main text
		// section, apart from dynamically linked ones (in the .plt)
		if (prog.pBF->isDynamicLinkedProc(dest) || !spec || (dest < upper)) {
			// Don't visit the destination of a register call
			if (dest != NO_ADDRESS) prog.visitProc(dest);
		}
	}
	return true;
}

/*==============================================================================
 * FUNCTION:      emitNop
 * OVERVIEW:      Emit a null RTL with the given address.
 * PARAMETERS:    rtls - List of RTLs to append this instruction to
 *                addr - Native address of this instruction
 *============================================================================*/
void
emitNop(HRTLList &rtls, ADDRESS addr)
{
	// Emit a null RTL with the given address. Required to cope with
	// SKIP instructions. Yes, they really happen, e.g. /usr/bin/vi 2.5
	rtls.push_back(new RTL(addr));
}

/*==============================================================================
 * FUNCTION:      emitCopyPC
 * OVERVIEW:      Emit the RTL for a call $+8 instruction, which is merely
 *                  %o7 = %pc
 * NOTE:          Assumes that the delay slot RTL has already been pushed; we
 *                  must push the semantics BEFORE that RTL, since the delay
 *                  slot instruction may use %o7. Example:
 *                  CALL $+8            ! This code is common in startup code
 *                  ADD  %o7, 20, %o0
 * PARAMETERS:    rtls - list of RTLs to append to
 *                addr - native address for the RTL
 *============================================================================*/
void
emitCopyPC(HRTLList &rtls, ADDRESS addr)
{
	// Emit %o7 = %pc
	auto pssSrc = new SemStr;
	pssSrc->push(idPC);
	auto pssDest = new SemStr;
	pssDest->push(idRegOf);
	pssDest->push(idIntConst);
	pssDest->push(15);      // %o7
	// Make an assignment RT
	auto pRt = new RTAssgn(pssDest, pssSrc, 32);
	// Add the RT to an RTL
	HRTL *rtl = new RTL(addr);
	rtl->appendRT(pRt);
	// Add the RTL to the list of RTLs, but to the second last position
	rtls.insert(--rtls.end(), rtl);
}

// Append one assignment to a list of RTLs
void
appendAssignment(HRTLList &rtls, ADDRESS addr, SemStr *lhs, SemStr *rhs, int size)
{
	auto rta = new RTAssgn(lhs, rhs, size);
	// Create an RTL with this one RT
	auto lrt = new list<RT *>;
	lrt->push_back(rta);
	HRTL *rtl = new RTL(addr, lrt);
	// Append this RTL to the list of RTLs for this BB
	rtls.push_back(rtl);
}

/*==============================================================================
 * FUNCTION:        helperFunc
 * OVERVIEW:        Checks for sparc specific helper functions like .urem,
 *                      which have specific semantics.
 * NOTE:            This needs to be handled in a resourcable way.
 * PARAMETERS:      dest: destination of the call (native address)
 *                  addr: address of current instruction (native addr)
 *                  rtls: list of RTL* for current BB
 * RETURNS:         True if a helper function was found and handled; false
 *                      otherwise
 *============================================================================*/
// Determine if this is a helper function, e.g. .mul. If so, append the
// appropriate RTLs to rtls, and return true
bool
helperFunc(HRTLList &rtls, ADDRESS addr, ADDRESS dest)
{
	// Helper functions are millicode, and don't seem to appear in the imports
	// section. So they don't appear to be dynamically linked
//	if (!prog.pBF->isDynamicLinkedProc(dest)) return false;
	const char *p = prog.pBF->getSymbolByAddress(dest);
	if (!p) return false;
	auto name = std::string(p);
//	if (!progOptions.fastInstr)
//		return helperFuncLong(rtls, addr, name);
	auto rhs = new SemStr;
	if (name == "$$remU") {
		// %r26 % %r25
		*rhs << idMod
		     << idRegOf << idIntConst << 26
		     << idRegOf << idIntConst << 25;
	} else if (name == "$$remI") {
		// %r26 %! %r25
		*rhs << idMods
		     << idRegOf << idIntConst << 26
		     << idRegOf << idIntConst << 25;
	} else if (name == "$$divU") {
		// %r26 / %r25
		*rhs << idDiv
		     << idRegOf << idIntConst << 26
		     << idRegOf << idIntConst << 25;
	} else if (name == "$$divI") {
		// %r26 /! %r25
		*rhs << idDivs
		     << idRegOf << idIntConst << 26
		     << idRegOf << idIntConst << 25;
	} else if (name == "$$dyncall") {
		// *(r22)()
		list<RT *> ll;
		auto call = new HLCall(addr);
		auto dest = new SemStr;
		*dest << idMemOf << idRegOf << idIntConst << 22;
		call->setDest(dest);
		// Append this RTL to the list of RTLs for this BB
		rtls.push_back(call);
		return true;
	} else {
		// Not a (known) helper function
		delete rhs;
		return false;
	}
	// Need to make an RTAssgn with %r29 = rhs
	// Note: r29 is the millicode return value register. This code assumes that
	// all helper functions are millicode functions!
	auto lhs = new SemStr;
	*lhs << idRegOf << idIntConst << 29;
	auto rta = new RTAssgn(lhs, rhs, 32);
	// Create an RTL with this one RT
	auto lrt = new list<RT *>;
	lrt->push_back(rta);
	HRTL *rtl = new RTL(addr, lrt);
	// Append this RTL to the list of RTLs for this BB
	rtls.push_back(rtl);
	return true;
}

#if 0
/* Small "local" function to build an expression with
 * *128* m[m[r[14]+64]] = m[r[8]] OP m[r[9]] */
void
quadOperation(HRTLList &rtls, ADDRESS addr, int op)
{
	auto lhs = new SemStr(Type(FLOATP, 128, true));
	auto rhs = new SemStr(Type(FLOATP, 128, true));
	lhs->push(idMemOf);
	lhs->push(idMemOf);
	lhs->push(idPlus);
	lhs->push(idRegOf);
	lhs->push(idIntConst);
	lhs->push(14);
	lhs->push(idIntConst);
	lhs->push(64);
	rhs->push(op);
	rhs->push(idMemOf);
	rhs->push(idRegOf);
	rhs->push(idIntConst);
	rhs->push(8);
	rhs->push(idMemOf);
	rhs->push(idRegOf);
	rhs->push(idIntConst);
	rhs->push(9);
	appendAssignment(rtls, addr, lhs, rhs, 128);
}
// This is the long version of helperFunc (i.e. -f not used). This does the
// complete 64 bit semantics
bool
helperFuncLong(HRTLList &rtls, ADDRESS addr, const std::string &name)
{
	auto rhs = new SemStr;
	auto lhs = new SemStr;
	auto lrt = new list<RT *>;
	int tmpl = theSemTable.findItem("tmpl");
	if (name == ".umul") {
		// r[tmpl] = sgnex(32, 64, r8) * sgnex(32, 64, r9)
		*lhs << idRegOf << idTemp << tmpl;
		*rhs << idMult
		     << idSgnEx << 32 << 64 << idRegOf << idIntConst << 8
		     << idSgnEx << 32 << 64 << idRegOf << idIntConst << 9;
		lrt->push_back(new RTAssgn(lhs, rhs, 64));
		// r8 = truncs(64, 32, r[tmpl]);
		lhs = new SemStr;
		rhs = new SemStr;
		*lhs << idRegOf << idIntConst << 8;
		*rhs << idTruncs << 64 << 32 << idRegOf << idTemp << tmpl;
		lrt->push_back(new RTAssgn(lhs, rhs, 32));
		// r9 = r[tmpl]@32:63;
		lhs = new SemStr;
		rhs = new SemStr;
		*lhs << idRegOf << idIntConst << 9;
		*rhs << idAt << idRegOf << idTemp << tmpl << idIntConst << 32 << idIntConst << 63;
		lrt->push_back(new RTAssgn(lhs, rhs, 32));
		HRTL *rtl = new RTL(addr, lrt);
		rtls.push_back(rtl);
		return true;
	} else if (name == ".mul") {
		// r[tmpl] = sgnex(32, 64, r8) *! sgnex(32, 64, r9)
		*lhs << idRegOf << idTemp << tmpl;
		*rhs << idMults
		     << idSgnEx << 32 << 64 << idRegOf << idIntConst << 8
		     << idSgnEx << 32 << 64 << idRegOf << idIntConst << 9;
		lrt->push_back(new RTAssgn(lhs, rhs, 64));
		// r8 = truncs(64, 32, r[tmpl]);
		lhs = new SemStr;
		rhs = new SemStr;
		*lhs << idRegOf << idIntConst << 8;
		*rhs << idTruncs << 64 << 32 << idRegOf << idTemp << tmpl;
		lrt->push_back(new RTAssgn(lhs, rhs, 32));
		// r9 = r[tmpl]@32:63;
		lhs = new SemStr;
		rhs = new SemStr;
		*lhs << idRegOf << idIntConst << 9;
		*rhs << idAt << idRegOf << idTemp << tmpl << idIntConst << 32 << idIntConst << 63;
		lrt->push_back(new RTAssgn(lhs, rhs, 32));
		HRTL *rtl = new RTL(addr, lrt);
		rtls.push_back(rtl);
		return true;
	} else if (name == ".udiv") {
		// %o0 / %o1
		*rhs << idDiv
		     << idRegOf << idIntConst << 8
		     << idRegOf << idIntConst << 9;
	} else if (name == ".div") {
		// %o0 /! %o1
		*rhs << idDivs
		     << idRegOf << idIntConst << 8
		     << idRegOf << idIntConst << 9;
	} else if (name == ".urem") {
		// %o0 % %o1
		*rhs << idMod
		     << idRegOf << idIntConst << 8
		     << idRegOf << idIntConst << 9;
	} else if (name == ".rem") {
		// %o0 %! %o1
		*rhs << idMods
		     << idRegOf << idIntConst << 8
		     << idRegOf << idIntConst << 9;
//	} else if (name.substr(0, 6) == ".stret") {
//		// No operation. Just use %o0
//		rhs->push(idRegOf);
//		rhs->push(idIntConst);
//		rhs->push(8);
	} else if (name == "_Q_mul") {
		// Pointers to args are in %o0 and %o1; ptr to result at [%sp+64]
		// So semantics is m[m[r[14]] = m[r[8]] *f m[r[9]]
		quadOperation(rtls, addr, idFMult);
		return true;
	} else if (name == "_Q_div") {
		quadOperation(rtls, addr, idFDiv);
		return true;
	} else if (name == "_Q_add") {
		quadOperation(rtls, addr, idFPlus);
		return true;
	} else if (name == "_Q_sub") {
		quadOperation(rtls, addr, idFMinus);
		return true;
	} else {
		// Not a (known) helper function
		delete lhs;
		delete rhs;
		delete lrt;
		return false;
	}
	// Need to make an RTAssgn with %o0 = rhs
	*lhs << idRegOf << idIntConst << 8;
	appendAssignment(rtls, addr, lhs, rhs, 32);
	return true;
}
#endif

/*==============================================================================
 * FUNCTION:        setReturnLocations
 * OVERVIEW:        Set the return location for the given callee epilogue
 *                      to be the standard set of Hppa locations, using iReg
 *                      (always 28) to return integers
 * NOTE:            This is part of a hack that would go away if we could have
 *                  Logues that were both caller-prologues and callee-epilogues
 * PARAMETERS:      epilogue: pointer to a CalleeEpilogue object that is to
 *                      have it's return spec set
 *                  iReg: The register that integers are returned in
 * RETURNS:         nothing
 *============================================================================*/
void
setReturnLocations(CalleeEpilogue *epilogue, int iReg)
{
	// This function is somewhat similar to CSRParser::setReturnLocations() in
	// CSR/csrparser.y
	// First need to set up spec, which is a ReturnLocation object with the
	// four Sparc return locations
	typeToSemStrMap retMap;
	SemStr ss;
	ss.push(idRegOf);
	ss.push(idIntConst);
	ss.push(iReg);
	retMap.insert(pair<Type, SemStr>(Type(::INTEGER), ss));
	retMap.insert(pair<Type, SemStr>(Type(::DATA_ADDRESS), ss));
	// FIXME! This is sparc specific, but we haven't considered floating point
	// as yet
	// Now we want ss to be %f0, i.e. r[32]
	ss.substIndex(2, 32);
	retMap.insert(pair<Type, SemStr>(Type(::FLOATP, 32), ss));
	// Now we want ss to be %f0to1, i.e. r[64]
	ss.substIndex(2, 64);
	retMap.insert(pair<Type, SemStr>(Type(::FLOATP, 64), ss));
	ReturnLocations spec(retMap);
	epilogue->setRetSpec(spec);
}
