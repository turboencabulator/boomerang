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

/**
 * \todo Don't use macros like this inside matcher arms,
 * since multiple copies may be made.
 */
#define SETS(name, dest, cond) \
	auto bs = new BoolAssign(8); \
	bs->setLeftFromList(result.rtl->getList()); \
	result.rtl->getList().front() = bs; \
	bs->setCondType(cond); \
	SHOW_ASM(name << " " << *dest)

static DecodeResult &genBSFR(ADDRESS pc, Exp *reg, Exp *modrm, int init, int size, OPER incdec, int numBytes);

/**
 * Constructor.  The code won't work without this (not sure why the default
 * constructor won't do...)
 */
PentiumDecoder::PentiumDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get()->getProgPath() + "frontend/machine/pentium/pentium.ssl";
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

	| CALL.Evod(Eaddr) =>
		/*
		 * Register call
		 */
		// Mike: there should probably be a HLNwayCall class for this!
		result.rtl = instantiate(pc, "CALL.Evod", DIS_EADDR32);
		auto newCall = new CallStatement;
		// Record the fact that this is a computed call
		newCall->setIsComputed();
		// Set the destination expression
		newCall->setDest(DIS_EADDR32);
		result.rtl->appendStmt(newCall);

	| JMP.Evod(Eaddr) =>
		/*
		 * Register jump
		 */
		auto newJump = new CaseStatement;
		// Record the fact that this is a computed call
		newJump->setIsComputed();
		// Set the destination expression
		newJump->setDest(DIS_EADDR32);
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
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSG);
	| SETb.LE(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSLE);
	| SETb.NL(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSGE);
	| SETb.L(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSL);
//	| SETb.NP(Eaddr) [name] =>
//		result.rtl = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG);
//	| SETb.P(Eaddr) [name] =>
//		result.rtl = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG);
	| SETb.NS(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JPOS);
	| SETb.S(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JMI);
	| SETb.NBE(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JUG);
	| SETb.BE(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JULE);
	| SETb.NZ(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JNE);
	| SETb.Z(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JE);
	| SETb.NB(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JUGE);
	| SETb.B(Eaddr) [name] =>
		result.rtl = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JUL);
//	| SETb.NO(Eaddr) [name] =>
//		result.rtl = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG);
//	| SETb.O(Eaddr) [name] =>
//		result.rtl = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG);

	| XLATB() =>
		result.rtl = instantiate(pc, "XLATB");

	| XCHG.Ev.Gvod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XCHG.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| XCHG.Ev.Gvow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XCHG.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| XCHG.Eb.Gb(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XCHG.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| NOP() =>
		result.rtl = instantiate(pc, "NOP");

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

	| XCHGeAXod(r32) =>
		result.rtl = instantiate(pc, "XCHGeAXod", DIS_R32);

	| XCHGeAXow(r32) =>
		result.rtl = instantiate(pc, "XCHGeAXow", DIS_R32);

	| XADD.Ev.Gvod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XADD.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| XADD.Ev.Gvow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XADD.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| XADD.Eb.Gb(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XADD.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| WRMSR() =>
		result.rtl = instantiate(pc, "WRMSR");

	| WBINVD() =>
		result.rtl = instantiate(pc, "WBINVD");

	| WAIT() =>
		result.rtl = instantiate(pc, "WAIT");

	| VERW(Eaddr) =>
		result.rtl = instantiate(pc, "VERW", DIS_EADDR32);

	| VERR(Eaddr) =>
		result.rtl = instantiate(pc, "VERR", DIS_EADDR32);

	| TEST.Ev.Gvod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "TEST.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| TEST.Ev.Gvow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "TEST.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| TEST.Eb.Gb(Eaddr, reg) =>
		result.rtl = instantiate(pc, "TEST.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| TEST.Ed.Id(Eaddr, i32) =>
		result.rtl = instantiate(pc, "TEST.Ed.Id", DIS_EADDR32, DIS_I32);

	| TEST.Ew.Iw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "TEST.Ew.Iw", DIS_EADDR16, DIS_I16);

	| TEST.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "TEST.Eb.Ib", DIS_EADDR8, DIS_I8);

	| TEST.eAX.Ivod(i32) =>
		result.rtl = instantiate(pc, "TEST.eAX.Ivod", DIS_I32);

	| TEST.eAX.Ivow(i16) =>
		result.rtl = instantiate(pc, "TEST.eAX.Ivow", DIS_I16);

	| TEST.AL.Ib(i8) =>
		result.rtl = instantiate(pc, "TEST.AL.Ib", DIS_I8);

	| STR(Mem) =>
		result.rtl = instantiate(pc, "STR", DIS_MEM);

	| STOSvod() =>
		result.rtl = instantiate(pc, "STOSvod");

	| STOSvow() =>
		result.rtl = instantiate(pc, "STOSvow");

	| STOSB() =>
		result.rtl = instantiate(pc, "STOSB");

	| STI() =>
		result.rtl = instantiate(pc, "STI");

	| STD() =>
		result.rtl = instantiate(pc, "STD");

	| STC() =>
		result.rtl = instantiate(pc, "STC");

	| SMSW(Eaddr) =>
		result.rtl = instantiate(pc, "SMSW", DIS_EADDR32);

	| SLDT(Eaddr) =>
		result.rtl = instantiate(pc, "SLDT", DIS_EADDR32);

	| SHLD.CLod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SHLD.CLod", DIS_EADDR32, DIS_REG32);

	| SHLD.CLow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SHLD.CLow", DIS_EADDR16, DIS_REG16);

	| SHRD.CLod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SHRD.CLod", DIS_EADDR32, DIS_REG32);

	| SHRD.CLow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SHRD.CLow", DIS_EADDR16, DIS_REG16);

	| SHLD.Ibod(Eaddr, reg, count) =>
		result.rtl = instantiate(pc, "SHLD.Ibod", DIS_EADDR32, DIS_REG32, DIS_COUNT);

	| SHLD.Ibow(Eaddr, reg, count) =>
		result.rtl = instantiate(pc, "SHLD.Ibow", DIS_EADDR16, DIS_REG16, DIS_COUNT);

	| SHRD.Ibod(Eaddr, reg, count) =>
		result.rtl = instantiate(pc, "SHRD.Ibod", DIS_EADDR32, DIS_REG32, DIS_COUNT);

	| SHRD.Ibow(Eaddr, reg, count) =>
		result.rtl = instantiate(pc, "SHRD.Ibow", DIS_EADDR16, DIS_REG16, DIS_COUNT);

	| SIDT(Mem) =>
		result.rtl = instantiate(pc, "SIDT", DIS_MEM);

	| SGDT(Mem) =>
		result.rtl = instantiate(pc, "SGDT", DIS_MEM);

	// Sets are now in the high level instructions
	| SCASvod() =>
		result.rtl = instantiate(pc, "SCASvod");

	| SCASvow() =>
		result.rtl = instantiate(pc, "SCASvow");

	| SCASB() =>
		result.rtl = instantiate(pc, "SCASB");

	| SAHF() =>
		result.rtl = instantiate(pc, "SAHF");

	| RSM() =>
		result.rtl = instantiate(pc, "RSM");

	| RET.far.Iw(i16) =>
		result.rtl = instantiate(pc, "RET.far.Iw", DIS_I16);
		result.rtl->appendStmt(new ReturnStatement);

	| RET.Iw(i16) =>
		result.rtl = instantiate(pc, "RET.Iw", DIS_I16);
		result.rtl->appendStmt(new ReturnStatement);

	| RET.far() =>
		result.rtl = instantiate(pc, "RET.far");
		result.rtl->appendStmt(new ReturnStatement);

	| RET() =>
		result.rtl = instantiate(pc, "RET");
		result.rtl->appendStmt(new ReturnStatement);

//	| REPNE() =>
//		result.rtl = instantiate(pc, "REPNE");

//	| REP() =>
//		result.rtl = instantiate(pc, "REP");

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

	| RDMSR() =>
		result.rtl = instantiate(pc, "RDMSR");

	| SARB.Ev.Ibod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SARB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| SARB.Ev.Ibow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SARB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| SHRB.Ev.Ibod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SHRB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| SHRB.Ev.Ibow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SHRB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| SHLSALB.Ev.Ibod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SHLSALB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| SHLSALB.Ev.Ibow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SHLSALB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| RCRB.Ev.Ibod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RCRB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| RCRB.Ev.Ibow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RCRB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| RCLB.Ev.Ibod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RCLB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| RCLB.Ev.Ibow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RCLB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| RORB.Ev.Ibod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RORB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| RORB.Ev.Ibow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RORB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| ROLB.Ev.Ibod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ROLB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| ROLB.Ev.Ibow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ROLB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| SARB.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SARB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| SHRB.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SHRB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| SHLSALB.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SHLSALB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| RCRB.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RCRB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| RCLB.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RCLB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| RORB.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "RORB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| ROLB.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ROLB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| SARB.Ev.CLod(Eaddr) =>
		result.rtl = instantiate(pc, "SARB.Ev.CLod", DIS_EADDR32);

	| SARB.Ev.CLow(Eaddr) =>
		result.rtl = instantiate(pc, "SARB.Ev.CLow", DIS_EADDR16);

	| SARB.Ev.1od(Eaddr) =>
		result.rtl = instantiate(pc, "SARB.Ev.1od", DIS_EADDR32);

	| SARB.Ev.1ow(Eaddr) =>
		result.rtl = instantiate(pc, "SARB.Ev.1ow", DIS_EADDR16);

	| SHRB.Ev.CLod(Eaddr) =>
		result.rtl = instantiate(pc, "SHRB.Ev.CLod", DIS_EADDR32);

	| SHRB.Ev.CLow(Eaddr) =>
		result.rtl = instantiate(pc, "SHRB.Ev.CLow", DIS_EADDR16);

	| SHRB.Ev.1od(Eaddr) =>
		result.rtl = instantiate(pc, "SHRB.Ev.1od", DIS_EADDR32);

	| SHRB.Ev.1ow(Eaddr) =>
		result.rtl = instantiate(pc, "SHRB.Ev.1ow", DIS_EADDR16);

	| SHLSALB.Ev.CLod(Eaddr) =>
		result.rtl = instantiate(pc, "SHLSALB.Ev.CLod", DIS_EADDR32);

	| SHLSALB.Ev.CLow(Eaddr) =>
		result.rtl = instantiate(pc, "SHLSALB.Ev.CLow", DIS_EADDR16);

	| SHLSALB.Ev.1od(Eaddr) =>
		result.rtl = instantiate(pc, "SHLSALB.Ev.1od", DIS_EADDR32);

	| SHLSALB.Ev.1ow(Eaddr) =>
		result.rtl = instantiate(pc, "SHLSALB.Ev.1ow", DIS_EADDR16);

	| RCRB.Ev.CLod(Eaddr) =>
		result.rtl = instantiate(pc, "RCRB.Ev.CLod", DIS_EADDR32);

	| RCRB.Ev.CLow(Eaddr) =>
		result.rtl = instantiate(pc, "RCRB.Ev.CLow", DIS_EADDR16);

	| RCRB.Ev.1od(Eaddr) =>
		result.rtl = instantiate(pc, "RCRB.Ev.1od", DIS_EADDR32);

	| RCRB.Ev.1ow(Eaddr) =>
		result.rtl = instantiate(pc, "RCRB.Ev.1ow", DIS_EADDR16);

	| RCLB.Ev.CLod(Eaddr) =>
		result.rtl = instantiate(pc, "RCLB.Ev.CLod", DIS_EADDR32);

	| RCLB.Ev.CLow(Eaddr) =>
		result.rtl = instantiate(pc, "RCLB.Ev.CLow", DIS_EADDR16);

	| RCLB.Ev.1od(Eaddr) =>
		result.rtl = instantiate(pc, "RCLB.Ev.1od", DIS_EADDR32);

	| RCLB.Ev.1ow(Eaddr) =>
		result.rtl = instantiate(pc, "RCLB.Ev.1ow", DIS_EADDR16);

	| RORB.Ev.CLod(Eaddr) =>
		result.rtl = instantiate(pc, "RORB.Ev.CLod", DIS_EADDR32);

	| RORB.Ev.CLow(Eaddr) =>
		result.rtl = instantiate(pc, "RORB.Ev.CLow", DIS_EADDR16);

	| RORB.Ev.1od(Eaddr) =>
		result.rtl = instantiate(pc, "RORB.Ev.1od", DIS_EADDR32);

	| RORB.Ev.1ow(Eaddr) =>
		result.rtl = instantiate(pc, "ORB.Ev.1owR", DIS_EADDR16);

	| ROLB.Ev.CLod(Eaddr) =>
		result.rtl = instantiate(pc, "ROLB.Ev.CLod", DIS_EADDR32);

	| ROLB.Ev.CLow(Eaddr) =>
		result.rtl = instantiate(pc, "ROLB.Ev.CLow", DIS_EADDR16);

	| ROLB.Ev.1od(Eaddr) =>
		result.rtl = instantiate(pc, "ROLB.Ev.1od", DIS_EADDR32);

	| ROLB.Ev.1ow(Eaddr) =>
		result.rtl = instantiate(pc, "ROLB.Ev.1ow", DIS_EADDR16);

	| SARB.Eb.CL(Eaddr) =>
		result.rtl = instantiate(pc, "SARB.Eb.CL", DIS_EADDR32);

	| SARB.Eb.1(Eaddr) =>
		result.rtl = instantiate(pc, "SARB.Eb.1", DIS_EADDR16);

	| SHRB.Eb.CL(Eaddr) =>
		result.rtl = instantiate(pc, "SHRB.Eb.CL", DIS_EADDR8);

	| SHRB.Eb.1(Eaddr) =>
		result.rtl = instantiate(pc, "SHRB.Eb.1", DIS_EADDR8);

	| SHLSALB.Eb.CL(Eaddr) =>
		result.rtl = instantiate(pc, "SHLSALB.Eb.CL", DIS_EADDR8);

	| SHLSALB.Eb.1(Eaddr) =>
		result.rtl = instantiate(pc, "SHLSALB.Eb.1", DIS_EADDR8);

	| RCRB.Eb.CL(Eaddr) =>
		result.rtl = instantiate(pc, "RCRB.Eb.CL", DIS_EADDR8);

	| RCRB.Eb.1(Eaddr) =>
		result.rtl = instantiate(pc, "RCRB.Eb.1", DIS_EADDR8);

	| RCLB.Eb.CL(Eaddr) =>
		result.rtl = instantiate(pc, "RCLB.Eb.CL", DIS_EADDR8);

	| RCLB.Eb.1(Eaddr) =>
		result.rtl = instantiate(pc, "RCLB.Eb.1", DIS_EADDR8);

	| RORB.Eb.CL(Eaddr) =>
		result.rtl = instantiate(pc, "RORB.Eb.CL", DIS_EADDR8);

	| RORB.Eb.1(Eaddr) =>
		result.rtl = instantiate(pc, "RORB.Eb.1", DIS_EADDR8);

	| ROLB.Eb.CL(Eaddr) =>
		result.rtl = instantiate(pc, "ROLB.Eb.CL", DIS_EADDR8);

	| ROLB.Eb.1(Eaddr) =>
		result.rtl = instantiate(pc, "ROLB.Eb.1", DIS_EADDR8);

	// There is no SSL for these, so don't call instantiate, it will only
	// cause an assert failure. Also, may as well treat these as invalid instr
//	| PUSHFod() =>
//		result.rtl = instantiate(pc, "PUSHFod");

//	| PUSHFow() =>
//		result.rtl = instantiate(pc, "PUSHFow");

//	| PUSHAod() =>
//		result.rtl = instantiate(pc, "PUSHAod");

//	| PUSHAow() =>
//		result.rtl = instantiate(pc, "PUSHAow");

	| PUSH.GS() =>
		result.rtl = instantiate(pc, "PUSH.GS");

	| PUSH.FS() =>
		result.rtl = instantiate(pc, "PUSH.FS");

	| PUSH.ES() =>
		result.rtl = instantiate(pc, "PUSH.ES");

	| PUSH.DS() =>
		result.rtl = instantiate(pc, "PUSH.DS");

	| PUSH.SS() =>
		result.rtl = instantiate(pc, "PUSH.SS");

	| PUSH.CS() =>
		result.rtl = instantiate(pc, "PUSH.CS");

	| PUSH.Ivod(i32) =>
		result.rtl = instantiate(pc, "PUSH.Ivod", DIS_I32);

	| PUSH.Ivow(i16) =>
		result.rtl = instantiate(pc, "PUSH.Ivow", DIS_I16);

	| PUSH.Ixob(i8) =>
		result.rtl = instantiate(pc, "PUSH.Ixob", DIS_I8);

	| PUSH.Ixow(i8) =>
		result.rtl = instantiate(pc, "PUSH.Ixow", DIS_I8);

	| PUSHod(r32) =>
		result.rtl = instantiate(pc, "PUSHod", DIS_R32);

	| PUSHow(r32) =>
		result.rtl = instantiate(pc, "PUSHow", DIS_R32);  // Check!

	| PUSH.Evod(Eaddr) =>
		result.rtl = instantiate(pc, "PUSH.Evod", DIS_EADDR32);

	| PUSH.Evow(Eaddr) =>
		result.rtl = instantiate(pc, "PUSH.Evow", DIS_EADDR16);

//	| POPFod() =>
//		result.rtl = instantiate(pc, "POPFod");

//	| POPFow() =>
//		result.rtl = instantiate(pc, "POPFow");

//	| POPAod() =>
//		result.rtl = instantiate(pc, "POPAod");

//	| POPAow() =>
//		result.rtl = instantiate(pc, "POPAow");

	| POP.GS() =>
		result.rtl = instantiate(pc, "POP.GS");

	| POP.FS() =>
		result.rtl = instantiate(pc, "POP.FS");

	| POP.DS() =>
		result.rtl = instantiate(pc, "POP.DS");

	| POP.SS() =>
		result.rtl = instantiate(pc, "POP.SS");

	| POP.ES() =>
		result.rtl = instantiate(pc, "POP.ES");

	| POPod(r32) =>
		result.rtl = instantiate(pc, "POPod", DIS_R32);

	| POPow(r32) =>
		result.rtl = instantiate(pc, "POPow", DIS_R32);  // Check!

	| POP.Evod(Eaddr) =>
		result.rtl = instantiate(pc, "POP.Evod", DIS_EADDR32);

	| POP.Evow(Eaddr) =>
		result.rtl = instantiate(pc, "POP.Evow", DIS_EADDR16);

//	| OUTSvod() =>
//		result.rtl = instantiate(pc, "OUTSvod");

//	| OUTSvow() =>
//		result.rtl = instantiate(pc, "OUTSvow");

//	| OUTSB() =>
//		result.rtl = instantiate(pc, "OUTSB");

//	| OUT.DX.eAXod() =>
//		result.rtl = instantiate(pc, "OUT.DX.eAXod");

//	| OUT.DX.eAXow() =>
//		result.rtl = instantiate(pc, "OUT.DX.eAXow");

//	| OUT.DX.AL() =>
//		result.rtl = instantiate(pc, "OUT.DX.AL");

//	| OUT.Ib.eAXod(i8) =>
//		result.rtl = instantiate(pc, "OUT.Ib.eAXod", DIS_I8);

//	| OUT.Ib.eAXow(i8) =>
//		result.rtl = instantiate(pc, "OUT.Ib.eAXow", DIS_I8);

//	| OUT.Ib.AL(i8) =>
//		result.rtl = instantiate(pc, "OUT.Ib.AL", DIS_I8);

	| NOTod(Eaddr) =>
		result.rtl = instantiate(pc, "NOTod", DIS_EADDR32);

	| NOTow(Eaddr) =>
		result.rtl = instantiate(pc, "NOTow", DIS_EADDR16);

	| NOTb(Eaddr) =>
		result.rtl = instantiate(pc, "NOTb", DIS_EADDR8);

	| NEGod(Eaddr) =>
		result.rtl = instantiate(pc, "NEGod", DIS_EADDR32);

	| NEGow(Eaddr) =>
		result.rtl = instantiate(pc, "NEGow", DIS_EADDR16);

	| NEGb(Eaddr) =>
		result.rtl = instantiate(pc, "NEGb", DIS_EADDR8);

	| MUL.AXod(Eaddr) =>
		result.rtl = instantiate(pc, "MUL.AXod", DIS_EADDR32);

	| MUL.AXow(Eaddr) =>
		result.rtl = instantiate(pc, "MUL.AXow", DIS_EADDR16);

	| MUL.AL(Eaddr) =>
		result.rtl = instantiate(pc, "MUL.AL", DIS_EADDR8);

	| MOVZX.Gv.Ew(r32, Eaddr) =>
		result.rtl = instantiate(pc, "MOVZX.Gv.Ew", DIS_R32, DIS_EADDR16);

	| MOVZX.Gv.Ebod(r32, Eaddr) =>
		result.rtl = instantiate(pc, "MOVZX.Gv.Ebod", DIS_R32, DIS_EADDR8);

	| MOVZX.Gv.Ebow(r16, Eaddr) =>
		result.rtl = instantiate(pc, "MOVZX.Gv.Ebow", DIS_R16, DIS_EADDR8);

	| MOVSX.Gv.Ew(r32, Eaddr) =>
		result.rtl = instantiate(pc, "MOVSX.Gv.Ew", DIS_R32, DIS_EADDR16);

	| MOVSX.Gv.Ebod(r32, Eaddr) =>
		result.rtl = instantiate(pc, "MOVSX.Gv.Ebod", DIS_R32, DIS_EADDR8);

	| MOVSX.Gv.Ebow(r16, Eaddr) =>
		result.rtl = instantiate(pc, "MOVZX.Gv.Ebow", DIS_R16, DIS_EADDR8);

	| MOVSvod() =>
		result.rtl = instantiate(pc, "MOVSvod");

	| MOVSvow() =>
		result.rtl = instantiate(pc, "MOVSvow");

	| MOVSB() =>
		result.rtl = instantiate(pc, "MOVSB");

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

	| MOV.Ed.Ivod(Eaddr, i32) =>
		result.rtl = instantiate(pc, "MOV.Ed.Ivod", DIS_EADDR32, DIS_I32);

	| MOV.Ew.Ivow(Eaddr, i16) =>
		result.rtl = instantiate(pc, "MOV.Ew.Ivow", DIS_EADDR16, DIS_I16);

	| MOV.Eb.Ib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "MOV.Eb.Ib", DIS_EADDR8, DIS_I8);

	| MOVid(r32, i32) =>
		result.rtl = instantiate(pc, "MOVid", DIS_R32, DIS_I32);

	| MOViw(r16, i16) =>
		result.rtl = instantiate(pc, "MOViw", DIS_R16, DIS_I16);  // Check!

	| MOVib(r8, i8) =>
		result.rtl = instantiate(pc, "MOVib", DIS_R8, DIS_I8);

	| MOV.Ov.eAXod(off) =>
		result.rtl = instantiate(pc, "MOV.Ov.eAXod", DIS_OFF);

	| MOV.Ov.eAXow(off) =>
		result.rtl = instantiate(pc, "MOV.Ov.eAXow", DIS_OFF);

	| MOV.Ob.AL(off) =>
		result.rtl = instantiate(pc, "MOV.Ob.AL", DIS_OFF);

	| MOV.eAX.Ovod(off) =>
		result.rtl = instantiate(pc, "MOV.eAX.Ovod", DIS_OFF);

	| MOV.eAX.Ovow(off) =>
		result.rtl = instantiate(pc, "MOV.eAX.Ovow", DIS_OFF);

	| MOV.AL.Ob(off) =>
		result.rtl = instantiate(pc, "MOV.AL.Ob", DIS_OFF);

//	| MOV.Sw.Ew(Mem, sr16) =>
//		result.rtl = instantiate(pc, "MOV.Sw.Ew", DIS_MEM, DIS_SR16);

//	| MOV.Ew.Sw(Mem, sr16) =>
//		result.rtl = instantiate(pc, "MOV.Ew.Sw", DIS_MEM, DIS_SR16);

	| MOVrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "MOVrmod", DIS_REG32, DIS_EADDR32);

	| MOVrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "MOVrmow", DIS_REG16, DIS_EADDR16);

	| MOVrmb(reg, Eaddr) =>
		result.rtl = instantiate(pc, "MOVrmb", DIS_REG8, DIS_EADDR8);

	| MOVmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "MOVmrod", DIS_EADDR32, DIS_REG32);

	| MOVmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "MOVmrow", DIS_EADDR16, DIS_REG16);

	| MOVmrb(Eaddr, reg) =>
		result.rtl = instantiate(pc, "MOVmrb", DIS_EADDR8, DIS_REG8);

	| LTR(Eaddr) =>
		result.rtl = instantiate(pc, "LTR", DIS_EADDR32);

	| LSS(reg, Mem) =>
		result.rtl = instantiate(pc, "LSS", DIS_REG32, DIS_MEM);

	| LSLod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "LSLod", DIS_REG32, DIS_EADDR32);

	| LSLow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "LSLow", DIS_REG16, DIS_EADDR16);

	| LOOPNE(relocd) =>
		result.rtl = instantiate(pc, "LOOPNE", dis_Num(relocd));  // FIXME:  Replace with a conditional jump

	| LOOPE(relocd) =>
		result.rtl = instantiate(pc, "LOOPE", dis_Num(relocd));  // FIXME:  Replace with a conditional jump

	| LOOP(relocd) =>
		result.rtl = instantiate(pc, "LOOP", dis_Num(relocd));  // FIXME:  Replace with a conditional jump

	| LGS(reg, Mem) =>
		result.rtl = instantiate(pc, "LGS", DIS_REG32, DIS_MEM);

	| LFS(reg, Mem) =>
		result.rtl = instantiate(pc, "LFS", DIS_REG32, DIS_MEM);

	| LES(reg, Mem) =>
		result.rtl = instantiate(pc, "LES", DIS_REG32, DIS_MEM);

	| LEAVE() =>
		result.rtl = instantiate(pc, "LEAVE");

	| LEAod(reg, Mem) =>
		result.rtl = instantiate(pc, "LEA.od", DIS_REG32, DIS_MEM);

	| LEAow(reg, Mem) =>
		result.rtl = instantiate(pc, "LEA.ow", DIS_REG16, DIS_MEM);

	| LDS(reg, Mem) =>
		result.rtl = instantiate(pc, "LDS", DIS_REG32, DIS_MEM);

	| LARod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "LAR.od", DIS_REG32, DIS_EADDR32);

	| LARow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "LAR.ow", DIS_REG16, DIS_EADDR16);

	| LAHF() =>
		result.rtl = instantiate(pc, "LAHF");

	/* Branches have been handled in decodeInstruction() now */
	| IRET() =>
		result.rtl = instantiate(pc, "IRET");

	| INVLPG(Mem) =>
		result.rtl = instantiate(pc, "INVLPG", DIS_MEM);

	| INVD() =>
		result.rtl = instantiate(pc, "INVD");

	| INTO() =>
		result.rtl = instantiate(pc, "INTO");

	| INT.Ib(i8) =>
		result.rtl = instantiate(pc, "INT.Ib", DIS_I8);

// Removing because an invalid instruction is better than trying to
// instantiate this. -trent
//	| INT3() =>
//		result.rtl = instantiate(pc, "INT3");

//	| INSvod() =>
//		result.rtl = instantiate(pc, "INSvod");

//	| INSvow() =>
//		result.rtl = instantiate(pc, "INSvow");

//	| INSB() =>
//		result.rtl = instantiate(pc, "INSB");

	| INCod(r32) =>
		result.rtl = instantiate(pc, "INCod", DIS_R32);

	| INCow(r32) =>
		result.rtl = instantiate(pc, "INCow", DIS_R32);

	| INC.Evod(Eaddr) =>
		result.rtl = instantiate(pc, "INC.Evod", DIS_EADDR32);

	| INC.Evow(Eaddr) =>
		result.rtl = instantiate(pc, "INC.Evow", DIS_EADDR16);

	| INC.Eb(Eaddr) =>
		result.rtl = instantiate(pc, "INC.Eb", DIS_EADDR8);

//	| IN.eAX.DXod() =>
//		result.rtl = instantiate(pc, "IN.eAX.DXod");

//	| IN.eAX.DXow() =>
//		result.rtl = instantiate(pc, "IN.eAX.DXow");

//	| IN.AL.DX() =>
//		result.rtl = instantiate(pc, "IN.AL.DX");

//	| IN.eAX.Ibod(i8) =>
//		result.rtl = instantiate(pc, "IN.eAX.Ibod", DIS_I8);

//	| IN.eAX.Ibow(i8) =>
//		result.rtl = instantiate(pc, "IN.eAX.Ibow", DIS_I8);

//	| IN.AL.Ib(i8) =>
//		result.rtl = instantiate(pc, "IN.AL.Ib", DIS_I8);

	| IMUL.Ivd(reg, Eaddr, i32) =>
		result.rtl = instantiate(pc, "IMUL.Ivd", DIS_REG32, DIS_EADDR32, DIS_I32);

	| IMUL.Ivw(reg, Eaddr, i16) =>
		result.rtl = instantiate(pc, "IMUL.Ivw", DIS_REG16, DIS_EADDR16, DIS_I16);

	| IMUL.Ibod(reg, Eaddr, i8) =>
		result.rtl = instantiate(pc, "IMUL.Ibod", DIS_REG32, DIS_EADDR32, DIS_I8);

	| IMUL.Ibow(reg, Eaddr, i8) =>
		result.rtl = instantiate(pc, "IMUL.Ibow", DIS_REG16, DIS_EADDR16, DIS_I8);

	| IMULrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "IMULrmod", DIS_REG32, DIS_EADDR32);

	| IMULrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "IMULrmow", DIS_REG16, DIS_EADDR16);

	| IMULod(Eaddr) =>
		result.rtl = instantiate(pc, "IMULod", DIS_EADDR32);

	| IMULow(Eaddr) =>
		result.rtl = instantiate(pc, "IMULow", DIS_EADDR16);

	| IMULb(Eaddr) =>
		result.rtl = instantiate(pc, "IMULb", DIS_EADDR8);

	| IDIVeAX(Eaddr) =>
		result.rtl = instantiate(pc, "IDIVeAX", DIS_EADDR32);

	| IDIVAX(Eaddr) =>
		result.rtl = instantiate(pc, "IDIVAX", DIS_EADDR16);

	| IDIV(Eaddr) =>
		result.rtl = instantiate(pc, "IDIV", DIS_EADDR8); /* ?? */

//	| HLT() =>
//		result.rtl = instantiate(pc, "HLT");

	| ENTER(i16, i8) =>
		result.rtl = instantiate(pc, "ENTER", DIS_I16, DIS_I8);

	| DIVeAX(Eaddr) =>
		result.rtl = instantiate(pc, "DIVeAX", DIS_EADDR32);

	| DIVAX(Eaddr) =>
		result.rtl = instantiate(pc, "DIVAX", DIS_EADDR16);

	| DIVAL(Eaddr) =>
		result.rtl = instantiate(pc, "DIVAL", DIS_EADDR8);

	| DECod(r32) =>
		result.rtl = instantiate(pc, "DECod", DIS_R32);

	| DECow(r32) =>
		result.rtl = instantiate(pc, "DECow", DIS_R32);

	| DEC.Evod(Eaddr) =>
		result.rtl = instantiate(pc, "DEC.Evod", DIS_EADDR32);

	| DEC.Evow(Eaddr) =>
		result.rtl = instantiate(pc, "DEC.Evow", DIS_EADDR16);

	| DEC.Eb(Eaddr) =>
		result.rtl = instantiate(pc, "DEC.Eb", DIS_EADDR8);

	| DAS() =>
		result.rtl = instantiate(pc, "DAS");

	| DAA() =>
		result.rtl = instantiate(pc, "DAA");

	| CDQ() =>
		result.rtl = instantiate(pc, "CDQ");

	| CWD() =>
		result.rtl = instantiate(pc, "CWD");

	| CPUID() =>
		result.rtl = instantiate(pc, "CPUID");

	| CMPXCHG8B(Mem) =>
		result.rtl = instantiate(pc, "CMPXCHG8B", DIS_MEM);

	| CMPXCHG.Ev.Gvod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "CMPXCHG.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| CMPXCHG.Ev.Gvow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "CMPXCHG.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| CMPXCHG.Eb.Gb(Eaddr, reg) =>
		result.rtl = instantiate(pc, "CMPXCHG.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| CMPSvod() =>
		result.rtl = instantiate(pc, "CMPSvod");

	| CMPSvow() =>
		result.rtl = instantiate(pc, "CMPSvow");

	| CMPSB() =>
		result.rtl = instantiate(pc, "CMPSB");

	| CMC() =>
		result.rtl = instantiate(pc, "CMC");

	| CLTS() =>
		result.rtl = instantiate(pc, "CLTS");

	| CLI() =>
		result.rtl = instantiate(pc, "CLI");

	| CLD() =>
		result.rtl = instantiate(pc, "CLD");

	| CLC() =>
		result.rtl = instantiate(pc, "CLC");

	| CWDE() =>
		result.rtl = instantiate(pc, "CWDE");

	| CBW() =>
		result.rtl = instantiate(pc, "CBW");

	/* Decode the following as a NOP. We see these in startup code, and anywhere
	 * that calls the OS (as lcall 7, 0) */
	| CALL.aPod(_, _) =>
	//| CALL.aPod(seg, off) =>
		result.rtl = instantiate(pc, "NOP");

	| CALL.Jvod(relocd) =>
		result.rtl = instantiate(pc, "CALL.Jvod", dis_Num(relocd));
		if (relocd == nextPC) {
			// This is a call $+5
			// Use the standard semantics, except for the last statement
			// (just updates %pc)
			result.rtl->getList().pop_back();
			// And don't make it a call statement
		} else {
			auto call = new CallStatement;
			// Set the destination
			call->setDest(relocd);
			result.rtl->getList().push_back(call);
			Proc *destProc = prog->setNewProc(relocd);
			if (destProc == (Proc *)-1) destProc = nullptr;  // In case a deleted Proc
			call->setDestProc(destProc);
		}

	| BTSiod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTSiod", DIS_I8, DIS_EADDR32);

	| BTSiow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTSiow", DIS_I8, DIS_EADDR16);

	| BTSod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTSod", DIS_EADDR32, DIS_REG32);

	| BTSow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTSow", DIS_EADDR16, DIS_REG16);

	| BTRiod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTRiod", DIS_EADDR32, DIS_I8);

	| BTRiow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTRiow", DIS_EADDR16, DIS_I8);

	| BTRod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTRod", DIS_EADDR32, DIS_REG32);

	| BTRow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTRow", DIS_EADDR16, DIS_REG16);

	| BTCiod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTCiod", DIS_EADDR32, DIS_I8);

	| BTCiow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTCiow", DIS_EADDR16, DIS_I8);

	| BTCod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTCod", DIS_EADDR32, DIS_REG32);

	| BTCow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTCow", DIS_EADDR16, DIS_REG16);

	| BTiod(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTiod", DIS_EADDR32, DIS_I8);

	| BTiow(Eaddr, i8) =>
		result.rtl = instantiate(pc, "BTiow", DIS_EADDR16, DIS_I8);

	| BTod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTod", DIS_EADDR32, DIS_REG32);

	| BTow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "BTow", DIS_EADDR16, DIS_REG16);

	| BSWAP(r32) =>
		result.rtl = instantiate(pc, "BSWAP", DIS_R32);

	| BSRod(reg, Eaddr) =>
		//result.rtl = instantiate(pc, "BSRod", DIS_REG32, DIS_EADDR32);
		return genBSFR(pc, DIS_REG32, DIS_EADDR32, 32, 32, opMinus, nextPC - pc);

	| BSRow(reg, Eaddr) =>
		//result.rtl = instantiate(pc, "BSRow", DIS_REG16, DIS_EADDR16);
		return genBSFR(pc, DIS_REG16, DIS_EADDR16, 16, 16, opMinus, nextPC - pc);

	| BSFod(reg, Eaddr) =>
		//result.rtl = instantiate(pc, "BSFod", DIS_REG32, DIS_EADDR32);
		return genBSFR(pc, DIS_REG32, DIS_EADDR32, -1, 32, opPlus, nextPC - pc);

	| BSFow(reg, Eaddr) =>
		//result.rtl = instantiate(pc, "BSFow", DIS_REG16, DIS_EADDR16);
		return genBSFR(pc, DIS_REG16, DIS_EADDR16, -1, 16, opPlus, nextPC - pc);

	// Not "user" instructions:
//	| BOUNDod(reg, Mem) =>
//		result.rtl = instantiate(pc, "BOUNDod", DIS_REG32, DIS_MEM);

//	| BOUNDow(reg, Mem) =>
//		result.rtl = instantiate(pc, "BOUNDow", DIS_REG16, DIS_MEM);

//	| ARPL(_, _) =>
//	//| ARPL(Eaddr, reg) =>
//		result.rtl = instantiate(pc, "UNIMP");

//	| AAS() =>
//		result.rtl = instantiate(pc, "AAS");

//	| AAM() =>
//		result.rtl = instantiate(pc, "AAM");

//	| AAD() =>
//		result.rtl = instantiate(pc, "AAD");

//	| AAA() =>
//		result.rtl = instantiate(pc, "AAA");

	| CMPrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "CMPrmod", DIS_REG32, DIS_EADDR32);

	| CMPrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "CMPrmow", DIS_REG16, DIS_EADDR16);

	| XORrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "XORrmod", DIS_REG32, DIS_EADDR32);

	| XORrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "XORrmow", DIS_REG16, DIS_EADDR16);

	| SUBrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "SUBrmod", DIS_REG32, DIS_EADDR32);

	| SUBrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "SUBrmow", DIS_REG16, DIS_EADDR16);

	| ANDrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ANDrmod", DIS_REG32, DIS_EADDR32);

	| ANDrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ANDrmow", DIS_REG16, DIS_EADDR16);

	| SBBrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "SBBrmod", DIS_REG32, DIS_EADDR32);

	| SBBrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "SBBrmow", DIS_REG16, DIS_EADDR16);

	| ADCrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ADCrmod", DIS_REG32, DIS_EADDR32);

	| ADCrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ADCrmow", DIS_REG16, DIS_EADDR16);

	| ORrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ORrmod", DIS_REG32, DIS_EADDR32);

	| ORrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ORrmow", DIS_REG16, DIS_EADDR16);

	| ADDrmod(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ADDrmod", DIS_REG32, DIS_EADDR32);

	| ADDrmow(reg, Eaddr) =>
		result.rtl = instantiate(pc, "ADDrmow", DIS_REG16, DIS_EADDR16);

	| CMPrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "CMPrmb", DIS_R8, DIS_EADDR8);

	| XORrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "XORrmb", DIS_R8, DIS_EADDR8);

	| SUBrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "SUBrmb", DIS_R8, DIS_EADDR8);

	| ANDrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "ANDrmb", DIS_R8, DIS_EADDR8);

	| SBBrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "SBBrmb", DIS_R8, DIS_EADDR8);

	| ADCrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "ADCrmb", DIS_R8, DIS_EADDR8);

	| ORrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "ORrmb", DIS_R8, DIS_EADDR8);

	| ADDrmb(r8, Eaddr) =>
		result.rtl = instantiate(pc, "ADDrmb", DIS_R8, DIS_EADDR8);

	| CMPmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "CMPmrod", DIS_EADDR32, DIS_REG32);

	| CMPmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "CMPmrow", DIS_EADDR16, DIS_REG16);

	| XORmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XORmrod", DIS_EADDR32, DIS_REG32);

	| XORmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "XORmrow", DIS_EADDR16, DIS_REG16);

	| SUBmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SUBmrod", DIS_EADDR32, DIS_REG32);

	| SUBmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SUBmrow", DIS_EADDR16, DIS_REG16);

	| ANDmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ANDmrod", DIS_EADDR32, DIS_REG32);

	| ANDmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ANDmrow", DIS_EADDR16, DIS_REG16);

	| SBBmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SBBmrod", DIS_EADDR32, DIS_REG32);

	| SBBmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "SBBmrow", DIS_EADDR16, DIS_REG16);

	| ADCmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ADCmrod", DIS_EADDR32, DIS_REG32);

	| ADCmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ADCmrow", DIS_EADDR16, DIS_REG16);

	| ORmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ORmrod", DIS_EADDR32, DIS_REG32);

	| ORmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ORmrow", DIS_EADDR16, DIS_REG16);

	| ADDmrod(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ADDmrod", DIS_EADDR32, DIS_REG32);

	| ADDmrow(Eaddr, reg) =>
		result.rtl = instantiate(pc, "ADDmrow", DIS_EADDR16, DIS_REG16);

	| CMPmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "CMPmrb", DIS_EADDR8, DIS_R8);

	| XORmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "XORmrb", DIS_EADDR8, DIS_R8);

	| SUBmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "SUBmrb", DIS_EADDR8, DIS_R8);

	| ANDmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "ANDmrb", DIS_EADDR8, DIS_R8);

	| SBBmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "SBBmrb", DIS_EADDR8, DIS_R8);

	| ADCmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "ADCmrb", DIS_EADDR8, DIS_R8);

	| ORmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "ORmrb", DIS_EADDR8, DIS_R8);

	| ADDmrb(Eaddr, r8) =>
		result.rtl = instantiate(pc, "ADDmrb", DIS_EADDR8, DIS_R8);

	| CMPiodb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "CMPiodb", DIS_EADDR32, DIS_I8);

	| CMPiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "CMPiowb", DIS_EADDR16, DIS_I8);

	| XORiodb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "XORiodb", DIS_EADDR32, DIS_I8);

	| XORiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "XORiowb", DIS_EADDR16, DIS_I8);

	| SUBiodb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SUBiodb", DIS_EADDR32, DIS_I8);

	| SUBiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SUBiowb", DIS_EADDR16, DIS_I8);

	| ANDiodb(Eaddr, i8) =>
		// Special hack to ignore and $0xfffffff0, %esp
		Exp *oper = DIS_EADDR32;
		if (i8 != -16 || !(*oper == *Location::regOf(28)))
			result.rtl = instantiate(pc, "ANDiodb", DIS_EADDR32, DIS_I8);

	| ANDiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ANDiowb", DIS_EADDR16, DIS_I8);

	| SBBiodb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SBBiodb", DIS_EADDR32, DIS_I8);

	| SBBiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SBBiowb", DIS_EADDR16, DIS_I8);

	| ADCiodb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ADCiodb", DIS_EADDR32, DIS_I8);

	| ADCiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ADCiowb", DIS_EADDR16, DIS_I8);

	| ORiodb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ORiodb", DIS_EADDR32, DIS_I8);

	| ORiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ORiowb", DIS_EADDR16, DIS_I8);

	| ADDiodb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ADDiodb", DIS_EADDR32, DIS_I8);

	| ADDiowb(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ADDiowb", DIS_EADDR16, DIS_I8);

	| CMPid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "CMPid", DIS_EADDR32, DIS_I32);

	| XORid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "XORid", DIS_EADDR32, DIS_I32);

	| SUBid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "SUBid", DIS_EADDR32, DIS_I32);

	| ANDid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "ANDid", DIS_EADDR32, DIS_I32);

	| SBBid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "SBBid", DIS_EADDR32, DIS_I32);

	| ADCid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "ADCid", DIS_EADDR32, DIS_I32);

	| ORid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "ORid", DIS_EADDR32, DIS_I32);

	| ADDid(Eaddr, i32) =>
		result.rtl = instantiate(pc, "ADDid", DIS_EADDR32, DIS_I32);

	| CMPiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "CMPiw", DIS_EADDR16, DIS_I16);

	| XORiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "XORiw", DIS_EADDR16, DIS_I16);

	| SUBiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "SUBiw", DIS_EADDR16, DIS_I16);

	| ANDiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "ANDiw", DIS_EADDR16, DIS_I16);

	| SBBiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "SBBiw", DIS_EADDR16, DIS_I16);

	| ADCiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "ADCiw", DIS_EADDR16, DIS_I16);

	| ORiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "ORiw", DIS_EADDR16, DIS_I16);

	| ADDiw(Eaddr, i16) =>
		result.rtl = instantiate(pc, "ADDiw", DIS_EADDR16, DIS_I16);

	| CMPib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "CMPib", DIS_EADDR8, DIS_I8);

	| XORib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "XORib", DIS_EADDR8, DIS_I8);

	| SUBib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SUBib", DIS_EADDR8, DIS_I8);

	| ANDib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ANDib", DIS_EADDR8, DIS_I8);

	| SBBib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "SBBib", DIS_EADDR8, DIS_I8);

	| ADCib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ADCib", DIS_EADDR8, DIS_I8);

	| ORib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ORib", DIS_EADDR8, DIS_I8);

	| ADDib(Eaddr, i8) =>
		result.rtl = instantiate(pc, "ADDib", DIS_EADDR8, DIS_I8);

	| CMPiEAX(i32) =>
		result.rtl = instantiate(pc, "CMPiEAX", DIS_I32);

	| XORiEAX(i32) =>
		result.rtl = instantiate(pc, "XORiEAX", DIS_I32);

	| SUBiEAX(i32) =>
		result.rtl = instantiate(pc, "SUBiEAX", DIS_I32);

	| ANDiEAX(i32) =>
		result.rtl = instantiate(pc, "ANDiEAX", DIS_I32);

	| SBBiEAX(i32) =>
		result.rtl = instantiate(pc, "SBBiEAX", DIS_I32);

	| ADCiEAX(i32) =>
		result.rtl = instantiate(pc, "ADCiEAX", DIS_I32);

	| ORiEAX(i32) =>
		result.rtl = instantiate(pc, "ORiEAX", DIS_I32);

	| ADDiEAX(i32) =>
		result.rtl = instantiate(pc, "ADDiEAX", DIS_I32);

	| CMPiAX(i16) =>
		result.rtl = instantiate(pc, "CMPiAX", DIS_I16);

	| XORiAX(i16) =>
		result.rtl = instantiate(pc, "XORiAX", DIS_I16);

	| SUBiAX(i16) =>
		result.rtl = instantiate(pc, "SUBiAX", DIS_I16);

	| ANDiAX(i16) =>
		result.rtl = instantiate(pc, "ANDiAX", DIS_I16);

	| SBBiAX(i16) =>
		result.rtl = instantiate(pc, "SBBiAX", DIS_I16);

	| ADCiAX(i16) =>
		result.rtl = instantiate(pc, "ADCiAX", DIS_I16);

	| ORiAX(i16) =>
		result.rtl = instantiate(pc, "ORiAX", DIS_I16);

	| ADDiAX(i16) =>
		result.rtl = instantiate(pc, "ADDiAX", DIS_I16);

	| CMPiAL(i8) =>
		result.rtl = instantiate(pc, "CMPiAL", DIS_I8);

	| XORiAL(i8) =>
		result.rtl = instantiate(pc, "XORiAL", DIS_I8);

	| SUBiAL(i8) =>
		result.rtl = instantiate(pc, "SUBiAL", DIS_I8);

	| ANDiAL(i8) =>
		result.rtl = instantiate(pc, "ANDiAL", DIS_I8);

	| SBBiAL(i8) =>
		result.rtl = instantiate(pc, "SBBiAL", DIS_I8);

	| ADCiAL(i8) =>
		result.rtl = instantiate(pc, "ADCiAL", DIS_I8);

	| ORiAL(i8) =>
		result.rtl = instantiate(pc, "ORiAL", DIS_I8);

	| ADDiAL(i8) =>
		result.rtl = instantiate(pc, "ADDiAL", DIS_I8);

	| LODSvod() =>
		result.rtl = instantiate(pc, "LODSvod");

	| LODSvow() =>
		result.rtl = instantiate(pc, "LODSvow");

	| LODSB() =>
		result.rtl = instantiate(pc, "LODSB");

	/* Floating point instructions */
	| F2XM1() =>
		result.rtl = instantiate(pc, "F2XM1");

	| FABS() =>
		result.rtl = instantiate(pc, "FABS");

	| FADD.R32(Mem32) =>
		result.rtl = instantiate(pc, "FADD.R32", DIS_MEM32);

	| FADD.R64(Mem64) =>
		result.rtl = instantiate(pc, "FADD.R64", DIS_MEM64);

	| FADD.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FADD.St.STi", DIS_IDX);

	| FADD.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FADD.STi.ST", DIS_IDX);

	| FADDP.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FADDP.STi.ST", DIS_IDX);

	| FIADD.I32(Mem32) =>
		result.rtl = instantiate(pc, "FIADD.I32", DIS_MEM32);

	| FIADD.I16(Mem16) =>
		result.rtl = instantiate(pc, "FIADD.I16", DIS_MEM16);

	| FBLD(Mem80) =>
		result.rtl = instantiate(pc, "FBLD", DIS_MEM80);

	| FBSTP(Mem80) =>
		result.rtl = instantiate(pc, "FBSTP", DIS_MEM80);

	| FCHS() =>
		result.rtl = instantiate(pc, "FCHS");

	| FNCLEX() =>
		result.rtl = instantiate(pc, "FNCLEX");

	| FCOM.R32(Mem32) =>
		result.rtl = instantiate(pc, "FCOM.R32", DIS_MEM32);

	| FCOM.R64(Mem64) =>
		result.rtl = instantiate(pc, "FCOM.R64", DIS_MEM64);

	| FICOM.I32(Mem32) =>
		result.rtl = instantiate(pc, "FICOM.I32", DIS_MEM32);

	| FICOM.I16(Mem16) =>
		result.rtl = instantiate(pc, "FICOM.I16", DIS_MEM16);

	| FCOMP.R32(Mem32) =>
		result.rtl = instantiate(pc, "FCOMP.R32", DIS_MEM32);

	| FCOMP.R64(Mem64) =>
		result.rtl = instantiate(pc, "FCOMP.R64", DIS_MEM64);

	| FCOM.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FCOM.ST.STi", DIS_IDX);

	| FCOMP.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FCOMP.ST.STi", DIS_IDX);

	| FICOMP.I32(Mem32) =>
		result.rtl = instantiate(pc, "FICOMP.I32", DIS_MEM32);

	| FICOMP.I16(Mem16) =>
		result.rtl = instantiate(pc, "FICOMP.I16", DIS_MEM16);

	| FCOMPP() =>
		result.rtl = instantiate(pc, "FCOMPP");

	| FCOMI.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FCOMIP.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FCOS() =>
		result.rtl = instantiate(pc, "FCOS");

	| FDECSTP() =>
		result.rtl = instantiate(pc, "FDECSTP");

	| FDIV.R32(Mem32) =>
		result.rtl = instantiate(pc, "FDIV.R32", DIS_MEM32);

	| FDIV.R64(Mem64) =>
		result.rtl = instantiate(pc, "FDIV.R64", DIS_MEM64);

	| FDIV.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FDIV.ST.STi", DIS_IDX);

	| FDIV.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FDIV.STi.ST", DIS_IDX);

	| FDIVP.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FDIVP.STi.ST", DIS_IDX);

	| FIDIV.I32(Mem32) =>
		result.rtl = instantiate(pc, "FIDIV.I32", DIS_MEM32);

	| FIDIV.I16(Mem16) =>
		result.rtl = instantiate(pc, "FIDIV.I16", DIS_MEM16);

	| FDIVR.R32(Mem32) =>
		result.rtl = instantiate(pc, "FDIVR.R32", DIS_MEM32);

	| FDIVR.R64(Mem64) =>
		result.rtl = instantiate(pc, "FDIVR.R64", DIS_MEM64);

	| FDIVR.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FDIVR.ST.STi", DIS_IDX);

	| FDIVR.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FDIVR.STi.ST", DIS_IDX);

	| FIDIVR.I32(Mem32) =>
		result.rtl = instantiate(pc, "FIDIVR.I32", DIS_MEM32);

	| FIDIVR.I16(Mem16) =>
		result.rtl = instantiate(pc, "FIDIVR.I16", DIS_MEM16);

	| FDIVRP.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FDIVRP.STi.ST", DIS_IDX);

	| FFREE(idx) =>
		result.rtl = instantiate(pc, "FFREE", DIS_IDX);

	| FILD.lsI16(Mem16) =>
		result.rtl = instantiate(pc, "FILD.lsI16", DIS_MEM16);

	| FILD.lsI32(Mem32) =>
		result.rtl = instantiate(pc, "FILD.lsI32", DIS_MEM32);

	| FILD64(Mem64) =>
		result.rtl = instantiate(pc, "FILD.lsI64", DIS_MEM64);

	| FINIT() =>
		result.rtl = instantiate(pc, "FINIT");

	| FIST.lsI16(Mem16) =>
		result.rtl = instantiate(pc, "FIST.lsI16", DIS_MEM16);

	| FIST.lsI32(Mem32) =>
		result.rtl = instantiate(pc, "FIST.lsI32", DIS_MEM32);

	| FISTP.lsI16(Mem16) =>
		result.rtl = instantiate(pc, "FISTP.lsI16", DIS_MEM16);

	| FISTP.lsI32(Mem32) =>
		result.rtl = instantiate(pc, "FISTP.lsI32", DIS_MEM32);

	| FISTP64(Mem64) =>
		result.rtl = instantiate(pc, "FISTP64", DIS_MEM64);

	| FLD.lsR32(Mem32) =>
		result.rtl = instantiate(pc, "FLD.lsR32", DIS_MEM32);

	| FLD.lsR64(Mem64) =>
		result.rtl = instantiate(pc, "FLD.lsR64", DIS_MEM64);

	| FLD80(Mem80) =>
		result.rtl = instantiate(pc, "FLD80", DIS_MEM80);

/* This is a bit tricky. The FPUSH logically comes between the read of STi and
 * the write to ST0. In particular, FLD ST0 is supposed to duplicate the TOS.
 * This problem only happens with this load instruction, so there is a work
 * around here that gives us the SSL a value of i that is one more than in
 * the instruction */
	| FLD.STi(idx) =>
		result.rtl = instantiate(pc, "FLD.STi", DIS_IDXP1);

	| FLD1() =>
		result.rtl = instantiate(pc, "FLD1");

	| FLDL2T() =>
		result.rtl = instantiate(pc, "FLDL2T");

	| FLDL2E() =>
		result.rtl = instantiate(pc, "FLDL2E");

	| FLDPI() =>
		result.rtl = instantiate(pc, "FLDPI");

	| FLDLG2() =>
		result.rtl = instantiate(pc, "FLDLG2");

	| FLDLN2() =>
		result.rtl = instantiate(pc, "FLDLN2");

	| FLDZ() =>
		result.rtl = instantiate(pc, "FLDZ");

	| FLDCW(Mem16) =>
		result.rtl = instantiate(pc, "FLDCW", DIS_MEM16);

	| FLDENV(Mem) =>
		result.rtl = instantiate(pc, "FLDENV", DIS_MEM);

	| FMUL.R32(Mem32) =>
		result.rtl = instantiate(pc, "FMUL.R32", DIS_MEM32);

	| FMUL.R64(Mem64) =>
		result.rtl = instantiate(pc, "FMUL.R64", DIS_MEM64);

	| FMUL.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FMUL.ST.STi", DIS_IDX);

	| FMUL.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FMUL.STi.ST", DIS_IDX);

	| FMULP.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FMULP.STi.ST", DIS_IDX);

	| FIMUL.I32(Mem32) =>
		result.rtl = instantiate(pc, "FIMUL.I32", DIS_MEM32);

	| FIMUL.I16(Mem16) =>
		result.rtl = instantiate(pc, "FIMUL.I16", DIS_MEM16);

	| FNOP() =>
		result.rtl = instantiate(pc, "FNOP");

	| FPATAN() =>
		result.rtl = instantiate(pc, "FPATAN");

	| FPREM() =>
		result.rtl = instantiate(pc, "FPREM");

	| FPREM1() =>
		result.rtl = instantiate(pc, "FPREM1");

	| FPTAN() =>
		result.rtl = instantiate(pc, "FPTAN");

	| FRNDINT() =>
		result.rtl = instantiate(pc, "FRNDINT");

	| FRSTOR(Mem) =>
		result.rtl = instantiate(pc, "FRSTOR", DIS_MEM);

	| FNSAVE(Mem) =>
		result.rtl = instantiate(pc, "FNSAVE", DIS_MEM);

	| FSCALE() =>
		result.rtl = instantiate(pc, "FSCALE");

	| FSIN() =>
		result.rtl = instantiate(pc, "FSIN");

	| FSINCOS() =>
		result.rtl = instantiate(pc, "FSINCOS");

	| FSQRT() =>
		result.rtl = instantiate(pc, "FSQRT");

	| FST.lsR32(Mem32) =>
		result.rtl = instantiate(pc, "FST.lsR32", DIS_MEM32);

	| FST.lsR64(Mem64) =>
		result.rtl = instantiate(pc, "FST.lsR64", DIS_MEM64);

	| FSTP.lsR32(Mem32) =>
		result.rtl = instantiate(pc, "FSTP.lsR32", DIS_MEM32);

	| FSTP.lsR64(Mem64) =>
		result.rtl = instantiate(pc, "FSTP.lsR64", DIS_MEM64);

	| FSTP80(Mem80) =>
		result.rtl = instantiate(pc, "FSTP80", DIS_MEM80);

	| FST.st.STi(idx) =>
		result.rtl = instantiate(pc, "FST.st.STi", DIS_IDX);

	| FSTP.st.STi(idx) =>
		result.rtl = instantiate(pc, "FSTP.st.STi", DIS_IDX);

	| FSTCW(Mem16) =>
		result.rtl = instantiate(pc, "FSTCW", DIS_MEM16);

	| FSTENV(Mem) =>
		result.rtl = instantiate(pc, "FSTENV", DIS_MEM);

	| FSTSW(Mem16) =>
		result.rtl = instantiate(pc, "FSTSW", DIS_MEM16);

	| FSTSW.AX() =>
		result.rtl = instantiate(pc, "FSTSW.AX");

	| FSUB.R32(Mem32) =>
		result.rtl = instantiate(pc, "FSUB.R32", DIS_MEM32);

	| FSUB.R64(Mem64) =>
		result.rtl = instantiate(pc, "FSUB.R64", DIS_MEM64);

	| FSUB.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FSUB.ST.STi", DIS_IDX);

	| FSUB.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FSUB.STi.ST", DIS_IDX);

	| FISUB.I32(Mem32) =>
		result.rtl = instantiate(pc, "FISUB.I32", DIS_MEM32);

	| FISUB.I16(Mem16) =>
		result.rtl = instantiate(pc, "FISUB.I16", DIS_MEM16);

	| FSUBP.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FSUBP.STi.ST", DIS_IDX);

	| FSUBR.R32(Mem32) =>
		result.rtl = instantiate(pc, "FSUBR.R32", DIS_MEM32);

	| FSUBR.R64(Mem64) =>
		result.rtl = instantiate(pc, "FSUBR.R64", DIS_MEM64);

	| FSUBR.ST.STi(idx) =>
		result.rtl = instantiate(pc, "FSUBR.ST.STi", DIS_IDX);

	| FSUBR.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FSUBR.STi.ST", DIS_IDX);

	| FISUBR.I32(Mem32) =>
		result.rtl = instantiate(pc, "FISUBR.I32", DIS_MEM32);

	| FISUBR.I16(Mem16) =>
		result.rtl = instantiate(pc, "FISUBR.I16", DIS_MEM16);

	| FSUBRP.STi.ST(idx) =>
		result.rtl = instantiate(pc, "FSUBRP.STi.ST", DIS_IDX);

	| FTST() =>
		result.rtl = instantiate(pc, "FTST");

	| FUCOM(idx) =>
		result.rtl = instantiate(pc, "FUCOM", DIS_IDX);

	| FUCOMP(idx) =>
		result.rtl = instantiate(pc, "FUCOMP", DIS_IDX);

	| FUCOMPP() =>
		result.rtl = instantiate(pc, "FUCOMPP");

	| FUCOMI.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FUCOMIP.ST.STi(idx) [name] =>
		result.rtl = instantiate(pc, name, DIS_IDX);

	| FXAM() =>
		result.rtl = instantiate(pc, "FXAM");

	| FXCH(idx) =>
		result.rtl = instantiate(pc, "FXCH", DIS_IDX);

	| FXTRACT() =>
		result.rtl = instantiate(pc, "FXTRACT");

	| FYL2X() =>
		result.rtl = instantiate(pc, "FYL2X");

	| FYL2XP1() =>
		result.rtl = instantiate(pc, "FYL2XP1");

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
		b = new BranchStatement;
		b->setDest(pc + numBytes);
		b->setCondType(BRANCH_JE);
		b->setCondExpr(new Binary(opEquals,
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
		b = new BranchStatement;
		b->setDest(pc + 2);
		b->setCondType(BRANCH_JE);
		b->setCondExpr(new Binary(opEquals,
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
	result.rtl->append(stmts);
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
