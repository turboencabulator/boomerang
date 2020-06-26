/**
 * \file
 *
 * This file contains routines to manage the decoding of mc68k instructions
 * and the instantiation to RTLs.  These functions replace Frontend.cc for
 * decoding mc68k instructions.
 *
 * \authors
 * Copyright (C) 2000-2001, The University of Queensland
 * \authors
 * Copyright (C) 2000-2001, Sun Microsystems, Inc
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "global.h"
#include "frontend.h"
#include "decoder.h"        // prototype for decodeInstruction()
#include "rtl.h"
#include "cfg.h"
#include "ss.h"
#include "prog.h"           // For findProc()
#include "proc.h"
#include "options.h"
#include "BinaryFile.h"     // For getSymbolByAddress()
#include "csr.h"            // For class CalleeEpilogue

// Queues used by various functions
queue<ADDRESS> qLabels;     // Queue of labels this procedure

void initCti();             // Imp in cti68k.cc

struct SemCmp {
	bool operator ()(const SemStr &ss1, const SemStr &ss2) const;
};

bool
SemCmp::operator ()(const SemStr &ss1, const SemStr &ss2) const
{
	return ss1 < ss2;
}

// A map from semantic string to integer (for identifying branch statements)
static map<SemStr, int, SemCmp> condMap;

int arrConds[12][7] = {
	{ idZF },                                                // HLJCOND_JE: Z
	{ idNot, idZF },                                         // HLJCOND_JNE: ~Z
	{ idBitXor, idNF, idOF },                                // HLJCOND_JSL: N ^ O
	{ idBitOr, idBitXor, idNF, idOF, idZF },                 // HLJCOND_JSLE: (N ^ O) | Z
	{ idNot, idBitXor, idNF, idOF },                         // HLJCOND_JGES: ~(N ^ O)
	{ idBitAnd, idNot, idBitXor, idNF, idOF, idNot, idZF },  // HLJCOND_JSG: ~(N ^ O) & ~Z
	{ idCF },                                                // HLJCOND_JUL: C
	{ idBitOr, idCF, idZF },                                 // HLJCOND_JULE: C | Z
	{ idNot, idCF },                                         // HLJCOND_JUGE: ~C
	{ idBitAnd, idNot, idCF, idNot, idZF },                  // HLJCOND_JUG: ~C & ~Z
	{ idNF },                                                // HLJCOND_JMI: N
	{ idNot, idNF },                                         // HLJCOND_JPOS: ~N
};

// Ugly. The lengths of the above arrays.
int condLengths[12] = { 1, 2, 3, 5, 4, 7, 1, 3, 2, 5, 1, 2 };

/**
 * \brief Initialise the front end.
 */
void
initFront()
{
	for (int i = 0; i < 12; i++) {
		SemStr ss;
		ss.pushArr(condLengths[i], arrConds[i]);
		condMap[ss] = i;
	}
}

/**
 * \brief Get the condition, given that it's something like %NF ^ %OF.
 */
JCOND_TYPE
getCond(const SemStr *pCond)
{
	map<SemStr, int, SemCmp>::const_iterator mit;

	mit = condMap.find(*pCond);
	if (mit == condMap.end()) {
		ostrstream os;
		os << "Condition ";
		pCond->print(os);
		os << " not known";
		error(os.str());
		return (JCOND_TYPE)0;
	}
	return (JCOND_TYPE)(*mit).second;
}

/**
 * \brief Process a procedure, given a native (source machine) address.
 *
 * \param addr  The address at which the procedure starts.
 * \param proc  The procedure object.
 * \param spec  true if a speculative decode.
 * \returns     true if successful decode.
 */
bool
FrontEndSrc::processProc(ADDRESS addr, UserProc *proc, bool spec)
{
	// Call the base class to do all of the work
	return FrontEnd::processProc(addr, proc, spec);
}

#if 0
/**
 * \brief Process a procedure, given a native (source machine) address.
 *
 * \param addr     The address at which the procedure starts.
 * \param delta    The offset of the above address from the logical address at
 *                 which the procedure starts (i.e. the one given by dis).
 * \param upper    The highest address of the text segment.
 * \param proc     The procedure object.
 * \param decoder  NJMCDecoder object.
 */
void
processProc(ADDRESS addr, int delta, ADDRESS upper, UserProc *proc, NJMCDecoder &decoder)
{
	INSTTYPE type;              // Cfg type of instruction (e.g. IRET)

	Cfg *cfg = proc->getCFG();

	// Initialise the queue of control flow targets that have yet to be decoded.
	cfg->enqueue(addr);

	// Clear the pointer used by the caller prologue code to access the last
	// call rtl of this procedure
	//decoder.resetLastCall();

	while ((addr = cfg->dequeue()) != NO_ADDRESS) {
		// The list of RTLs for the current basic block
		auto BB_rtls = (list<HRTL *> *)nullptr;

		// Indicates whether or not the next instruction to be decoded is the lexical successor of the current one.
		// Will be true for all NCTs and for CTIs with a fall through branch.
		// Keep decoding sequentially until a CTI without a fall through branch is decoded
		bool sequentialDecode = true;
		while (sequentialDecode) {

			// Decode and classify the current instruction
			if (progOptions.trace)
				cout << "*" << hex << addr << "\t" << flush;

			// Decode the inst at addr.
			auto inst = decoder.decodeInstruction(addr, delta, proc);

			// Need to construct a new list of RTLs if a basic block has just
			// been finished but decoding is continuing from its lexical
			// successor
			if (!BB_rtls)
				BB_rtls = new list<HRTL *>();

			if (inst.numBytes == 0) {
				// An invalid instruction. Most likely because a call did
				// not return (e.g. call _exit()), etc. Best thing is to
				// emit a INVALID BB, and continue with valid instructions
				ostrstream ost;
				ost << "invalid instruction at " << hex << addr;
				warning(str(ost));
				// Emit the RTL anyway, so we have the address and maybe
				// some other clues
				BB_rtls->push_back(new RTL(addr));
				auto bb = cfg->newBB(BB_rtls, INVALID);
				BB_rtls = nullptr;
				sequentialDecode = false;
				continue;
			}

			// Display RTL representation if asked
			if (progOptions.rtl) inst.rtl->print();

			switch (inst.rtl->getKind()) {
			case JUMP_HRTL:
				{
					auto rtl_jump = static_cast<HLJump *>(inst.rtl);
					auto dest = rtl_jump->getFixedDest();

					// Handle one way jumps and computed jumps separately
					if (dest != NO_ADDRESS) {
						BB_rtls->push_back(inst.rtl);
						auto bb = cfg->newBB(BB_rtls, ONEWAY);
						sequentialDecode = false;

						// Add the out edge if it is to a destination within the
						// procedure
						if (dest < upper) {
							cfg->visit(dest, bb);
							cfg->addOutEdge(bb, dest);
						} else {
							ostrstream ost;
							ost << "Error: Instruction at " << hex << addr;
							ost << " branches beyond end of section, to ";
							ost << dest;
							error(str(ost));
						}
					}
				}
				break;

			case NWAYJUMP_HRTL:
				{
					auto rtl_jump = static_cast<HLJump *>(inst.rtl);
					// We create the BB as a COMPJUMP type, then change
					// to an NWAY if it turns out to be a switch stmt
					BB_rtls->push_back(inst.rtl);
					auto bb = cfg->newBB(BB_rtls, COMPJUMP);
					sequentialDecode = false;
					if (isSwitch(bb, rtl_jump->getDest(), proc, pBF)) {
						processSwitch(bb, delta, cfg, pBF);
					} else { // Computed jump
						// Not a switch statement
						ostrstream ost;
						string sKind("JUMP");
						if (type == I_COMPCALL) sKind = "CALL";
						ost << "COMPUTED " << sKind
						    << " at " << hex << addr << endl;
						warning(str(ost));
						BB_rtls = nullptr;  // New HRTLList for next BB
					}
				}
				break;

			case JCOND_HRTL:
				{
					auto rtl_jump = static_cast<HLJump *>(inst.rtl);
					auto dest = rtl_jump->getFixedDest();
					BB_rtls->push_back(inst.rtl);
					auto bb = cfg->newBB(BB_rtls, TWOWAY);
					BB_rtls = nullptr;

					// Add the out edge if it is to a destination within the
					// procedure
					if (dest < upper) {
						cfg->visit(dest, bb);
						cfg->addOutEdge(bb, dest);
					} else {
						ostrstream ost;
						ost << "Error: Instruction at " << hex << addr;
						ost << " branches beyond end of section, to ";
						ost << dest;
						error(str(ost));
					}

					// Add the fall-through outedge
					cfg->addOutEdge(bb, addr + inst.numBytes);

					// Continue with the next instruction.
				}
				break;

			case CALL_HRTL:
				{
					auto call = static_cast<HLCall *>(inst.rtl);

					// Treat computed and static calls seperately
					if (call->isComputed()) {
						BB_rtls->push_back(inst.rtl);
						auto bb = cfg->newBB(BB_rtls, COMPCALL);
						BB_rtls = nullptr;

						cfg->addOutEdge(bb, addr + inst.numBytes);

					} else {      // Static call

						BB_rtls->push_back(inst.rtl);
						auto bb = cfg->newBB(BB_rtls, CALL);
						BB_rtls = nullptr;

						// Find the address of the callee.
						ADDRESS newAddr = call->getFixedDest();

						// Add this non computed call site to the set of call
						// sites which need to be analysed later.
						//cfg->addCall(call);

						// Record the called address as the start of a new
						// procedure if it didn't already exist.
						if ((newAddr != NO_ADDRESS)
						 && !prog.findProc(newAddr)) {
							prog.visitProc(newAddr);
							if (progOptions.trace)
								cout << "p" << hex << newAddr << "\t" << flush;
						}

						// Check if this is the _exit function. May prevent us from
						// attempting to decode invalid instructions.
						char *name = prog.pBF->getSymbolByAddress(newAddr);
						if (name && noReturnCallDest(name)) {
							sequentialDecode = false;
						} else {
							if (call->isReturnAfterCall()) {
								appendSyntheticReturn(bb, proc);
								// Give the enclosing proc a dummy callee epilogue
								proc->setEpilogue(new CalleeEpilogue("__dummy", list<string>()));
								// Mike: do we need to set return locations?
								// This ends the function
								sequentialDecode = false;
							} else {
								// Add the fall through edge
								cfg->addOutEdge(bb, addr + inst.numBytes);
							}
						}
					}

					// Continue with the next instruction.
				}
				break;

			case RET_HRTL:
				{
					BB_rtls->push_back(inst.rtl);
					auto bb = cfg->newBB(BB_rtls, RET);
					BB_rtls = nullptr;
					sequentialDecode = false;
				}
				break;

			case SCOND_HRTL:
				// This is just an ordinary instruction; no control transfer
				// Fall through
			case LOW_LEVEL_HRTL:
				// We must emit empty RTLs for NOPs, because they could be the
				// destinations of jumps (and splitBB won't work)
				// Just emit the current instr to the current BB
				BB_rtls->push_back(inst.rtl);
				break;

			} // switch (inst.rtl->getKind())

			addr += inst.numBytes;
			// Update the RTL's number of bytes for coverage analysis (only)
			inst.rtl->updateNumBytes(inst.numBytes);

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
					auto bb = cfg->newBB(BB_rtls, FALL);
					BB_rtls = nullptr;
					cfg->addOutEdge(bb, addr);
				}
				// Pick a new address to decode from, if the BB is complete
				if (cfg->isComplete(addr))
					sequentialDecode = false;
			}

		}   // while sequentialDecode
	}   // while cfg->dequeue()
}
#endif
