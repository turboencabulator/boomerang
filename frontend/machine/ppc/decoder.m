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

#include <iostream>

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
#define DIS_DISP    (new Binary(opPlus, dis_RAmbz(ra), new Const(d)))
#define DIS_INDEX   (new Binary(opPlus, DIS_RAZ, DIS_NZRB))
#define DIS_BICR    (new Const(BIcr))
#define DIS_RS_NUM  (new Const(rs))
#define DIS_RD_NUM  (new Const(rd))
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
	std::string file = Boomerang::get()->getProgPath() + "frontend/machine/ppc/ppc.ssl";
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
PPCDecoder::decodeInstruction(ADDRESS pc, const BinaryFile *bf)
{
	static DecodeResult result;

	// Clear the result structure;
	result.reset();

	// The actual list of instantiated statements
	std::list<Statement *> *stmts = nullptr;

	ADDRESS nextPC = NO_ADDRESS;
	match [nextPC] pc to
	| XO_(rd, ra, rb) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_RA, DIS_RB);
	| XOb_(rd, ra) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_RA);
	| Xsax_^Rc(rd, ra) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_RA);
	// The number of parameters in these matcher arms has to agree with the number in core.spec
	// The number of parameters passed to instantiate() after pc and name has to agree with ppc.ssl
	// Stores and loads pass rA to instantiate twice: as part of DIS_DISP, and separately as DIS_NZRA
	| Dsad_(rs, d, ra) [name] =>
		if (strcmp(name, "stmw") == 0) {
			// Needs the last param s, which is the register number from rs
			stmts = instantiate(pc, name, DIS_RS, DIS_DISP, DIS_RS_NUM);
		} else {
			stmts = instantiate(pc, name, DIS_RS, DIS_DISP, DIS_NZRA);
		}

	| Dsaui_(rd, ra, uimm) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_RA, DIS_UIMM);
	| Ddasi_(rd, ra, simm) [name] =>
		if (strcmp(name, "addi") == 0 || strcmp(name, "addis") == 0) {
			// Note the DIS_RAZ, since rA could be constant zero
			stmts = instantiate(pc, name, DIS_RD, DIS_RAZ, DIS_SIMM);
		} else {
			stmts = instantiate(pc, name, DIS_RD, DIS_RA, DIS_SIMM);
		}
	| Xsabx_^Rc(rd, ra, rb) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_RA, DIS_RB);
	| Xdab_(rd, ra, rb) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_INDEX);
	| Xsab_(rd, ra, rb) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_INDEX);
	// Load instructions
	| Ddad_(rd, d, ra) [name] =>
		if (strcmp(name, "lmw") == 0) {
			// Needs the third param d, which is the register number from rd
			stmts = instantiate(pc, name, DIS_RD, DIS_DISP, DIS_RD_NUM);
		} else {
			stmts = instantiate(pc, name, DIS_RD, DIS_DISP, DIS_NZRA);
		}
//	| XLb_(_, _) [name] =>
//	//| XLb_(b0, b1) [name] =>
#if BCCTR_LONG  // Prefer to see bltctr instead of bcctr 12,0
                // But also affects return instructions (bclr)
		/* FIXME: since this is used for returns, do a jump to LR instead (ie ignoring control registers) */
		stmts = instantiate(pc, name);
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(new ReturnStatement);
#endif
	| XLc_(crbD, crbA, crbB) [name] =>
		stmts = instantiate(pc, name, DIS_CRBD, DIS_CRBA, DIS_CRBB);

	| mfspr(rd, uimm) [name] =>
		stmts = instantiate(pc, name, DIS_RD, DIS_UIMM);
	| mtspr(uimm, rs) =>
		switch (uimm) {
		case 1:
			stmts = instantiate(pc, "MTXER", DIS_RS); break;
		case 8:
			stmts = instantiate(pc, "MTLR", DIS_RS); break;
		case 9:
			stmts = instantiate(pc, "MTCTR", DIS_RS); break;
		default:
			std::cerr << "ERROR: MTSPR instruction with invalid S field: " << uimm << "\n";
		}

	| Xd_(rd) [name] =>
		stmts = instantiate(pc, name, DIS_RD);

	| M_^Rc(ra, rs, uimm, beg, end) [name] =>
		stmts = instantiate(pc, name, DIS_RA, DIS_RS, DIS_UIMM, DIS_BEG, DIS_END);


	| bl(reladdr) [name] =>
		Exp *dest = DIS_RELADDR;
		stmts = instantiate(pc, name, dest);
		auto newCall = new CallStatement;
		// Record the fact that this is not a computed call
		newCall->setIsComputed(false);
		// Set the destination expression
		newCall->setDest(dest);
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(newCall);
		Proc *destProc = prog->setNewProc(reladdr);
		if (destProc == (Proc *)-1) destProc = nullptr;
		newCall->setDestProc(destProc);

	| b(reladdr) =>
		unconditionalJump("b", reladdr, pc, stmts, result);

	| ball(BIcr, reladdr) [name] =>  // Always "conditional" branch with link, test/OSX/hello has this
		Exp *dest = DIS_RELADDR;
		if (reladdr == nextPC) {  // Branch to next instr?
			// Effectively %LR = %pc+4, but give the actual value for %pc
			auto as = new Assign(new IntegerType,
			                     new Unary(opMachFtr, new Const("%LR")),
			                     dest);
			stmts = new std::list<Statement *>;
			stmts->push_back(as);
			SHOW_ASM(name << " " << BIcr << ", .+4" << " %LR = %pc+4")
		} else {
			stmts = instantiate(pc, name, dest);
			auto newCall = new CallStatement;
			// Record the fact that this is not a computed call
			newCall->setIsComputed(false);
			// Set the destination expression
			newCall->setDest(dest);
			result.rtl = new RTL(pc, stmts);
			result.rtl->appendStmt(newCall);
		}

	| Xcmp_(crfd, _, ra, rb) [name] =>
	//| Xcmp_(crfd, l, ra, rb) [name] =>
		stmts = instantiate(pc, name, DIS_CRFD, DIS_NZRA, DIS_NZRB);
	| cmpi(crfd, _, ra, simm) [name] =>
	//| cmpi(crfd, l, ra, simm) [name] =>
		stmts = instantiate(pc, name, DIS_CRFD, DIS_NZRA, DIS_SIMM);
	| cmpli(crfd, _, ra, uimm) [name] =>
	//| cmpli(crfd, l, ra, uimm) [name] =>
		stmts = instantiate(pc, name, DIS_CRFD, DIS_NZRA, DIS_UIMM);

	| Ddaf_(fd, d, ra) [name] =>   // Floating point loads (non indexed)
		stmts = instantiate(pc, name, DIS_FD, DIS_DISP, DIS_RA);   // Pass RA twice (needed for update)

	| Xdaf_(fd, ra, rb) [name] =>  // Floating point loads (indexed)
		stmts = instantiate(pc, name, DIS_FD, DIS_INDEX, DIS_RA);  // Pass RA twice (needed for update)

	| Dsaf_(fs, d, ra) [name] =>   // Floating point stores (non indexed)
		stmts = instantiate(pc, name, DIS_FS, DIS_DISP, DIS_RA);   // Pass RA twice (needed for update)

	| Xsaf_(fs, ra, rb) [name] =>  // Floating point stores (indexed)
		stmts = instantiate(pc, name, DIS_FS, DIS_INDEX, DIS_RA);  // Pass RA twice (needed for update)


	| Xcab_(crfd, fa, fb) [name] =>  // Floating point compare
		stmts = instantiate(pc, name, DIS_CRFD, DIS_FA, DIS_FB);

	| Xdbx_^Rc(fd, fb) [name] =>     // Floating point unary
		stmts = instantiate(pc, name, DIS_FD, DIS_FB);

	| Ac_^Rc(fd, fa, fb) [name] =>   // Floating point binary
		stmts = instantiate(pc, name, DIS_FD, DIS_FA, DIS_FB);




	// Conditional branches
	// bcc_ is blt | ble | beq | bge | bgt | bnl | bne | bng | bso | bns | bun | bnu | bal (branch always)
	| blt(BIcr, reladdr) [name] =>
		conditionalJump(name, BRANCH_JSL, BIcr, reladdr, pc, stmts, result);
	| ble(BIcr, reladdr) [name] =>
		conditionalJump(name, BRANCH_JSLE, BIcr, reladdr, pc, stmts, result);
	| beq(BIcr, reladdr) [name] =>
		conditionalJump(name, BRANCH_JE, BIcr, reladdr, pc, stmts, result);
	| bge(BIcr, reladdr) [name] =>
		conditionalJump(name, BRANCH_JSGE, BIcr, reladdr, pc, stmts, result);
	| bgt(BIcr, reladdr) [name] =>
		conditionalJump(name, BRANCH_JSG, BIcr, reladdr, pc, stmts, result);
//	| bnl(BIcr, reladdr) [name] =>  // bnl same as bge
//		conditionalJump(name, BRANCH_JSGE, BIcr, reladdr, pc, stmts, result);
	| bne(BIcr, reladdr) [name] =>
		conditionalJump(name, BRANCH_JNE, BIcr, reladdr, pc, stmts, result);
//	| bng(BIcr, reladdr) [name] =>  // bng same as blt
//		conditionalJump(name, BRANCH_JSLE, BIcr, reladdr, pc, stmts, result);
	| bso(BIcr, reladdr) [name] =>  // Branch on summary overflow
		conditionalJump(name, (BRANCH_TYPE)0, BIcr, reladdr, pc, stmts, result);  // MVE: Don't know these last 4 yet
	| bns(BIcr, reladdr) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, BIcr, reladdr, pc, stmts, result);
//	| bun(BIcr, reladdr) [name] =>
//		conditionalJump(name, (BRANCH_TYPE)0, BIcr, reladdr, pc, stmts, result);
//	| bnu(BIcr, reladdr) [name] =>
//		conditionalJump(name, (BRANCH_TYPE)0, BIcr, reladdr, pc, stmts, result);

	| balctr(_) [name] =>
	//| balctr(BIcr) [name] =>
		computedJump(name, new Unary(opMachFtr, new Const("%CTR")), pc, stmts, result);

	| balctrl(_) [name] =>
	//| balctrl(BIcr) [name] =>
		computedCall(name, new Unary(opMachFtr, new Const("%CTR")), pc, stmts, result);

	| bal(_, reladdr) =>
	//| bal(BIcr, reladdr) =>
		unconditionalJump("bal", reladdr, pc, stmts, result);

	// b<cond>lr: Branch conditionally to the link register. Model this as a conditional branch around a return
	// statement.
	| bltlr(BIcr) [name] =>
		conditionalJump(name, BRANCH_JSGE, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| blelr(BIcr) [name] =>
		conditionalJump(name, BRANCH_JSG, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| beqlr(BIcr) [name] =>
		conditionalJump(name, BRANCH_JNE, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| bgelr(BIcr) [name] =>
		conditionalJump(name, BRANCH_JSL, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| bgtlr(BIcr) [name] =>
		conditionalJump(name, BRANCH_JSLE, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| bnelr(BIcr) [name] =>
		conditionalJump(name, BRANCH_JE, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| bsolr(BIcr) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| bnslr(BIcr) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, BIcr, nextPC, pc, stmts, result);
		result.rtl->appendStmt(new ReturnStatement);

	| ballr(_) [name] =>
	//| ballr(BIcr) [name] =>
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(new ReturnStatement);
		SHOW_ASM(name << "\n");

	// Shift right arithmetic
	| srawi(ra, rs, uimm) [name] =>
		stmts = instantiate(pc, name, DIS_RA, DIS_RS, DIS_UIMM);
	| srawiq(ra, rs, uimm) [name] =>
		stmts = instantiate(pc, name, DIS_RA, DIS_RS, DIS_UIMM);

	else
		stmts = nullptr;
		result.valid = false;
	endmatch

	result.numBytes = nextPC - pc;
	if (result.valid && !result.rtl)  // Don't override higher level res
		result.rtl = new RTL(pc, stmts);

	return result;
}

/**
 * Process a conditional jump instruction.
 */
void
PPCDecoder::conditionalJump(const char *name, BRANCH_TYPE cond, unsigned BIcr, ADDRESS relocd, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result)
{
	result.rtl = new RTL(pc, stmts);
	auto jump = new BranchStatement();
	jump->setDest(relocd);
	jump->setCondType(cond);
	result.rtl->appendStmt(jump);
	SHOW_ASM(name << " " << BIcr << ", 0x" << std::hex << relocd)
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
