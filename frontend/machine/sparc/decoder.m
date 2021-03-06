/**
 * \file
 * \brief Implementation of the SPARC specific parts of the SparcDecoder
 *        class.
 *
 * \authors
 * Copyright (C) 1996-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sparcdecoder.h"

#include "boomerang.h"
#include "exp.h"
#include "prog.h"
#include "rtl.h"
#include "statement.h"

#include <cstring>

class Proc;

#define DIS_ROI     (dis_RegImm(roi, bf))
#define DIS_ADDR    (dis_Eaddr(addr, bf))
#define DIS_RD      (dis_RegLhs(rd))
#define DIS_RDR     (dis_RegRhs(rd))
#define DIS_RS1     (dis_RegRhs(rs1))
#define DIS_FS1S    (dis_RegRhs(fs1s + 32))
#define DIS_FS2S    (dis_RegRhs(fs2s + 32))
// Note: Sparc V9 has a second set of double precision registers that have an
// odd index. So far we only support V8
#define DIS_FDS     (dis_RegLhs(fds + 32))
#define DIS_FS1D    (dis_RegRhs((fs1d >> 1) + 64))
#define DIS_FS2D    (dis_RegRhs((fs2d >> 1) + 64))
#define DIS_FDD     (dis_RegLhs((fdd  >> 1) + 64))
#define DIS_FDQ     (dis_RegLhs((fdq  >> 2) + 80))
#define DIS_FS1Q    (dis_RegRhs((fs1q >> 2) + 80))
#define DIS_FS2Q    (dis_RegRhs((fs2q >> 2) + 80))

#define addressToPC(pc) (pc)
#define fetch32(pc) bf->readNative4(pc)

SparcDecoder::SparcDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get().getProgPath() + "frontend/machine/sparc/sparc.ssl";
	RTLDict.readSSLFile(file);
}

#if 0 // Cruft?
// For now...
int
SparcDecoder::decodeAssemblyInstruction(ADDRESS, ptrdiff_t)
{
	return 0;
}
#endif

/**
 * Create a HL Statement for a Bx instruction.  Instantiates a GotoStatement
 * for the unconditional branches, or a BranchStatement for the rest.
 *
 * \param pc    The location counter.
 * \param dest  The branch destination address.
 *
 * \returns  Pointer to newly created Statement, or null if invalid.
 */
Statement *
SparcDecoder::createBranch(ADDRESS pc, ADDRESS dest, const BinaryFile *bf)
{
	match pc to
	| BN =>
		return new GotoStatement(dest);
	| BA =>
		return new GotoStatement(dest);
	| BE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE);
		return br;
	}
	| BNE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE);
		return br;
	}
	| BLE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE);
		return br;
	}
	| BG => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG);
		return br;
	}
	| BL => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL);
		return br;
	}
	| BGE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE);
		return br;
	}
	| BLEU => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JULE);
		return br;
	}
	| BGU => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUG);
		return br;
	}
	| BCS => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUL);
		return br;
	}
	| BCC => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUGE);
		return br;
	}
	| BNEG => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JMI);
		return br;
	}
	| BPOS => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JPOS);
		return br;
	}
	| BVS =>
		return new GotoStatement(dest);
	| BVC =>
		return new GotoStatement(dest);

	// Predicted branches are handled identically.
	| BPN =>
		return new GotoStatement(dest);
	| BPA =>
		return new GotoStatement(dest);
	| BPE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE);
		return br;
	}
	| BPNE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE);
		return br;
	}
	| BPLE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE);
		return br;
	}
	| BPG => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG);
		return br;
	}
	| BPL => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL);
		return br;
	}
	| BPGE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE);
		return br;
	}
	| BPLEU => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JULE);
		return br;
	}
	| BPGU => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUG);
		return br;
	}
	| BPCS => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUL);
		return br;
	}
	| BPCC => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUGE);
		return br;
	}
	| BPNEG => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JMI);
		return br;
	}
	| BPPOS => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JPOS);
		return br;
	}
	| BPVS =>
		return new GotoStatement(dest);
	| BPVC =>
		return new GotoStatement(dest);

	| FBLG => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE, true);
		return br;
	}
	| FBE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE, true);
		return br;
	}
	| FBL => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL, true);
		return br;
	}
	| FBGE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE, true);
		return br;
	}
	| FBLE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE, true);
		return br;
	}
	| FBG => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG, true);
		return br;
	}

	// Just ignore unordered (for now)
	| FBNE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE, true);
		return br;
	}
	| FBUE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE, true);
		return br;
	}
	| FBUL => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL, true);
		return br;
	}
	| FBUGE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE, true);
		return br;
	}
	| FBULE => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE, true);
		return br;
	}
	| FBUG => {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG, true);
		return br;
	}

	else {
		// FBA, FBN, FBU, FBO are unhandled.
		// CBxxx branches (branches that depend on co-processor
		// instructions) are invalid, as far as we are concerned.
	}
	endmatch
	return nullptr;
}

void
SparcDecoder::decodeInstruction(DecodeResult &result, ADDRESS pc, const BinaryFile *bf)
{
	// Clear the result structure;
	result.reset();

	ADDRESS nextPC = NO_ADDRESS;
	match [nextPC] pc to

	| call__(addr) [name] =>
		/*
		 * A standard call
		 */
		auto newCall = new CallStatement(addr);

		Proc *destProc = prog->setNewProc(addr);
		if (destProc == (Proc *)-1) destProc = nullptr;
		newCall->setDestProc(destProc);
		result.rtl = new RTL(pc, newCall);
		result.type = SD;
		SHOW_ASM(name << " " << std::hex << addr << std::dec);
		DEBUG_STMTS

	| call_(addr) =>
		/*
		 * A JMPL with rd == %o7, i.e. a register call
		 */
		auto dest = DIS_ADDR;
		auto newCall = new CallStatement(dest);

		// Record the fact that this is a computed call
		newCall->setIsComputed();

		// Set the destination expression
		result.rtl = new RTL(pc, newCall);
		result.type = DD;

		SHOW_ASM("call_ " << *dest);
		DEBUG_STMTS

	| ret() =>
		/*
		 * Just a ret (non leaf)
		 */
		result.rtl = new RTL(pc, new ReturnStatement);
		result.type = DD;
		SHOW_ASM("ret");
		DEBUG_STMTS

	| retl() =>
		/*
		 * Just a ret (leaf; uses %o7 instead of %i7)
		 */
		result.rtl = new RTL(pc, new ReturnStatement);
		result.type = DD;
		SHOW_ASM("retl");
		DEBUG_STMTS

	| branch^",a"(tgt) [name] =>
		/*
		 * Anulled branch
		 */
		auto br = createBranch(pc, tgt, bf);
		if (!br) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BA,A" or "BN,A"
		result.type = SCDAN;
		if (strcmp(name, "BA,a") == 0 || strcmp(name, "BVC,a") == 0) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS

	| pbranch^",a"(cc01, tgt) [name] =>
		/*
		 * Anulled, predicted branch (treat as for non predicted)
		 */
		/* If 64 bit cc used, can't handle */
		if (strcmp(name, "BPA,a") != 0 && strcmp(name, "BPN,a") != 0 && cc01 != 0) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		auto br = createBranch(pc, tgt, bf);
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BPA,A" or "BPN,A"
		result.type = SCDAN;
		if (strcmp(name, "BPA,a") == 0 || strcmp(name, "BPVC,a") == 0) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS

	| branch(tgt) [name] =>
		/*
		 * Non anulled branch
		 */
		auto br = createBranch(pc, tgt, bf);
		if (!br) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BA" or "BN" (or the pseudo unconditionals BVx)
		result.type = SCD;
		if (strcmp(name, "BA") == 0 || strcmp(name, "BVC") == 0)
			result.type = SD;
		if (strcmp(name, "BN") == 0 || strcmp(name, "BVS") == 0)
			result.type = NCT;

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS

	| pbranch(cc01, tgt) [name] =>
		/* If 64 bit cc used, can't handle */
		if (strcmp(name, "BPA") != 0 && strcmp(name, "BPN") != 0 && cc01 != 0) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		auto br = createBranch(pc, tgt, bf);
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BPA" or "BPN" (or the pseudo unconditionals BPVx)
		result.type = SCD;
		if (strcmp(name, "BPA") == 0 || strcmp(name, "BPVC") == 0)
			result.type = SD;
		if (strcmp(name, "BPN") == 0 || strcmp(name, "BPVS") == 0)
			result.type = NCT;

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS

	| JMPL(addr, _) [name] =>
	//| JMPL(addr, rd) =>
		/*
		 * JMPL, with rd != %o7, i.e. register jump
		 * Note: if rd==%o7, then would be handled with the call_ arm
		 */
		auto jump = new CaseStatement(DIS_ADDR);
		// Record the fact that it is a computed jump
		jump->setIsComputed();
		result.rtl = new RTL(pc, jump);
		result.type = DD;
		SHOW_ASM(name);
		DEBUG_STMTS


	//  //  //  //  //  //  //  //
	//                          //
	//   Ordinary instructions  //
	//                          //
	//  //  //  //  //  //  //  //

	| SAVE(rs1, roi, rd) [name] =>
		// Decided to treat SAVE as an ordinary instruction
		// That is, use the large list of effects from the SSL file, and
		// hope that optimisation will vastly help the common cases
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI, DIS_RD);

	| RESTORE(rs1, roi, rd) [name] =>
		// Decided to treat RESTORE as an ordinary instruction
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI, DIS_RD);

	| NOP [name] =>
		result.type = NOP;
		result.rtl = instantiate(pc, name);

	| sethi(imm22, rd) [name] =>
		result.rtl = instantiate(pc, name, dis_Num(imm22), DIS_RD);

	| load_greg(addr, rd) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR, DIS_RD);

	| LDF(addr, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR, DIS_FDS);

	| LDDF(addr, fdd) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR, DIS_FDD);

	| load_asi(addr, _, rd) [name] =>
	//| load_asi(addr, asi, rd) [name] => // Note: this could be serious!
		result.rtl = instantiate(pc, name, DIS_RD, DIS_ADDR);

	| sto_greg(rd, addr) [name] =>
		// Note: RD is on the "right hand side" only for stores
		result.rtl = instantiate(pc, name, DIS_RDR, DIS_ADDR);

	| STF(fds, addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_FDS, DIS_ADDR);

	| STDF(fdd, addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_FDD, DIS_ADDR);

	| sto_asi(rd, addr, _) [name] =>
	//| sto_asi(rd, addr, asi) [name] => // Note: this could be serious!
		result.rtl = instantiate(pc, name, DIS_RDR, DIS_ADDR);

	| LDFSR(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);

	| LDCSR(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);

	| STFSR(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);

	| STCSR(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);

	| STDFQ(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);

	| STDCQ(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);

	| RDY(rd) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD);

	| RDPSR(rd) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD);

	| RDWIM(rd) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD);

	| RDTBR(rd) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD);

	| WRY(rs1, roi) [name] =>
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);

	| WRPSR(rs1, roi) [name] =>
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);

	| WRWIM(rs1, roi) [name] =>
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);

	| WRTBR(rs1, roi) [name] =>
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);

	| alu(rs1, roi, rd) [name] =>
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI, DIS_RD);

	| float2s(fs2s, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDS);

	| float3s(fs1s, fs2s, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS1S, DIS_FS2S, DIS_FDS);

	| float3d(fs1d, fs2d, fdd) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS1D, DIS_FS2D, DIS_FDD);

	| float3q(fs1q, fs2q, fdq) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS1Q, DIS_FS2Q, DIS_FDQ);

	| fcompares(fs1s, fs2s) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS1S, DIS_FS2S);

	| fcompared(fs1d, fs2d) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS1D, DIS_FS2D);

	| fcompareq(fs1q, fs2q) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS1Q, DIS_FS2Q);

	| FTOs(fs2s, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDS);

	// Note: itod and dtoi have different sized registers
	| FiTOd(fs2s, fdd) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDD);
	| FdTOi(fs2d, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDS);

	| FiTOq(fs2s, fdq) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDQ);
	| FqTOi(fs2q, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDS);

	| FsTOd(fs2s, fdd) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDD);
	| FdTOs(fs2d, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDS);

	| FsTOq(fs2s, fdq) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDQ);
	| FqTOs(fs2q, fds) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDS);

	| FdTOq(fs2d, fdq) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDQ);
	| FqTOd(fs2q, fdd) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDD);


	| FSQRTd(fs2d, fdd) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDD);

	| FSQRTq(fs2q, fdq) [name] =>
		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDQ);


	// In V9, the privileged RETT becomes user-mode RETURN
	// It has the semantics of "ret restore" without the add part of the restore
	| RETURN(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);
		result.rtl->appendStmt(new ReturnStatement);
		result.type = DD;

	| trap(addr) [name] =>
		result.rtl = instantiate(pc, name, DIS_ADDR);

	| UNIMP(_) =>
	//| UNIMP(n) =>
		result.valid = false;

	| inst = _ =>
	//| inst = n =>
		// What does this mean?
		result.valid = false;

	else
		result.valid = false;
	endmatch

	result.numBytes = nextPC - pc;
}

/**
 * Decode the register on the LHS.
 *
 * \param r  Register (0-31).
 * \returns  The expression representing the register.
 */
Exp *
SparcDecoder::dis_RegLhs(unsigned r)
{
	return Location::regOf(r);
}

/**
 * Decode the register on the RHS.
 *
 * \note  Replaces r[0] with const 0.
 * \note  Not used by DIS_RD since don't want 0 on LHS.
 *
 * \param r  Register (0-31).
 * \returns  The expression representing the register.
 */
Exp *
SparcDecoder::dis_RegRhs(unsigned r)
{
	if (r == 0)
		return new Const(0);
	return Location::regOf(r);
}

/**
 * Decode the register or immediate at the given address.
 *
 * \param pc  An address in the instruction stream.
 * \returns   The register or immediate at the given address.
 */
Exp *
SparcDecoder::dis_RegImm(ADDRESS pc, const BinaryFile *bf)
{
	match pc to
	| imode(simm13) =>
		return new Const(simm13);
	| rmode(rs2) =>
		return dis_RegRhs(rs2);
	endmatch
}

/**
 * Converts a dynamic address to a Exp* expression.
 * E.g. %o7 --> r[ 15 ]
 *
 * \param pc  The instruction stream address of the dynamic address.
 *
 * \returns  The Exp* representation of the given address.
 */
Exp *
SparcDecoder::dis_Eaddr(ADDRESS pc, const BinaryFile *bf)
{
	match pc to
	| indirectA(rs1) =>
		return Location::regOf(rs1);
	| indexA(rs1, rs2) =>
		return new Binary(opPlus,
		                  Location::regOf(rs1),
		                  Location::regOf(rs2));
	| absoluteA(simm13) =>
		return new Const((int)simm13);
	| dispA(rs1, simm13) =>
		return new Binary(opPlus,
		                  Location::regOf(rs1),
		                  new Const((int)simm13));
	endmatch
}

#if 0 // Cruft?
/**
 * Check to see if the instructions at the given offset match any callee
 * prologue, i.e. does it look like this offset is a pointer to a function?
 *
 * \param hostPC  Pointer to the code in question (host address).
 * \returns       True if a match found.
 */
bool
SparcDecoder::isFuncPrologue(ADDRESS hostPC)
{
#if 0  // Can't do this without patterns. It was a bit of a hack anyway
	int hiVal, loVal, reg, locals;
	if (InstructionPatterns::new_reg_win(prog.csrSrc, hostPC, locals))
		return true;
	if (InstructionPatterns::new_reg_win_large(prog.csrSrc, hostPC, hiVal, loVal, reg))
		return true;
	if (InstructionPatterns::same_reg_win(prog.csrSrc, hostPC, locals))
		return true;
	if (InstructionPatterns::same_reg_win_large(prog.csrSrc, hostPC, hiVal, loVal, reg))
		return true;
#endif
	return false;
}
#endif

/**
 * Check to see if the instruction at the given offset is a restore
 * instruction.
 *
 * \param pc      Pointer to the code in question.
 * \returns       True if a match found.
 */
bool
SparcDecoder::isRestore(ADDRESS pc, const BinaryFile *bf)
{
	match pc to
	| RESTORE =>
		return true;
	else
		return false;
	endmatch
}
