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
	std::string file = Boomerang::get()->getProgPath() + "frontend/machine/sparc/sparc.ssl";
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
 * Create an RTL for a Bx instruction.
 *
 * \param pc     The location counter.
 * \param name   Instruction name (e.g. "BNE,a", or "BPNE").
 *
 * \returns  Pointer to newly created RTL, or nullptr if invalid.
 */
RTL *
SparcDecoder::createBranchRtl(ADDRESS pc, const char *name)
{
	auto br = new BranchStatement();
	auto res = new RTL(pc, br);
	if (name[0] == 'F') {
		// fbranch is any of [ FBN FBNE FBLG FBUL FBL   FBUG FBG   FBU
		//                     FBA FBE  FBUE FBGE FBUGE FBLE FBULE FBO ],
		// fbranches are not the same as ibranches, so need a whole different set of tests
		if (name[2] == 'U')
			name++;  // Just ignore unordered (for now)
		switch (name[2]) {
		case 'E':
			br->setCondType(BRANCH_JE, true);  // FBE
			break;
		case 'L':
			if (name[3] == 'G')
				br->setCondType(BRANCH_JNE, true);   // FBLG
			else if (name[3] == 'E')
				br->setCondType(BRANCH_JSLE, true);  // FBLE
			else
				br->setCondType(BRANCH_JSL, true);   // FBL
			break;
		case 'G':
			if (name[3] == 'E')
				br->setCondType(BRANCH_JSGE, true);  // FBGE
			else
				br->setCondType(BRANCH_JSG, true);   // FBG
			break;
		case 'N':
			if (name[3] == 'E')
				br->setCondType(BRANCH_JNE, true);   // FBNE
			// Else it's FBN!
			break;
		default:
			std::cerr << "unknown float branch " << name << std::endl;
			delete res;
			res = nullptr;
		}
		return res;
	}

	// ibranch is any of [ BN BE  BLE BL  BLEU BCS BNEG BVS
	//                     BA BNE BG  BGE BGU  BCC BPOS BVC ],
	// Note: BPN, BPE, etc handled below
	switch (name[1]) {
	case 'E':
		br->setCondType(BRANCH_JE);  // BE
		break;
	case 'L':
		if (name[2] == 'E') {
			if (name[3] == 'U')
				br->setCondType(BRANCH_JULE);  // BLEU
			else
				br->setCondType(BRANCH_JSLE);  // BLE
		} else {
			br->setCondType(BRANCH_JSL);  // BL
		}
		break;
	case 'N':
		// BNE, BNEG (won't see BN)
		if (name[3] == 'G')
			br->setCondType(BRANCH_JMI);  // BNEG
		else
			br->setCondType(BRANCH_JNE);  // BNE
		break;
	case 'C':
		// BCC, BCS
		if (name[2] == 'C')
			br->setCondType(BRANCH_JUGE);  // BCC
		else
			br->setCondType(BRANCH_JUL);   // BCS
		break;
	case 'V':
		// BVC, BVS; should never see these now
		if (name[2] == 'C')
			std::cerr << "Decoded BVC instruction\n";  // BVC
		else
			std::cerr << "Decoded BVS instruction\n";  // BVS
		break;
	case 'G':
		// BGE, BG, BGU
		if (name[2] == 'E')
			br->setCondType(BRANCH_JSGE);  // BGE
		else if (name[2] == 'U')
			br->setCondType(BRANCH_JUG);   // BGU
		else
			br->setCondType(BRANCH_JSG);   // BG
		break;
	case 'P':
		if (name[2] == 'O') {
			br->setCondType(BRANCH_JPOS);  // BPOS
			break;
		}
		// Else, it's a BPXX; remove the P (for predicted) and try again
		// (recurse)
		// B P P O S ...
		// 0 1 2 3 4 ...
		char temp[8];
		temp[0] = 'B';
		strcpy(temp + 1, name + 2);
		delete res;
		return createBranchRtl(pc, temp);
	default:
		std::cerr << "unknown non-float branch " << name << std::endl;
	}
	return res;
}

/**
 * Attempt to decode the high level instruction at a given address and return
 * the corresponding HL type (e.g. CallStatement, GotoStatement etc).  If no
 * high level instruction exists at the given address, then simply return the
 * RTL for the low level instruction at this address.  There is an option to
 * also include the low level statements for a HL instruction.
 *
 * \param pc     The native address of the pc.
 * \param delta  The difference between the above address and the host address
 *               of the pc (i.e. the address that the pc is at in the loaded
 *               object file).
 * \param proc   The enclosing procedure.  This can be nullptr for those of us
 *               who are using this method in an interpreter.
 *
 * \returns  A DecodeResult structure containing all the information gathered
 *           during decoding.
 */
DecodeResult &
SparcDecoder::decodeInstruction(ADDRESS pc, const BinaryFile *bf)
{
	static DecodeResult result;

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
		SHOW_ASM(name << " " << std::hex << addr);
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

		// First, check for CBxxx branches (branches that depend on co-processor instructions). These are invalid,
		// as far as we are concerned
		if (name[0] == 'C') {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return result;
		}

		// Instantiate a GotoStatement for the unconditional branches, HLJconds for the rest.
		if (strcmp(name, "BA,a") == 0 || strcmp(name, "BN,a") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else if (strcmp(name, "BVS,a") == 0 || strcmp(name, "BVC,a") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else {
			result.rtl = createBranchRtl(pc, name);
			auto jump = (GotoStatement *)result.rtl->getList().back();
			jump->setDest(tgt);
		}

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BA,A" or "BN,A"
		result.type = SCDAN;
		if (strcmp(name, "BA,a") == 0 || strcmp(name, "BVC,a") == 0) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		SHOW_ASM(name << " " << std::hex << tgt);
		DEBUG_STMTS

	| pbranch^",a"(cc01, tgt) [name] =>
		/*
		 * Anulled, predicted branch (treat as for non predicted)
		 */

		// Instantiate a GotoStatement for the unconditional branches, HLJconds for the rest.
		if (strcmp(name, "BPA,a") == 0 || strcmp(name, "BPN,a") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else if (cc01 != 0) {  /* If 64 bit cc used, can't handle */
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return result;
		} else if (strcmp(name, "BPVS,a") == 0 || strcmp(name, "BPVC,a") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else {
			result.rtl = createBranchRtl(pc, name);
			auto jump = (GotoStatement *)result.rtl->getList().back();
			jump->setDest(tgt);
		}

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BPA,A" or "BPN,A"
		result.type = SCDAN;
		if (strcmp(name, "BPA,a") == 0 || strcmp(name, "BPVC,a") == 0) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		SHOW_ASM(name << " " << std::hex << tgt);
		DEBUG_STMTS

	| branch(tgt) [name] =>
		/*
		 * Non anulled branch
		 */

		// First, check for CBxxx branches (branches that depend on co-processor instructions). These are invalid,
		// as far as we are concerned
		if (name[0] == 'C') {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return result;
		}

		// Instantiate a GotoStatement for the unconditional branches, BranchStatement for the rest
		if (strcmp(name, "BA") == 0 || strcmp(name, "BN") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else if (strcmp(name, "BVS") == 0 || strcmp(name, "BVC") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else {
			result.rtl = createBranchRtl(pc, name);
			auto jump = (BranchStatement *)result.rtl->getList().back();
			jump->setDest(tgt);
		}

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BA" or "BN" (or the pseudo unconditionals BVx)
		result.type = SCD;
		if (strcmp(name, "BA") == 0 || strcmp(name, "BVC") == 0)
			result.type = SD;
		if (strcmp(name, "BN") == 0 || strcmp(name, "BVS") == 0)
			result.type = NCT;

		SHOW_ASM(name << " " << std::hex << tgt);
		DEBUG_STMTS

	| pbranch(cc01, tgt) [name] =>
		if (strcmp(name, "BPA") == 0 || strcmp(name, "BPN") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else if (cc01 != 0) {  /* If 64 bit cc used, can't handle */
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return result;
		} else if (strcmp(name, "BPVS") == 0 || strcmp(name, "BPVC") == 0) {
			result.rtl = new RTL(pc, new GotoStatement(tgt));
		} else {
			result.rtl = createBranchRtl(pc, name);
			// The BranchStatement will be the last Stmt of the rtl
			auto jump = (GotoStatement *)result.rtl->getList().back();
			jump->setDest(tgt);
		}

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BPA" or "BPN" (or the pseudo unconditionals BPVx)
		result.type = SCD;
		if (strcmp(name, "BPA") == 0 || strcmp(name, "BPVC") == 0)
			result.type = SD;
		if (strcmp(name, "BPN") == 0 || strcmp(name, "BPVS") == 0)
			result.type = NCT;

		SHOW_ASM(name << " " << std::hex << tgt);
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

	if (result.valid && !result.rtl)
		result.rtl = new RTL(pc);  // FIXME:  Why return an empty RTL?
	result.numBytes = nextPC - pc;
	return result;
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
	| imode(i) =>
		Exp *expr = new Const(i);
		return expr;
	| rmode(rs2) =>
		return dis_RegRhs(rs2);
	endmatch
}

/**
 * Converts a dynamic address to a Exp* expression.
 * E.g. %o7 --> r[ 15 ]
 *
 * \param pc      The instruction stream address of the dynamic address.
 * \param ignore  Redundant parameter on SPARC.
 *
 * \returns  The Exp* representation of the given address.
 */
Exp *
SparcDecoder::dis_Eaddr(ADDRESS pc, const BinaryFile *bf, int ignore /* = 0 */)
{
	Exp *expr;

	match pc to
	| indirectA(rs1) =>
		expr = Location::regOf(rs1);
	| indexA(rs1, rs2) =>
		expr = new Binary(opPlus,
		                  Location::regOf(rs1),
		                  Location::regOf(rs2));
	| absoluteA(i) =>
		expr = new Const((int)i);
	| dispA(rs1, i) =>
		expr = new Binary(opPlus,
		                  Location::regOf(rs1),
		                  new Const((int)i));
	endmatch

	return expr;
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
	| RESTORE(_, _, _) =>
	//| RESTORE(a, b, c) =>
		return true;
	else
		return false;
	endmatch
}
