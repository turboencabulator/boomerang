/**
 * \file
 * \brief Contains the high level decoding functionality, for example matching
 *        logues, calls, branches, etc.  Ordinary instructions are processed
 *        in decoder_low.m
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pentiumdecoder.h"

#include "boomerang.h"
#include "exp.h"
#include "prog.h"
#include "rtl.h"
#include "statement.h"

#include <cassert>

class Proc;

#define DIS_R8    (dis_Reg(r8  +  8))
#define DIS_R16   (dis_Reg(r16 +  0))
#define DIS_R32   (dis_Reg(r32 + 24))
#define DIS_REG8  (dis_Reg(reg +  8))
#define DIS_REG16 (dis_Reg(reg +  0))
#define DIS_REG32 (dis_Reg(reg + 24))
#define DIS_SR16  (dis_Reg(sr16 + 16))
#define DIS_IDX   (dis_Reg(idx + 32))
#define DIS_IDXP1 (dis_Reg((idx + 1) % 7 + 32))

#define DIS_EADDR32 (dis_Eaddr(Eaddr, bf, 32))
#define DIS_EADDR16 (dis_Eaddr(Eaddr, bf, 16))
#define DIS_EADDR8  (dis_Eaddr(Eaddr, bf,  8))
#define DIS_MEM     (dis_Mem(Mem, bf))
#define DIS_MEM16   (dis_Mem(Mem16, bf))    // Probably needs changing
#define DIS_MEM32   (dis_Mem(Mem32, bf))    // Probably needs changing
#define DIS_MEM64   (dis_Mem(Mem64, bf))    // Probably needs changing
#define DIS_MEM80   (dis_Mem(Mem80, bf))    // Probably needs changing

#define DIS_I32     (addReloc(new Const(i32)))
#define DIS_I16     (new Const(i16))
#define DIS_I8      (new Const(i8))
#define DIS_COUNT   (new Const(count))
#define DIS_OFF     (addReloc(new Const(off)))

#define addressToPC(pc) (pc)
#define fetch8(pc)  bf->readNative1(pc)
#define fetch16(pc) bf->readNative2(pc)
#define fetch32(pc) (lastDwordLc = pc, bf->readNative4(pc))


static DecodeResult &genBSFR(ADDRESS pc, Exp *reg, Exp *modrm, int init, int size, OPER incdec, int numBytes);

static RTL *
SETS(ADDRESS pc, const std::string &name, Exp *dest, BRANCH_TYPE cond)
{
	auto bs = new BoolAssign();
	bs->setLeft(dest);
	bs->setCondType(cond);
	SHOW_ASM(name << " " << *dest);
	return new RTL(pc, bs);
}

/**
 * Constructor.  The code won't work without this (not sure why the default
 * constructor won't do...)
 */
PentiumDecoder::PentiumDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get().getProgPath() + "frontend/machine/pentium/pentium.ssl";
	RTLDict.readSSLFile(file);
}

#if 0 // Cruft?
// For now...
int
PentiumDecoder::decodeAssemblyInstruction(ADDRESS, ptrdiff_t)
{
	return 0;
}
#endif

static DecodeResult result;

/**
 * Decodes a machine instruction and returns an RTL instance.  In most cases a
 * single instruction is decoded.  However, if a higher level construct that
 * may consist of multiple instructions is matched, then there may be a need
 * to return more than one RTL.  The caller_prologue2 is an example of such a
 * construct which encloses an abritary instruction that must be decoded into
 * its own RTL.
 *
 * \param pc       The native address of the pc.
 * \param delta    The difference between the above address and the host
 *                 address of the pc (i.e. the address that the pc is at in
 *                 the loaded object file).
 * \param RTLDict  The dictionary of RTL templates used to instantiate the RTL
 *                 for the instruction being decoded.
 * \param proc     The enclosing procedure.
 *
 * \returns  A DecodeResult structure containing all the information gathered
 *           during decoding.
 */
DecodeResult &
PentiumDecoder::decodeInstruction(ADDRESS pc, const BinaryFile *bf)
{
	// Clear the result structure;
	result.reset();

	ADDRESS nextPC = NO_ADDRESS;
	match [nextPC] pc to

	| CALL.Evod(Eaddr) [name] =>
		/*
		 * Register call
		 */
		// Mike: there should probably be a HLNwayCall class for this!
		result.rtl = instantiate(pc, name, DIS_EADDR32);
		auto newCall = new CallStatement(DIS_EADDR32);
		// Record the fact that this is a computed call
		newCall->setIsComputed();
		result.rtl->appendStmt(newCall);

	| JMP.Evod(Eaddr) =>
		/*
		 * Register jump
		 */
		auto newJump = new CaseStatement(DIS_EADDR32);
		// Record the fact that this is a computed call
		newJump->setIsComputed();
		result.rtl = new RTL(pc, newJump);

	/*
	 * Unconditional branches
	 */
	| JMP.Jvod(relocd) [name] =>
		result.rtl = unconditionalJump(pc, name, relocd);
	| JMP.Jvow(relocd) [name] =>
		result.rtl = unconditionalJump(pc, name, relocd);
	| JMP.Jb(relocd) [name] =>
		result.rtl = unconditionalJump(pc, name, relocd);

	/*
	 * Conditional branches, 8 bit offset: 7X XX
	 */
	| Jb.NLE(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSG);
	| Jb.LE(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSLE);
	| Jb.NL(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSGE);
	| Jb.L(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSL);
	| Jb.NP(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);
	| Jb.P(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JPAR);
	| Jb.NS(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JPOS);
	| Jb.S(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JMI);
	| Jb.NBE(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUG);
	| Jb.BE(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JULE);
	| Jb.NZ(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JNE);
	| Jb.Z(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JE);
	| Jb.NB(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUGE);
	| Jb.B(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUL);
	| Jb.NO(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);
	| Jb.O(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);

	/*
	 * Conditional branches, 16 bit offset: 66 0F 8X XX XX
	 */
	| Jv.NLEow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSG);
	| Jv.LEow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSLE);
	| Jv.NLow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSGE);
	| Jv.Low(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSL);
	| Jv.NPow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);
	| Jv.Pow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JPAR);
	| Jv.NSow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JPOS);
	| Jv.Sow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JMI);
	| Jv.NBEow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUG);
	| Jv.BEow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JULE);
	| Jv.NZow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JNE);
	| Jv.Zow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JE);
	| Jv.NBow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUGE);
	| Jv.Bow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUL);
	| Jv.NOow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);
	| Jv.Oow(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);

	/*
	 * Conditional branches, 32 bit offset: 0F 8X XX XX XX XX
	 */
	| Jv.NLEod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSG);
	| Jv.LEod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSLE);
	| Jv.NLod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSGE);
	| Jv.Lod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JSL);
	| Jv.NPod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);
	| Jv.Pod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JPAR);
	| Jv.NSod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JPOS);
	| Jv.Sod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JMI);
	| Jv.NBEod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUG);
	| Jv.BEod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JULE);
	| Jv.NZod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JNE);
	| Jv.Zod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JE);
	| Jv.NBod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUGE);
	| Jv.Bod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, BRANCH_JUL);
	| Jv.NOod(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);
	| Jv.Ood(relocd) [name] =>
		result.rtl = conditionalJump(pc, name, relocd, (BRANCH_TYPE)0);

	| SETb.NLE(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSG);
	| SETb.LE(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSLE);
	| SETb.NL(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSGE);
	| SETb.L(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSL);
//	| SETb.NP(Eaddr) [name] =>
//		//result.rtl = instantiate(pc, name, DIS_EADDR8);
//		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSG);
//	| SETb.P(Eaddr) [name] =>
//		//result.rtl = instantiate(pc, name, DIS_EADDR8);
//		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSG);
	| SETb.NS(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JPOS);
	| SETb.S(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JMI);
	| SETb.NBE(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JUG);
	| SETb.BE(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JULE);
	| SETb.NZ(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JNE);
	| SETb.Z(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JE);
	| SETb.NB(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JUGE);
	| SETb.B(Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_EADDR8);
		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JUL);
//	| SETb.NO(Eaddr) [name] =>
//		//result.rtl = instantiate(pc, name, DIS_EADDR8);
//		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSG);
//	| SETb.O(Eaddr) [name] =>
//		//result.rtl = instantiate(pc, name, DIS_EADDR8);
//		result.rtl = SETS(pc, name, DIS_EADDR8, BRANCH_JSG);

	| XLATB() [name] =>
		result.rtl = instantiate(pc, name);

	| XCHG.Ev.Gvod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| XCHG.Ev.Gvow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| XCHG.Eb.Gb(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_REG8);

	| NOP() [name] =>
		result.rtl = instantiate(pc, name);

	| SEG.CS() =>  // For now, treat seg.cs as a 1 byte NOP
		result.rtl = instantiate(pc, "NOP");

	| SEG.DS() =>  // For now, treat seg.ds as a 1 byte NOP
		result.rtl = instantiate(pc, "NOP");

	| SEG.ES() =>  // For now, treat seg.es as a 1 byte NOP
		result.rtl = instantiate(pc, "NOP");

	| SEG.FS() =>  // For now, treat seg.fs as a 1 byte NOP
		result.rtl = instantiate(pc, "NOP");

	| SEG.GS() =>  // For now, treat seg.gs as a 1 byte NOP
		result.rtl = instantiate(pc, "NOP");

	| SEG.SS() =>  // For now, treat seg.ss as a 1 byte NOP
		result.rtl = instantiate(pc, "NOP");

	| XCHGeAXod(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| XCHGeAXow(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| XADD.Ev.Gvod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| XADD.Ev.Gvow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| XADD.Eb.Gb(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_REG8);

	| WRMSR() [name] =>
		result.rtl = instantiate(pc, name);

	| WBINVD() [name] =>
		result.rtl = instantiate(pc, name);

	| WAIT() [name] =>
		result.rtl = instantiate(pc, name);

	| VERW(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| VERR(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| TEST.Ev.Gvod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| TEST.Ev.Gvow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| TEST.Eb.Gb(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_REG8);

	| TEST.Ed.Id(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| TEST.Ew.Iw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| TEST.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| TEST.eAX.Ivod(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| TEST.eAX.Ivow(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| TEST.AL.Ib(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| STR(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| STOSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| STOSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| STOSB() [name] =>
		result.rtl = instantiate(pc, name);

	| STI() [name] =>
		result.rtl = instantiate(pc, name);

	| STD() [name] =>
		result.rtl = instantiate(pc, name);

	| STC() [name] =>
		result.rtl = instantiate(pc, name);

	| SMSW(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SLDT(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SHLD.CLod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| SHLD.CLow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| SHRD.CLod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| SHRD.CLow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| SHLD.Ibod(Eaddr, reg, count) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32, DIS_COUNT);

	| SHLD.Ibow(Eaddr, reg, count) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16, DIS_COUNT);

	| SHRD.Ibod(Eaddr, reg, count) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32, DIS_COUNT);

	| SHRD.Ibow(Eaddr, reg, count) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16, DIS_COUNT);

	| SIDT(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| SGDT(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	// Sets are now in the high level instructions
	| SCASvod() [name] =>
		result.rtl = instantiate(pc, name);

	| SCASvow() [name] =>
		result.rtl = instantiate(pc, name);

	| SCASB() [name] =>
		result.rtl = instantiate(pc, name);

	| SAHF() [name] =>
		result.rtl = instantiate(pc, name);

	| RSM() [name] =>
		result.rtl = instantiate(pc, name);

	| RET.far.Iw(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);
		result.rtl->appendStmt(new ReturnStatement);

	| RET.Iw(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);
		result.rtl->appendStmt(new ReturnStatement);

	| RET.far() [name] =>
		result.rtl = instantiate(pc, name);
		result.rtl->appendStmt(new ReturnStatement);

	| RET() [name] =>
		result.rtl = instantiate(pc, name);
		result.rtl->appendStmt(new ReturnStatement);

//	| REPNE() [name] =>
//		result.rtl = instantiate(pc, name);

//	| REP() [name] =>
//		result.rtl = instantiate(pc, name);

	| REP.CMPSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.CMPSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.CMPSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.LODSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.LODSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.LODSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.MOVSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.MOVSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.MOVSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.SCASB() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.SCASvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.SCASvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.STOSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.STOSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REP.STOSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.CMPSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.CMPSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.CMPSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.LODSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.LODSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.LODSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.MOVSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.MOVSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.MOVSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.SCASB() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.SCASvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.SCASvod() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.STOSB() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.STOSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| REPNE.STOSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| RDMSR() [name] =>
		result.rtl = instantiate(pc, name);

	| SARB.Ev.Ibod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| SARB.Ev.Ibow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| SHRB.Ev.Ibod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| SHRB.Ev.Ibow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| SHLSALB.Ev.Ibod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| SHLSALB.Ev.Ibow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| RCRB.Ev.Ibod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| RCRB.Ev.Ibow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| RCLB.Ev.Ibod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| RCLB.Ev.Ibow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| RORB.Ev.Ibod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| RORB.Ev.Ibow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| ROLB.Ev.Ibod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| ROLB.Ev.Ibow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| SARB.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| SHRB.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| SHLSALB.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| RCRB.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| RCLB.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| RORB.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| ROLB.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| SARB.Ev.CLod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SARB.Ev.CLow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| SARB.Ev.1od(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SARB.Ev.1ow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| SHRB.Ev.CLod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SHRB.Ev.CLow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| SHRB.Ev.1od(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SHRB.Ev.1ow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| SHLSALB.Ev.CLod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SHLSALB.Ev.CLow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| SHLSALB.Ev.1od(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SHLSALB.Ev.1ow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| RCRB.Ev.CLod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| RCRB.Ev.CLow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| RCRB.Ev.1od(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| RCRB.Ev.1ow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| RCLB.Ev.CLod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| RCLB.Ev.CLow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| RCLB.Ev.1od(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| RCLB.Ev.1ow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| RORB.Ev.CLod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| RORB.Ev.CLow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| RORB.Ev.1od(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| RORB.Ev.1ow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| ROLB.Ev.CLod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| ROLB.Ev.CLow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| ROLB.Ev.1od(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| ROLB.Ev.1ow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| SARB.Eb.CL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| SARB.Eb.1(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| SHRB.Eb.CL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| SHRB.Eb.1(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| SHLSALB.Eb.CL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| SHLSALB.Eb.1(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| RCRB.Eb.CL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| RCRB.Eb.1(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| RCLB.Eb.CL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| RCLB.Eb.1(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| RORB.Eb.CL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| RORB.Eb.1(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| ROLB.Eb.CL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| ROLB.Eb.1(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	// There is no SSL for these, so don't call instantiate, it will only
	// cause an assert failure. Also, may as well treat these as invalid instr
//	| PUSHFod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| PUSHFow() [name] =>
//		result.rtl = instantiate(pc, name);

//	| PUSHAod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| PUSHAow() [name] =>
//		result.rtl = instantiate(pc, name);

	| PUSH.GS() [name] =>
		result.rtl = instantiate(pc, name);

	| PUSH.FS() [name] =>
		result.rtl = instantiate(pc, name);

	| PUSH.ES() [name] =>
		result.rtl = instantiate(pc, name);

	| PUSH.DS() [name] =>
		result.rtl = instantiate(pc, name);

	| PUSH.SS() [name] =>
		result.rtl = instantiate(pc, name);

	| PUSH.CS() [name] =>
		result.rtl = instantiate(pc, name);

	| PUSH.Ivod(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| PUSH.Ivow(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| PUSH.Ixob(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| PUSH.Ixow(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| PUSHod(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| PUSHow(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);  // Check!

	| PUSH.Evod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| PUSH.Evow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

//	| POPFod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| POPFow() [name] =>
//		result.rtl = instantiate(pc, name);

//	| POPAod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| POPAow() [name] =>
//		result.rtl = instantiate(pc, name);

	| POP.GS() [name] =>
		result.rtl = instantiate(pc, name);

	| POP.FS() [name] =>
		result.rtl = instantiate(pc, name);

	| POP.DS() [name] =>
		result.rtl = instantiate(pc, name);

	| POP.SS() [name] =>
		result.rtl = instantiate(pc, name);

	| POP.ES() [name] =>
		result.rtl = instantiate(pc, name);

	| POPod(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| POPow(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);  // Check!

	| POP.Evod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| POP.Evow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

//	| OUTSvod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| OUTSvow() [name] =>
//		result.rtl = instantiate(pc, name);

//	| OUTSB() [name] =>
//		result.rtl = instantiate(pc, name);

//	| OUT.DX.eAXod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| OUT.DX.eAXow() [name] =>
//		result.rtl = instantiate(pc, name);

//	| OUT.DX.AL() [name] =>
//		result.rtl = instantiate(pc, name);

//	| OUT.Ib.eAXod(i8) [name] =>
//		result.rtl = instantiate(pc, name, DIS_I8);

//	| OUT.Ib.eAXow(i8) [name] =>
//		result.rtl = instantiate(pc, name, DIS_I8);

//	| OUT.Ib.AL(i8) [name] =>
//		result.rtl = instantiate(pc, name, DIS_I8);

	| NOTod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| NOTow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| NOTb(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| NEGod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| NEGow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| NEGb(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| MUL.AXod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| MUL.AXow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| MUL.AL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| MOVZX.Gv.Ew(r32, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32, DIS_EADDR16);

	| MOVZX.Gv.Ebod(r32, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32, DIS_EADDR8);

	| MOVZX.Gv.Ebow(r16, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R16, DIS_EADDR8);

	| MOVSX.Gv.Ew(r32, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32, DIS_EADDR16);

	| MOVSX.Gv.Ebod(r32, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32, DIS_EADDR8);

	| MOVSX.Gv.Ebow(r16, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R16, DIS_EADDR8);

	| MOVSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| MOVSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| MOVSB() [name] =>
		result.rtl = instantiate(pc, name);

//	| MOV.Rd.Dd(_, _) =>
//	//| MOV.Rd.Dd(reg, dr) =>
//		result.rtl = instantiate(pc, "UNIMP");

//	| MOV.Dd.Rd(_, _) =>
//	//| MOV.Dd.Rd(dr, reg) =>
//		result.rtl = instantiate(pc, "UNIMP");

//	| MOV.Rd.Cd(_, _) =>
//	//| MOV.Rd.Cd(reg, cr) =>
//		result.rtl = instantiate(pc, "UNIMP");

//	| MOV.Cd.Rd(_, _) =>
//	//| MOV.Cd.Rd(cr, reg) =>
//		result.rtl = instantiate(pc, "UNIMP");

	| MOV.Ed.Ivod(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| MOV.Ew.Ivow(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| MOV.Eb.Ib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| MOVid(r32, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32, DIS_I32);

	| MOViw(r16, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_R16, DIS_I16);  // Check!

	| MOVib(r8, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_I8);

	| MOV.Ov.eAXod(off) [name] =>
		result.rtl = instantiate(pc, name, DIS_OFF);

	| MOV.Ov.eAXow(off) [name] =>
		result.rtl = instantiate(pc, name, DIS_OFF);

	| MOV.Ob.AL(off) [name] =>
		result.rtl = instantiate(pc, name, DIS_OFF);

	| MOV.eAX.Ovod(off) [name] =>
		result.rtl = instantiate(pc, name, DIS_OFF);

	| MOV.eAX.Ovow(off) [name] =>
		result.rtl = instantiate(pc, name, DIS_OFF);

	| MOV.AL.Ob(off) [name] =>
		result.rtl = instantiate(pc, name, DIS_OFF);

//	| MOV.Sw.Ew(Mem, sr16) [name] =>
//		result.rtl = instantiate(pc, name, DIS_MEM, DIS_SR16);

//	| MOV.Ew.Sw(Mem, sr16) [name] =>
//		result.rtl = instantiate(pc, name, DIS_MEM, DIS_SR16);

	| MOVrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| MOVrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| MOVrmb(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG8, DIS_EADDR8);

	| MOVmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| MOVmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| MOVmrb(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_REG8);

	| LTR(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| LSS(reg, Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_MEM);

	| LSLod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| LSLow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| LOOPNE(relocd) [name] =>
		result.rtl = instantiate(pc, name, dis_Num(relocd));  // FIXME:  Replace with a conditional jump

	| LOOPE(relocd) [name] =>
		result.rtl = instantiate(pc, name, dis_Num(relocd));  // FIXME:  Replace with a conditional jump

	| LOOP(relocd) [name] =>
		result.rtl = instantiate(pc, name, dis_Num(relocd));  // FIXME:  Replace with a conditional jump

	| LGS(reg, Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_MEM);

	| LFS(reg, Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_MEM);

	| LES(reg, Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_MEM);

	| LEAVE() [name] =>
		result.rtl = instantiate(pc, name);

	| LEAod(reg, Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_MEM);

	| LEAow(reg, Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_MEM);

	| LDS(reg, Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_MEM);

	| LARod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| LARow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| LAHF() [name] =>
		result.rtl = instantiate(pc, name);

	/* Branches have been handled in decodeInstruction() now */
	| IRET() [name] =>
		result.rtl = instantiate(pc, name);

	| INVLPG(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| INVD() [name] =>
		result.rtl = instantiate(pc, name);

	| INTO() [name] =>
		result.rtl = instantiate(pc, name);

	| INT.Ib(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

// Removing because an invalid instruction is better than trying to
// instantiate this. -trent
//	| INT3() [name] =>
//		result.rtl = instantiate(pc, name);

//	| INSvod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| INSvow() [name] =>
//		result.rtl = instantiate(pc, name);

//	| INSB() [name] =>
//		result.rtl = instantiate(pc, name);

	| INCod(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| INCow(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| INC.Evod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| INC.Evow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| INC.Eb(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

//	| IN.eAX.DXod() [name] =>
//		result.rtl = instantiate(pc, name);

//	| IN.eAX.DXow() [name] =>
//		result.rtl = instantiate(pc, name);

//	| IN.AL.DX() [name] =>
//		result.rtl = instantiate(pc, name);

//	| IN.eAX.Ibod(i8) [name] =>
//		result.rtl = instantiate(pc, name, DIS_I8);

//	| IN.eAX.Ibow(i8) [name] =>
//		result.rtl = instantiate(pc, name, DIS_I8);

//	| IN.AL.Ib(i8) [name] =>
//		result.rtl = instantiate(pc, name, DIS_I8);

	| IMUL.Ivd(reg, Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32, DIS_I32);

	| IMUL.Ivw(reg, Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16, DIS_I16);

	| IMUL.Ibod(reg, Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32, DIS_I8);

	| IMUL.Ibow(reg, Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16, DIS_I8);

	| IMULrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| IMULrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| IMULod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| IMULow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| IMULb(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| IDIVeAX(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| IDIVAX(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| IDIV(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8); /* ?? */

//	| HLT() [name] =>
//		result.rtl = instantiate(pc, name);

	| ENTER(i16, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16, DIS_I8);

	| DIVeAX(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| DIVAX(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| DIVAL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| DECod(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| DECow(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| DEC.Evod(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32);

	| DEC.Evow(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16);

	| DEC.Eb(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);

	| DAS() [name] =>
		result.rtl = instantiate(pc, name);

	| DAA() [name] =>
		result.rtl = instantiate(pc, name);

	| CDQ() [name] =>
		result.rtl = instantiate(pc, name);

	| CWD() [name] =>
		result.rtl = instantiate(pc, name);

	| CPUID() [name] =>
		result.rtl = instantiate(pc, name);

	| CMPXCHG8B(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| CMPXCHG.Ev.Gvod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| CMPXCHG.Ev.Gvow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| CMPXCHG.Eb.Gb(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_REG8);

	| CMPSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| CMPSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| CMPSB() [name] =>
		result.rtl = instantiate(pc, name);

	| CMC() [name] =>
		result.rtl = instantiate(pc, name);

	| CLTS() [name] =>
		result.rtl = instantiate(pc, name);

	| CLI() [name] =>
		result.rtl = instantiate(pc, name);

	| CLD() [name] =>
		result.rtl = instantiate(pc, name);

	| CLC() [name] =>
		result.rtl = instantiate(pc, name);

	| CWDE() [name] =>
		result.rtl = instantiate(pc, name);

	| CBW() [name] =>
		result.rtl = instantiate(pc, name);

	/* Decode the following as a NOP. We see these in startup code, and anywhere
	 * that calls the OS (as lcall 7, 0) */
	| CALL.aPod(_, _) =>
	//| CALL.aPod(seg, off) =>
		result.rtl = instantiate(pc, "NOP");

	| CALL.Jvod(relocd) [name] =>
		result.rtl = instantiate(pc, name, dis_Num(relocd));
		if (relocd == nextPC) {
			// This is a call $+5
			// Use the standard semantics, except for the last statement
			// (just updates %pc)
			result.rtl->getList().pop_back();
			// And don't make it a call statement
		} else {
			auto call = new CallStatement(relocd);
			result.rtl->getList().push_back(call);
			Proc *destProc = prog->setNewProc(relocd);
			if (destProc == (Proc *)-1) destProc = nullptr;  // In case a deleted Proc
			call->setDestProc(destProc);
		}

	| BTSiod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8, DIS_EADDR32);

	| BTSiow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8, DIS_EADDR16);

	| BTSod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| BTSow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| BTRiod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| BTRiow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| BTRod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| BTRow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| BTCiod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| BTCiow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| BTCod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| BTCow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| BTiod(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| BTiow(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| BTod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| BTow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| BSWAP(r32) [name] =>
		result.rtl = instantiate(pc, name, DIS_R32);

	| BSRod(reg, Eaddr) =>
	//| BSRod(reg, Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);
		return genBSFR(pc, DIS_REG32, DIS_EADDR32, 32, 32, opMinus, nextPC - pc);

	| BSRow(reg, Eaddr) =>
	//| BSRow(reg, Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);
		return genBSFR(pc, DIS_REG16, DIS_EADDR16, 16, 16, opMinus, nextPC - pc);

	| BSFod(reg, Eaddr) =>
	//| BSFod(reg, Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);
		return genBSFR(pc, DIS_REG32, DIS_EADDR32, -1, 32, opPlus, nextPC - pc);

	| BSFow(reg, Eaddr) =>
	//| BSFow(reg, Eaddr) [name] =>
		//result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);
		return genBSFR(pc, DIS_REG16, DIS_EADDR16, -1, 16, opPlus, nextPC - pc);

	// Not "user" instructions:
//	| BOUNDod(reg, Mem) [name] =>
//		result.rtl = instantiate(pc, name, DIS_REG32, DIS_MEM);

//	| BOUNDow(reg, Mem) [name] =>
//		result.rtl = instantiate(pc, name, DIS_REG16, DIS_MEM);

//	| ARPL(_, _) =>
//	//| ARPL(Eaddr, reg) =>
//		result.rtl = instantiate(pc, "UNIMP");

//	| AAS() [name] =>
//		result.rtl = instantiate(pc, name);

//	| AAM() [name] =>
//		result.rtl = instantiate(pc, name);

//	| AAD() [name] =>
//		result.rtl = instantiate(pc, name);

//	| AAA() [name] =>
//		result.rtl = instantiate(pc, name);

	| CMPrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| CMPrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| XORrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| XORrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| SUBrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| SUBrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| ANDrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| ANDrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| SBBrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| SBBrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| ADCrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| ADCrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| ORrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| ORrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| ADDrmod(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG32, DIS_EADDR32);

	| ADDrmow(reg, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_REG16, DIS_EADDR16);

	| CMPrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| XORrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| SUBrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| ANDrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| SBBrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| ADCrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| ORrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| ADDrmb(r8, Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_R8, DIS_EADDR8);

	| CMPmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| CMPmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| XORmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| XORmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| SUBmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| SUBmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| ANDmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| ANDmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| SBBmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| SBBmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| ADCmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| ADCmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| ORmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| ORmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| ADDmrod(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_REG32);

	| ADDmrow(Eaddr, reg) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_REG16);

	| CMPmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| XORmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| SUBmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| ANDmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| SBBmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| ADCmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| ORmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| ADDmrb(Eaddr, r8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_R8);

	| CMPiodb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| CMPiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| XORiodb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| XORiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| SUBiodb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| SUBiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| ANDiodb(Eaddr, i8) [name] =>
		// Special hack to ignore and $0xfffffff0, %esp
		auto oper = DIS_EADDR32;
		if (!(i8 == -16 && oper->isRegN(28)))
			result.rtl = instantiate(pc, name, oper, DIS_I8);

	| ANDiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| SBBiodb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| SBBiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| ADCiodb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| ADCiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| ORiodb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| ORiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| ADDiodb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I8);

	| ADDiowb(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I8);

	| CMPid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| XORid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| SUBid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| ANDid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| SBBid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| ADCid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| ORid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| ADDid(Eaddr, i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR32, DIS_I32);

	| CMPiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| XORiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| SUBiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| ANDiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| SBBiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| ADCiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| ORiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| ADDiw(Eaddr, i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR16, DIS_I16);

	| CMPib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| XORib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| SUBib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| ANDib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| SBBib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| ADCib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| ORib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| ADDib(Eaddr, i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8, DIS_I8);

	| CMPiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| XORiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| SUBiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| ANDiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| SBBiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| ADCiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| ORiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| ADDiEAX(i32) [name] =>
		result.rtl = instantiate(pc, name, DIS_I32);

	| CMPiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| XORiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| SUBiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| ANDiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| SBBiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| ADCiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| ORiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| ADDiAX(i16) [name] =>
		result.rtl = instantiate(pc, name, DIS_I16);

	| CMPiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| XORiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| SUBiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| ANDiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| SBBiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| ADCiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| ORiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| ADDiAL(i8) [name] =>
		result.rtl = instantiate(pc, name, DIS_I8);

	| LODSvod() [name] =>
		result.rtl = instantiate(pc, name);

	| LODSvow() [name] =>
		result.rtl = instantiate(pc, name);

	| LODSB() [name] =>
		result.rtl = instantiate(pc, name);

	/* Floating point instructions */
	| F2XM1() [name] =>
		result.rtl = instantiate(pc, name);

	| FABS() [name] =>
		result.rtl = instantiate(pc, name);

	| FADD.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FADD.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FADD.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FADD.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FADDP.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FIADD.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FIADD.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FBLD(Mem80) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM80);

	| FBSTP(Mem80) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM80);

	| FCHS() [name] =>
		result.rtl = instantiate(pc, name);

	| FNCLEX() [name] =>
		result.rtl = instantiate(pc, name);

	| FCOM.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FCOM.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FICOM.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FICOM.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FCOMP.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FCOMP.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FCOM.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FCOMP.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FICOMP.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FICOMP.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FCOMPP() [name] =>
		result.rtl = instantiate(pc, name);

	| FCOMI.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FCOMIP.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FCOS() [name] =>
		result.rtl = instantiate(pc, name);

	| FDECSTP() [name] =>
		result.rtl = instantiate(pc, name);

	| FDIV.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FDIV.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FDIV.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FDIV.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FDIVP.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FIDIV.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FIDIV.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FDIVR.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FDIVR.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FDIVR.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FDIVR.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FIDIVR.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FIDIVR.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FDIVRP.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FFREE(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FILD.lsI16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FILD.lsI32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FILD64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FINIT() [name] =>
		result.rtl = instantiate(pc, name);

	| FIST.lsI16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FIST.lsI32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FISTP.lsI16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FISTP.lsI32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FISTP64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FLD.lsR32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FLD.lsR64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FLD80(Mem80) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM80);

/* This is a bit tricky. The FPUSH logically comes between the read of STi and
 * the write to ST0. In particular, FLD ST0 is supposed to duplicate the TOS.
 * This problem only happens with this load instruction, so there is a work
 * around here that gives us the SSL a value of i that is one more than in
 * the instruction */
	| FLD.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDXP1);

	| FLD1() [name] =>
		result.rtl = instantiate(pc, name);

	| FLDL2T() [name] =>
		result.rtl = instantiate(pc, name);

	| FLDL2E() [name] =>
		result.rtl = instantiate(pc, name);

	| FLDPI() [name] =>
		result.rtl = instantiate(pc, name);

	| FLDLG2() [name] =>
		result.rtl = instantiate(pc, name);

	| FLDLN2() [name] =>
		result.rtl = instantiate(pc, name);

	| FLDZ() [name] =>
		result.rtl = instantiate(pc, name);

	| FLDCW(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FLDENV(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| FMUL.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FMUL.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FMUL.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FMUL.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FMULP.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FIMUL.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FIMUL.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FNOP() [name] =>
		result.rtl = instantiate(pc, name);

	| FPATAN() [name] =>
		result.rtl = instantiate(pc, name);

	| FPREM() [name] =>
		result.rtl = instantiate(pc, name);

	| FPREM1() [name] =>
		result.rtl = instantiate(pc, name);

	| FPTAN() [name] =>
		result.rtl = instantiate(pc, name);

	| FRNDINT() [name] =>
		result.rtl = instantiate(pc, name);

	| FRSTOR(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| FNSAVE(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| FSCALE() [name] =>
		result.rtl = instantiate(pc, name);

	| FSIN() [name] =>
		result.rtl = instantiate(pc, name);

	| FSINCOS() [name] =>
		result.rtl = instantiate(pc, name);

	| FSQRT() [name] =>
		result.rtl = instantiate(pc, name);

	| FST.lsR32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FST.lsR64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FSTP.lsR32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FSTP.lsR64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FSTP80(Mem80) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM80);

	| FST.st.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FSTP.st.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FSTCW(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FSTENV(Mem) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM);

	| FSTSW(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FSTSW.AX() [name] =>
		result.rtl = instantiate(pc, name);

	| FSUB.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FSUB.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FSUB.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FSUB.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FISUB.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FISUB.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FSUBP.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FSUBR.R32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FSUBR.R64(Mem64) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM64);

	| FSUBR.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FSUBR.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FISUBR.I32(Mem32) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM32);

	| FISUBR.I16(Mem16) [name] =>
		result.rtl = instantiate(pc, name, DIS_MEM16);

	| FSUBRP.STi.ST(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FTST() [name] =>
		result.rtl = instantiate(pc, name);

	| FUCOM(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FUCOMP(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FUCOMPP() [name] =>
		result.rtl = instantiate(pc, name);

	| FUCOMI.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FUCOMIP.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FXAM() [name] =>
		result.rtl = instantiate(pc, name);

	| FXCH(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FXTRACT() [name] =>
		result.rtl = instantiate(pc, name);

	| FYL2X() [name] =>
		result.rtl = instantiate(pc, name);

	| FYL2XP1() [name] =>
		result.rtl = instantiate(pc, name);

	else
		result.valid = false;
	endmatch

	if (result.valid && !result.rtl)
		result.rtl = new RTL(pc);  // FIXME:  Why return an empty RTL?
	result.numBytes = nextPC - pc;
	return result;
}

/**
 * Converts a dynamic address to a Exp* expression.
 * E.g. [1000] --> m[, 1000
 *
 * \param pc    The address of the Eaddr part of the instr.
 * \param expr  The expression that will be built.
 *
 * \returns  The Exp* representation of the given Eaddr.
 */
Exp *
PentiumDecoder::dis_Mem(ADDRESS pc, const BinaryFile *bf)
{
	Exp *expr = nullptr;
	lastDwordLc = (unsigned)-1;

	match pc to
	| Abs32(a) =>
		// [a]
		expr = Location::memOf(addReloc(new Const(a)));
	| Disp32(d, base) =>
		// m[ r[ base] + d]
		expr = Location::memOf(new Binary(opPlus,
		                                  dis_Reg(24 + base),
		                                  addReloc(new Const(d))));
	| Disp8(d, r32) =>
		// m[ r[ r32] + d]
		expr = Location::memOf(new Binary(opPlus,
		                                  dis_Reg(24 + r32),
		                                  addReloc(new Const(d))));
	| Index(base, index, ss) =>
		// m[ r[base] + r[index] * ss]
		expr = Location::memOf(new Binary(opPlus,
		                                  dis_Reg(24 + base),
		                                  new Binary(opMult,
		                                             dis_Reg(24 + index),
		                                             new Const(1 << ss))));
	| Base(base) =>
		// m[ r[base] ]
		expr = Location::memOf(dis_Reg(24 + base));
	| Index32(d, base, index, ss) =>
		// m[ r[ base ] + r[ index ] * ss + d ]
		expr = Location::memOf(new Binary(opPlus,
		                                  dis_Reg(24 + base),
		                                  new Binary(opPlus,
		                                             new Binary(opMult,
		                                                        dis_Reg(24 + index),
		                                                        new Const(1 << ss)),
		                                             addReloc(new Const(d)))));
	| Base32(d, base) =>
		// m[ r[ base] + d ]
		expr = Location::memOf(new Binary(opPlus,
		                                  dis_Reg(24 + base),
		                                  addReloc(new Const(d))));
	| Index8(d, base, index, ss) =>
		// m[ r[ base ] + r[ index ] * ss + d ]
		expr = Location::memOf(new Binary(opPlus,
		                                  dis_Reg(24 + base),
		                                  new Binary(opPlus,
		                                             new Binary(opMult,
		                                                        dis_Reg(24 + index),
		                                                        new Const(1 << ss)),
		                                             addReloc(new Const(d)))));
	| Base8(d, base) =>
		// m[ r[ base] + d ]
		// Note: d should be sign extended; we do it here manually
		signed char ds8 = d;
		expr = Location::memOf(new Binary(opPlus,
		                                  dis_Reg(24 + base),
		                                  new Const(ds8)));
	| Indir(base) =>
		// m[ r[base] ]
		expr = Location::memOf(dis_Reg(24 + base));
	| ShortIndex(d, index, ss) =>
		// m[ r[index] * ss + d ]
		expr = Location::memOf(new Binary(opPlus,
		                                  new Binary(opMult,
		                                             dis_Reg(24 + index),
		                                             new Const(1 << ss)),
		                                  addReloc(new Const(d))));
	| IndirMem(d) =>
		// [d] (Same as Abs32 using SIB)
		expr = Location::memOf(addReloc(new Const(d)));
	endmatch
	return expr;
}

/**
 * Converts a dynamic address to a Exp* expression.
 * E.g. %ecx --> r[ 25 ]
 *
 * \param pc    The instruction stream address of the dynamic address.
 * \param size  Size of the operand (important if a register).
 *
 * \returns  The Exp* representation of the given Eaddr.
 */
Exp *
PentiumDecoder::dis_Eaddr(ADDRESS pc, const BinaryFile *bf, int size)
{
	match pc to
	| E(Mem) =>
		return DIS_MEM;
	| Reg(reg) =>
		switch (size) {
		case  8: return DIS_REG8;
		case 16: return DIS_REG16;
		default:
		case 32: return DIS_REG32;
		}
	endmatch
}

#if 0 // Cruft?
/**
 * Check to see if the instructions at the given offset match any callee
 * prologue, i.e. does it look like this offset is a pointer to a function?
 *
 * \param hostPC  Pointer to the code in question (native address).
 * \returns       True if a match found.
 */
bool
PentiumDecoder::isFuncPrologue(ADDRESS hostPC)
{
#if 0
	int locals, regs;
	if (InstructionPatterns::frameless_pro(prog.csrSrc, hostPC, locals, regs))
		return true;
	if (InstructionPatterns::struct_ptr(prog.csrSrc, hostPC, locals, regs))
		return true;
	if (InstructionPatterns::std_entry(prog.csrSrc, hostPC, locals, regs))
		return true;
#endif
	return false;
}
#endif

static int BSFRstate = 0;  // State number for this state machine

/**
 * Generates statements for the BSF and BSR series (Bit Scan Forward/Reverse).
 *
 * \param pc        Native PC address (start of the BSF/BSR instruction).
 * \param reg       An expression for the destination register.
 * \param modrm     An expression for the operand being scanned.
 * \param init      Initial value for the dest register.
 * \param size      sizeof(modrm) (in bits).
 * \param incdec    Either opPlus for Forward scans,
 *                  or opMinus for Reverse scans.
 * \param numBytes  Number of bytes this instruction.
 *
 * \returns  true if have to exit early (not in last state).
 */
static DecodeResult &
genBSFR(ADDRESS pc, Exp *dest, Exp *modrm, int init, int size, OPER incdec, int numBytes)
{
	// Note the horrible hack needed here. We need initialisation code, and an extra branch, so the %SKIP/%RPT won't
	// work. We need to emit 6 statements, but these need to be in 3 RTLs, since the destination of a branch has to be
	// to the start of an RTL.  So we use a state machine, and set numBytes to 0 for the first two times. That way, this
	// instruction ends up emitting three RTLs, each with the semantics we need.
	// Note: we don't use pentium.SSL for these.
	// BSFR1:
	//  pc+0:   zf := 1
	//  pc+0:   branch exit condition modrm = 0
	// BSFR2:
	//  pc+1:   zf := 0
	//  pc+1:   dest := init
	// BSFR3:
	//  pc+2: dest := dest op 1
	//  pc+2: branch pc+2 condition modrm@[dest:dest]=0
	// exit:

	auto stmts = std::list<Statement *>();
	Statement *s;
	BranchStatement *b;
	switch (BSFRstate) {
	case 0:
		s = new Assign(new IntegerType(1),
		               new Terminal(opZF),
		               new Const(1));
		stmts.push_back(s);
		b = new BranchStatement(pc + numBytes);
		b->setCondType(BRANCH_JE);
		b->setCondExpr(new Binary(opEqual,
		                          modrm->clone(),
		                          new Const(0)));
		stmts.push_back(b);
		break;
	case 1:
		s = new Assign(new IntegerType(1),
		               new Terminal(opZF),
		               new Const(0));
		stmts.push_back(s);
		s = new Assign(new IntegerType(size),
		               dest->clone(),
		               new Const(init));
		stmts.push_back(s);
		break;
	case 2:
		s = new Assign(new IntegerType(size),
		               dest->clone(),
		               new Binary(incdec,
		                          dest->clone(),
		                          new Const(1)));
		stmts.push_back(s);
		b = new BranchStatement(pc + 2);
		b->setCondType(BRANCH_JE);
		b->setCondExpr(new Binary(opEqual,
		                          new Ternary(opAt,
		                                      modrm->clone(),
		                                      dest->clone(),
		                                      dest->clone()),
		                          new Const(0)));
		stmts.push_back(b);
		break;
	default:
		// Should never happen
		assert(BSFRstate - BSFRstate);
	}
	result.rtl = new RTL(pc + BSFRstate);
	result.rtl->splice(stmts);
	// Keep numBytes == 0 until the last state, so we re-decode this instruction 3 times
	if (BSFRstate != 3 - 1) {
		// Let the number of bytes be 1. This is important at least for setting the fallthrough address for the branch
		// (in the first RTL), which should point to the next RTL
		result.numBytes = 1;
		result.reDecode = true;  // Decode this instruction again
	} else {
		result.numBytes = numBytes;
		result.reDecode = false;
	}
	if (DEBUG_DECODER)
		std::cout << std::hex << pc+BSFRstate << std::dec << ": "
		          << "BS" << (init == -1 ? "F" : "R") << (size == 32 ? ".od" : ".ow")
		          << BSFRstate + 1 << "\n";
	if (++BSFRstate == 3)
		BSFRstate = 0;  // Ready for next time
	return result;
}

Exp *
PentiumDecoder::addReloc(Exp *e)
{
	if (lastDwordLc != (unsigned)-1)
		e = prog->addReloc(e, lastDwordLc);
	return e;
}
