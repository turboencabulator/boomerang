/**
 * \file
 * \brief Implementation of the PPC specific parts of the PPCDecoder class.
 *
 * \authors
 * Copyright (C) 2004, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ppcdecoder.h"

#include "boomerang.h"
#include "exp.h"
#include "prog.h"
#include "rtl.h"
#include "statement.h"

#include <cstring>

class Proc;

Exp *
crBit(int bitNum);  // Get an expression for a CR bit access

#define DIS_UIMM    (new Const(uimm))
#define DIS_SIMM    (new Const(simm))
#define DIS_RS      (dis_Reg(rs))
#define DIS_RD      (dis_Reg(rd))
//#define DIS_CRFD    (dis_Reg(64/* condition registers start*/ + crfd))
#define DIS_CRFD    (new Const(crfd))
#define DIS_RDR     (dis_Reg(rd))
#define DIS_RA      (dis_Reg(ra))
#define DIS_RAZ     (dis_RAmbz(ra))  // As above, but May Be constant Zero
#define DIS_RB      (dis_Reg(rb))
#define DIS_D       (new Const(d))
#define DIS_NZRA    (dis_Reg(ra))
#define DIS_NZRB    (dis_Reg(rb))
#define DIS_RELADDR (new Const(reladdr))
#define DIS_CRBD    (crBit(crbD))
#define DIS_CRBA    (crBit(crbA))
#define DIS_CRBB    (crBit(crbB))
#define DIS_DISP    (new Binary(opPlus, DIS_RAZ, DIS_D))
#define DIS_INDEX   (new Binary(opPlus, DIS_RAZ, DIS_RB))
#define DIS_BICR    (new Const(BIcr))
#define DIS_RS_NUM  (new Const(rs))
#define DIS_RD_NUM  (new Const(rd))
#define DIS_SPR_NUM (new Const(spr))
#define DIS_BEG     (new Const(beg))
#define DIS_END     (new Const(end))
#define DIS_FD      (dis_Reg(fd + 32))
#define DIS_FS      (dis_Reg(fs + 32))
#define DIS_FA      (dis_Reg(fa + 32))
#define DIS_FB      (dis_Reg(fb + 32))

#define addressToPC(pc) (pc)
#define fetch32(pc) bf->readNative4(pc)

PPCDecoder::PPCDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get().getProgPath() + "frontend/machine/ppc/ppc.ssl";
	RTLDict.readSSLFile(file);
}

#if 0 // Cruft?
// For now...
int
PPCDecoder::decodeAssemblyInstruction(ADDRESS, ptrdiff_t)
{
	return 0;
}
#endif

void
PPCDecoder::decodeInstruction(DecodeResult &result, ADDRESS pc, const BinaryFile *bf)
{
	// Clear the result structure;
	result.reset();

	ADDRESS nextPC = NO_ADDRESS;
	match [nextPC] pc to
	| XO_(rd, ra, rb) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD, DIS_RA, DIS_RB);
	| XOb_(rd, ra) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD, DIS_RA);
	| Xsax_^Rc(rd, ra) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD, DIS_RA);
	// The number of parameters in these matcher arms has to agree with the number in core.spec
	// The number of parameters passed to instantiate() after pc and name has to agree with ppc.ssl
	// Stores and loads pass rA to instantiate twice: as part of DIS_DISP, and separately as DIS_NZRA
	| Dsad_(rs, d, ra) [name] =>
		if (strcmp(name, "stmw") == 0) {
			// Needs the last param s, which is the register number from rs
			result.rtl = instantiate(pc, name, DIS_RS, DIS_DISP, DIS_RS_NUM);
		} else {
			result.rtl = instantiate(pc, name, DIS_RS, DIS_DISP, DIS_NZRA);
		}

	| Dsaui_(ra, rs, uimm) [name] =>
		result.rtl = instantiate(pc, name, DIS_RA, DIS_RS, DIS_UIMM);
	| Ddasi_(rd, ra, simm) [name] =>
		if (strcmp(name, "addi") == 0 || strcmp(name, "addis") == 0) {
			// Note the DIS_RAZ, since rA could be constant zero
			result.rtl = instantiate(pc, name, DIS_RD, DIS_RAZ, DIS_SIMM);
		} else {
			result.rtl = instantiate(pc, name, DIS_RD, DIS_RA, DIS_SIMM);
		}
	| Xsabx_^Rc(rd, ra, rb) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD, DIS_RA, DIS_RB);
	| Xdab_(rd, ra, rb) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD, DIS_INDEX, DIS_NZRA);
	| Xsab_(rd, ra, rb) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD, DIS_INDEX, DIS_NZRA);
	// Load instructions
	| Ddad_(rd, d, ra) [name] =>
		if (strcmp(name, "lmw") == 0) {
			// Needs the third param d, which is the register number from rd
			result.rtl = instantiate(pc, name, DIS_RD, DIS_DISP, DIS_RD_NUM);
		} else {
			result.rtl = instantiate(pc, name, DIS_RD, DIS_DISP, DIS_NZRA);
		}
//	| XLb_(_, _) [name] =>
//	//| XLb_(b0, b1) [name] =>
#if BCCTR_LONG  // Prefer to see bltctr instead of bcctr 12,0
                // But also affects return instructions (bclr)
		/* FIXME: since this is used for returns, do a jump to LR instead (ie ignoring control registers) */
		result.rtl = instantiate(pc, name);
		result.rtl->appendStmt(new ReturnStatement);
#endif
	| XLc_(crbD, crbA, crbB) [name] =>
		result.rtl = instantiate(pc, name, DIS_CRBD, DIS_CRBA, DIS_CRBB);

	// FIXME: Can't do this, names don't work right for synthetic instructions, and the generated code becomes even more bloated.
	//| mfxer(rd) [name] =>
	//	result.rtl = instantiate(pc, name, DIS_RD);
	//| mflr(rd) [name] =>
	//	result.rtl = instantiate(pc, name, DIS_RD);
	//| mfctr(rd) [name] =>
	//	result.rtl = instantiate(pc, name, DIS_RD);
	//| mtxer(rs) [name] =>
	//	result.rtl = instantiate(pc, name, DIS_RS);
	//| mtlr(rs) [name] =>
	//	result.rtl = instantiate(pc, name, DIS_RS);
	//| mtctr(rs) [name] =>
	//	result.rtl = instantiate(pc, name, DIS_RS);
	| mfspr(rd, spr) [name] =>
		if (spr == 1 || spr == 8 || spr == 9) {
			// User instructions are supported
			result.rtl = instantiate(pc, name, DIS_RD, DIS_SPR_NUM);
		} else {
			// Supervisor instructions (spr@[4] = 1) are not yet supported
			result.valid = false;
		}
	| mtspr(spr, rs) [name] =>
		if (spr == 1 || spr == 8 || spr == 9) {
			// User instructions are supported
			result.rtl = instantiate(pc, name, DIS_SPR_NUM, DIS_RS);
		} else {
			// Supervisor instructions (spr@[4] = 1) are not yet supported
			result.valid = false;
		}

	| Xd_(rd) [name] =>
		result.rtl = instantiate(pc, name, DIS_RD);

	| M_^Rc(ra, rs, uimm, beg, end) [name] =>
		result.rtl = instantiate(pc, name, DIS_RA, DIS_RS, DIS_UIMM, DIS_BEG, DIS_END);


	| bl(reladdr) [name] =>
		auto dest = DIS_RELADDR;
		result.rtl = instantiate(pc, name, dest);
		auto newCall = new CallStatement(dest);
		// Record the fact that this is not a computed call
		newCall->setIsComputed(false);
		result.rtl->appendStmt(newCall);
		Proc *destProc = prog->setNewProc(reladdr);
		if (destProc == (Proc *)-1) destProc = nullptr;
		newCall->setDestProc(destProc);

	| b(reladdr) [name] =>
		result.rtl = unconditionalJump(pc, name, reladdr);

	| ball(BIcr, reladdr) [name] =>  // Always "conditional" branch with link, test/OSX/hello has this
		auto dest = DIS_RELADDR;
		if (reladdr == nextPC) {  // Branch to next instr?
			// Effectively %LR = %pc+4, but give the actual value for %pc
			auto as = new Assign(new IntegerType,
			                     new Unary(opMachFtr, new Const("%LR")),
			                     dest);
			result.rtl = new RTL(pc, as);
			SHOW_ASM(name << " " << BIcr << ", .+4" << " %LR = %pc+4");
		} else {
			result.rtl = instantiate(pc, name, dest);
			auto newCall = new CallStatement(dest);
			// Record the fact that this is not a computed call
			newCall->setIsComputed(false);
			result.rtl->appendStmt(newCall);
		}

	| Xcmp_(crfd, _, ra, rb) [name] =>
	//| Xcmp_(crfd, l, ra, rb) [name] =>
		result.rtl = instantiate(pc, name, DIS_CRFD, DIS_NZRA, DIS_NZRB);
	| cmpi(crfd, _, ra, simm) [name] =>
	//| cmpi(crfd, l, ra, simm) [name] =>
		result.rtl = instantiate(pc, name, DIS_CRFD, DIS_NZRA, DIS_SIMM);
	| cmpli(crfd, _, ra, uimm) [name] =>
	//| cmpli(crfd, l, ra, uimm) [name] =>
		result.rtl = instantiate(pc, name, DIS_CRFD, DIS_NZRA, DIS_UIMM);

	| Ddaf_(fd, d, ra) [name] =>   // Floating point loads (non indexed)
		result.rtl = instantiate(pc, name, DIS_FD, DIS_DISP, DIS_NZRA);   // Pass RA twice (needed for update)

	| Xdaf_(fd, ra, rb) [name] =>  // Floating point loads (indexed)
		result.rtl = instantiate(pc, name, DIS_FD, DIS_INDEX, DIS_NZRA);  // Pass RA twice (needed for update)

	| Dsaf_(fs, d, ra) [name] =>   // Floating point stores (non indexed)
		result.rtl = instantiate(pc, name, DIS_FS, DIS_DISP, DIS_NZRA);   // Pass RA twice (needed for update)

	| Xsaf_(fs, ra, rb) [name] =>  // Floating point stores (indexed)
		result.rtl = instantiate(pc, name, DIS_FS, DIS_INDEX, DIS_NZRA);  // Pass RA twice (needed for update)


	| Xcab_(crfd, fa, fb) [name] =>  // Floating point compare
		result.rtl = instantiate(pc, name, DIS_CRFD, DIS_FA, DIS_FB);

	| Xdbx_^Rc(fd, fb) [name] =>     // Floating point unary
		result.rtl = instantiate(pc, name, DIS_FD, DIS_FB);

	| Ac_^Rc(fd, fa, fb) [name] =>   // Floating point binary
		result.rtl = instantiate(pc, name, DIS_FD, DIS_FA, DIS_FB);




	// Conditional branches
	// bcc_ is blt | ble | beq | bge | bgt | bnl | bne | bng | bso | bns | bun | bnu | bal (branch always)
	| blt(BIcr, reladdr) [name] =>
		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JSL, BIcr);
	| ble(BIcr, reladdr) [name] =>
		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JSLE, BIcr);
	| beq(BIcr, reladdr) [name] =>
		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JE, BIcr);
	| bge(BIcr, reladdr) [name] =>
		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JSGE, BIcr);
	| bgt(BIcr, reladdr) [name] =>
		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JSG, BIcr);
//	| bnl(BIcr, reladdr) [name] =>  // bnl same as bge
//		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JSGE, BIcr);
	| bne(BIcr, reladdr) [name] =>
		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JNE, BIcr);
//	| bng(BIcr, reladdr) [name] =>  // bng same as blt
//		result.rtl = conditionalJump(pc, name, reladdr, BRANCH_JSLE, BIcr);
	| bso(BIcr, reladdr) [name] =>  // Branch on summary overflow
		result.rtl = conditionalJump(pc, name, reladdr, (BRANCH_TYPE)0, BIcr);  // MVE: Don't know these last 4 yet
	| bns(BIcr, reladdr) [name] =>
		result.rtl = conditionalJump(pc, name, reladdr, (BRANCH_TYPE)0, BIcr);
//	| bun(BIcr, reladdr) [name] =>
//		result.rtl = conditionalJump(pc, name, reladdr, (BRANCH_TYPE)0, BIcr);
//	| bnu(BIcr, reladdr) [name] =>
//		result.rtl = conditionalJump(pc, name, reladdr, (BRANCH_TYPE)0, BIcr);

	| balctr(_) [name] =>
	//| balctr(BIcr) [name] =>
		result.rtl = computedJump(pc, name, new Unary(opMachFtr, new Const("%CTR")));

	| balctrl(_) [name] =>
	//| balctrl(BIcr) [name] =>
		result.rtl = computedCall(pc, name, new Unary(opMachFtr, new Const("%CTR")));

	| bal(_, reladdr) [name] =>
	//| bal(BIcr, reladdr) =>
		result.rtl = unconditionalJump(pc, name, reladdr);

	// b<cond>lr: Branch conditionally to the link register. Model this as a conditional branch around a return
	// statement.
	| bltlr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, BRANCH_JSGE, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| blelr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, BRANCH_JSG, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| beqlr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, BRANCH_JNE, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| bgelr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, BRANCH_JSL, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| bgtlr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, BRANCH_JSLE, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| bnelr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, BRANCH_JE, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| bsolr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, (BRANCH_TYPE)0, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| bnslr(BIcr) [name] =>
		result.rtl = conditionalJump(pc, name, nextPC, (BRANCH_TYPE)0, BIcr);
		result.rtl->appendStmt(new ReturnStatement);

	| ballr(_) [name] =>
	//| ballr(BIcr) [name] =>
		result.rtl = new RTL(pc, new ReturnStatement);
		SHOW_ASM(name << "\n");

	// Shift right arithmetic
	| srawi(ra, rs, uimm) [name] =>
		result.rtl = instantiate(pc, name, DIS_RA, DIS_RS, DIS_UIMM);
	| srawiq(ra, rs, uimm) [name] =>
		result.rtl = instantiate(pc, name, DIS_RA, DIS_RS, DIS_UIMM);

	else
		result.valid = false;
	endmatch

	result.numBytes = nextPC - pc;
}

/**
 * Process a conditional jump instruction.
 */
RTL *
PPCDecoder::conditionalJump(ADDRESS pc, const std::string &name, ADDRESS relocd, BRANCH_TYPE cond, unsigned BIcr)
{
	auto jump = new BranchStatement(relocd);
	jump->setCondType(cond);
	SHOW_ASM(name << " " << BIcr << ", 0x" << std::hex << relocd << std::dec);
	return new RTL(pc, jump);
}

/**
 * Decode the register.
 *
 * \param r  Register (0-31).
 * \returns  The expression representing the register.
 */
Exp *
PPCDecoder::dis_Reg(unsigned r)
{
	return Location::regOf(r);
}

/**
 * Decode the register rA when rA represents constant 0 if r == 0.
 *
 * \param r  Register (0-31).
 * \returns  The expression representing the register.
 */
Exp *
PPCDecoder::dis_RAmbz(unsigned r)
{
	if (r == 0)
		return new Const(0);
	return Location::regOf(r);
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
PPCDecoder::isFuncPrologue(ADDRESS hostPC)
{
	return false;
}
#endif

/**
 * Get an expression for a CR bit.
 * For example, if bitNum is 6, return r65@[2:2]
 * (r64 .. r71 are the %cr0 .. %cr7 flag sets)
 */
Exp *
crBit(int bitNum)
{
	int crNum = bitNum / 4;
	bitNum = bitNum & 3;
	return new Ternary(opAt,
	                   Location::regOf(64 + crNum),
	                   new Const(bitNum),
	                   new Const(bitNum));
}
