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

static void genBSFR(ADDRESS pc, Exp *reg, Exp *modrm, int init, int size, OPER incdec, int numBytes);

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

	// The actual list of instantiated Statements
	std::list<Statement *> *stmts = nullptr;

	ADDRESS nextPC = NO_ADDRESS;
	match [nextPC] pc to

	| CALL.Evod(Eaddr) =>
		/*
		 * Register call
		 */
		// Mike: there should probably be a HLNwayCall class for this!
		stmts = instantiate(pc, "CALL.Evod", DIS_EADDR32);
		auto newCall = new CallStatement;
		// Record the fact that this is a computed call
		newCall->setIsComputed();
		// Set the destination expression
		newCall->setDest(DIS_EADDR32);
		result.rtl = new RTL(pc, stmts);
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
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(newJump);

	/*
	 * Unconditional branches
	 */
	| JMP.Jvod(relocd) [name] =>
		unconditionalJump(name, relocd, pc, stmts, result);
	| JMP.Jvow(relocd) [name] =>
		unconditionalJump(name, relocd, pc, stmts, result);
	| JMP.Jb(relocd) [name] =>
		unconditionalJump(name, relocd, pc, stmts, result);

	/*
	 * Conditional branches, 8 bit offset: 7X XX
	 */
	| Jb.NLE(relocd) [name] =>
		conditionalJump(name, BRANCH_JSG, relocd, pc, stmts, result);
	| Jb.LE(relocd) [name] =>
		conditionalJump(name, BRANCH_JSLE, relocd, pc, stmts, result);
	| Jb.NL(relocd) [name] =>
		conditionalJump(name, BRANCH_JSGE, relocd, pc, stmts, result);
	| Jb.L(relocd) [name] =>
		conditionalJump(name, BRANCH_JSL, relocd, pc, stmts, result);
	| Jb.NP(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);
	| Jb.P(relocd) [name] =>
		conditionalJump(name, BRANCH_JPAR, relocd, pc, stmts, result);
	| Jb.NS(relocd) [name] =>
		conditionalJump(name, BRANCH_JPOS, relocd, pc, stmts, result);
	| Jb.S(relocd) [name] =>
		conditionalJump(name, BRANCH_JMI, relocd, pc, stmts, result);
	| Jb.NBE(relocd) [name] =>
		conditionalJump(name, BRANCH_JUG, relocd, pc, stmts, result);
	| Jb.BE(relocd) [name] =>
		conditionalJump(name, BRANCH_JULE, relocd, pc, stmts, result);
	| Jb.NZ(relocd) [name] =>
		conditionalJump(name, BRANCH_JNE, relocd, pc, stmts, result);
	| Jb.Z(relocd) [name] =>
		conditionalJump(name, BRANCH_JE, relocd, pc, stmts, result);
	| Jb.NB(relocd) [name] =>
		conditionalJump(name, BRANCH_JUGE, relocd, pc, stmts, result);
	| Jb.B(relocd) [name] =>
		conditionalJump(name, BRANCH_JUL, relocd, pc, stmts, result);
	| Jb.NO(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);
	| Jb.O(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);

	/*
	 * Conditional branches, 16 bit offset: 66 0F 8X XX XX
	 */
	| Jv.NLEow(relocd) [name] =>
		conditionalJump(name, BRANCH_JSG, relocd, pc, stmts, result);
	| Jv.LEow(relocd) [name] =>
		conditionalJump(name, BRANCH_JSLE, relocd, pc, stmts, result);
	| Jv.NLow(relocd) [name] =>
		conditionalJump(name, BRANCH_JSGE, relocd, pc, stmts, result);
	| Jv.Low(relocd) [name] =>
		conditionalJump(name, BRANCH_JSL, relocd, pc, stmts, result);
	| Jv.NPow(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);
	| Jv.Pow(relocd) [name] =>
		conditionalJump(name, BRANCH_JPAR, relocd, pc, stmts, result);
	| Jv.NSow(relocd) [name] =>
		conditionalJump(name, BRANCH_JPOS, relocd, pc, stmts, result);
	| Jv.Sow(relocd) [name] =>
		conditionalJump(name, BRANCH_JMI, relocd, pc, stmts, result);
	| Jv.NBEow(relocd) [name] =>
		conditionalJump(name, BRANCH_JUG, relocd, pc, stmts, result);
	| Jv.BEow(relocd) [name] =>
		conditionalJump(name, BRANCH_JULE, relocd, pc, stmts, result);
	| Jv.NZow(relocd) [name] =>
		conditionalJump(name, BRANCH_JNE, relocd, pc, stmts, result);
	| Jv.Zow(relocd) [name] =>
		conditionalJump(name, BRANCH_JE, relocd, pc, stmts, result);
	| Jv.NBow(relocd) [name] =>
		conditionalJump(name, BRANCH_JUGE, relocd, pc, stmts, result);
	| Jv.Bow(relocd) [name] =>
		conditionalJump(name, BRANCH_JUL, relocd, pc, stmts, result);
	| Jv.NOow(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);
	| Jv.Oow(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);

	/*
	 * Conditional branches, 32 bit offset: 0F 8X XX XX XX XX
	 */
	| Jv.NLEod(relocd) [name] =>
		conditionalJump(name, BRANCH_JSG, relocd, pc, stmts, result);
	| Jv.LEod(relocd) [name] =>
		conditionalJump(name, BRANCH_JSLE, relocd, pc, stmts, result);
	| Jv.NLod(relocd) [name] =>
		conditionalJump(name, BRANCH_JSGE, relocd, pc, stmts, result);
	| Jv.Lod(relocd) [name] =>
		conditionalJump(name, BRANCH_JSL, relocd, pc, stmts, result);
	| Jv.NPod(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);
	| Jv.Pod(relocd) [name] =>
		conditionalJump(name, BRANCH_JPAR, relocd, pc, stmts, result);
	| Jv.NSod(relocd) [name] =>
		conditionalJump(name, BRANCH_JPOS, relocd, pc, stmts, result);
	| Jv.Sod(relocd) [name] =>
		conditionalJump(name, BRANCH_JMI, relocd, pc, stmts, result);
	| Jv.NBEod(relocd) [name] =>
		conditionalJump(name, BRANCH_JUG, relocd, pc, stmts, result);
	| Jv.BEod(relocd) [name] =>
		conditionalJump(name, BRANCH_JULE, relocd, pc, stmts, result);
	| Jv.NZod(relocd) [name] =>
		conditionalJump(name, BRANCH_JNE, relocd, pc, stmts, result);
	| Jv.Zod(relocd) [name] =>
		conditionalJump(name, BRANCH_JE, relocd, pc, stmts, result);
	| Jv.NBod(relocd) [name] =>
		conditionalJump(name, BRANCH_JUGE, relocd, pc, stmts, result);
	| Jv.Bod(relocd) [name] =>
		conditionalJump(name, BRANCH_JUL, relocd, pc, stmts, result);
	| Jv.NOod(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);
	| Jv.Ood(relocd) [name] =>
		conditionalJump(name, (BRANCH_TYPE)0, relocd, pc, stmts, result);

	| SETb.NLE(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSG)
	| SETb.LE(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSLE)
	| SETb.NL(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSGE)
	| SETb.L(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JSL)
//	| SETb.NP(Eaddr) [name] =>
//		stmts = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG)
//	| SETb.P(Eaddr) [name] =>
//		stmts = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG)
	| SETb.NS(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JPOS)
	| SETb.S(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JMI)
	| SETb.NBE(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JUG)
	| SETb.BE(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JULE)
	| SETb.NZ(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JNE)
	| SETb.Z(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JE)
	| SETb.NB(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JUGE)
	| SETb.B(Eaddr) [name] =>
		stmts = instantiate(pc, name, DIS_EADDR8);
		SETS(name, DIS_EADDR8, BRANCH_JUL)
//	| SETb.NO(Eaddr) [name] =>
//		stmts = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG)
//	| SETb.O(Eaddr) [name] =>
//		stmts = instantiate(pc, name, DIS_EADDR8);
//		SETS(name, DIS_EADDR8, BRANCH_JSG)

	| XLATB() =>
		stmts = instantiate(pc, "XLATB");

	| XCHG.Ev.Gvod(Eaddr, reg) =>
		stmts = instantiate(pc, "XCHG.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| XCHG.Ev.Gvow(Eaddr, reg) =>
		stmts = instantiate(pc, "XCHG.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| XCHG.Eb.Gb(Eaddr, reg) =>
		stmts = instantiate(pc, "XCHG.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| NOP() =>
		stmts = instantiate(pc, "NOP");

	| SEG.CS() =>  // For now, treat seg.cs as a 1 byte NOP
		stmts = instantiate(pc, "NOP");

	| SEG.DS() =>  // For now, treat seg.ds as a 1 byte NOP
		stmts = instantiate(pc, "NOP");

	| SEG.ES() =>  // For now, treat seg.es as a 1 byte NOP
		stmts = instantiate(pc, "NOP");

	| SEG.FS() =>  // For now, treat seg.fs as a 1 byte NOP
		stmts = instantiate(pc, "NOP");

	| SEG.GS() =>  // For now, treat seg.gs as a 1 byte NOP
		stmts = instantiate(pc, "NOP");

	| SEG.SS() =>  // For now, treat seg.ss as a 1 byte NOP
		stmts = instantiate(pc, "NOP");

	| XCHGeAXod(r32) =>
		stmts = instantiate(pc, "XCHGeAXod", DIS_R32);

	| XCHGeAXow(r32) =>
		stmts = instantiate(pc, "XCHGeAXow", DIS_R32);

	| XADD.Ev.Gvod(Eaddr, reg) =>
		stmts = instantiate(pc, "XADD.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| XADD.Ev.Gvow(Eaddr, reg) =>
		stmts = instantiate(pc, "XADD.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| XADD.Eb.Gb(Eaddr, reg) =>
		stmts = instantiate(pc, "XADD.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| WRMSR() =>
		stmts = instantiate(pc, "WRMSR");

	| WBINVD() =>
		stmts = instantiate(pc, "WBINVD");

	| WAIT() =>
		stmts = instantiate(pc, "WAIT");

	| VERW(Eaddr) =>
		stmts = instantiate(pc, "VERW", DIS_EADDR32);

	| VERR(Eaddr) =>
		stmts = instantiate(pc, "VERR", DIS_EADDR32);

	| TEST.Ev.Gvod(Eaddr, reg) =>
		stmts = instantiate(pc, "TEST.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| TEST.Ev.Gvow(Eaddr, reg) =>
		stmts = instantiate(pc, "TEST.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| TEST.Eb.Gb(Eaddr, reg) =>
		stmts = instantiate(pc, "TEST.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| TEST.Ed.Id(Eaddr, i32) =>
		stmts = instantiate(pc, "TEST.Ed.Id", DIS_EADDR32, DIS_I32);

	| TEST.Ew.Iw(Eaddr, i16) =>
		stmts = instantiate(pc, "TEST.Ew.Iw", DIS_EADDR16, DIS_I16);

	| TEST.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "TEST.Eb.Ib", DIS_EADDR8, DIS_I8);

	| TEST.eAX.Ivod(i32) =>
		stmts = instantiate(pc, "TEST.eAX.Ivod", DIS_I32);

	| TEST.eAX.Ivow(i16) =>
		stmts = instantiate(pc, "TEST.eAX.Ivow", DIS_I16);

	| TEST.AL.Ib(i8) =>
		stmts = instantiate(pc, "TEST.AL.Ib", DIS_I8);

	| STR(Mem) =>
		stmts = instantiate(pc, "STR", DIS_MEM);

	| STOSvod() =>
		stmts = instantiate(pc, "STOSvod");

	| STOSvow() =>
		stmts = instantiate(pc, "STOSvow");

	| STOSB() =>
		stmts = instantiate(pc, "STOSB");

	| STI() =>
		stmts = instantiate(pc, "STI");

	| STD() =>
		stmts = instantiate(pc, "STD");

	| STC() =>
		stmts = instantiate(pc, "STC");

	| SMSW(Eaddr) =>
		stmts = instantiate(pc, "SMSW", DIS_EADDR32);

	| SLDT(Eaddr) =>
		stmts = instantiate(pc, "SLDT", DIS_EADDR32);

	| SHLD.CLod(Eaddr, reg) =>
		stmts = instantiate(pc, "SHLD.CLod", DIS_EADDR32, DIS_REG32);

	| SHLD.CLow(Eaddr, reg) =>
		stmts = instantiate(pc, "SHLD.CLow", DIS_EADDR16, DIS_REG16);

	| SHRD.CLod(Eaddr, reg) =>
		stmts = instantiate(pc, "SHRD.CLod", DIS_EADDR32, DIS_REG32);

	| SHRD.CLow(Eaddr, reg) =>
		stmts = instantiate(pc, "SHRD.CLow", DIS_EADDR16, DIS_REG16);

	| SHLD.Ibod(Eaddr, reg, count) =>
		stmts = instantiate(pc, "SHLD.Ibod", DIS_EADDR32, DIS_REG32, DIS_COUNT);

	| SHLD.Ibow(Eaddr, reg, count) =>
		stmts = instantiate(pc, "SHLD.Ibow", DIS_EADDR16, DIS_REG16, DIS_COUNT);

	| SHRD.Ibod(Eaddr, reg, count) =>
		stmts = instantiate(pc, "SHRD.Ibod", DIS_EADDR32, DIS_REG32, DIS_COUNT);

	| SHRD.Ibow(Eaddr, reg, count) =>
		stmts = instantiate(pc, "SHRD.Ibow", DIS_EADDR16, DIS_REG16, DIS_COUNT);

	| SIDT(Mem) =>
		stmts = instantiate(pc, "SIDT", DIS_MEM);

	| SGDT(Mem) =>
		stmts = instantiate(pc, "SGDT", DIS_MEM);

	// Sets are now in the high level instructions
	| SCASvod() =>
		stmts = instantiate(pc, "SCASvod");

	| SCASvow() =>
		stmts = instantiate(pc, "SCASvow");

	| SCASB() =>
		stmts = instantiate(pc, "SCASB");

	| SAHF() =>
		stmts = instantiate(pc, "SAHF");

	| RSM() =>
		stmts = instantiate(pc, "RSM");

	| RET.far.Iw(i16) =>
		stmts = instantiate(pc, "RET.far.Iw", DIS_I16);
		auto ret = new ReturnStatement;
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(ret);

	| RET.Iw(i16) =>
		stmts = instantiate(pc, "RET.Iw", DIS_I16);
		auto ret = new ReturnStatement;
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(ret);

	| RET.far() =>
		stmts = instantiate(pc, "RET.far");
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(new ReturnStatement);

	| RET() =>
		stmts = instantiate(pc, "RET");
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(new ReturnStatement);

//	| REPNE() =>
//		stmts = instantiate(pc, "REPNE");

//	| REP() =>
//		stmts = instantiate(pc, "REP");

	| REP.CMPSB() [name] =>
		stmts = instantiate(pc, name);

	| REP.CMPSvow() [name] =>
		stmts = instantiate(pc, name);

	| REP.CMPSvod() [name] =>
		stmts = instantiate(pc, name);

	| REP.LODSB() [name] =>
		stmts = instantiate(pc, name);

	| REP.LODSvow() [name] =>
		stmts = instantiate(pc, name);

	| REP.LODSvod() [name] =>
		stmts = instantiate(pc, name);

	| REP.MOVSB() [name] =>
		stmts = instantiate(pc, name);

	| REP.MOVSvow() [name] =>
		stmts = instantiate(pc, name);

	| REP.MOVSvod() [name] =>
		stmts = instantiate(pc, name);

	| REP.SCASB() [name] =>
		stmts = instantiate(pc, name);

	| REP.SCASvow() [name] =>
		stmts = instantiate(pc, name);

	| REP.SCASvod() [name] =>
		stmts = instantiate(pc, name);

	| REP.STOSB() [name] =>
		stmts = instantiate(pc, name);

	| REP.STOSvow() [name] =>
		stmts = instantiate(pc, name);

	| REP.STOSvod() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.CMPSB() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.CMPSvow() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.CMPSvod() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.LODSB() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.LODSvow() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.LODSvod() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.MOVSB() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.MOVSvow() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.MOVSvod() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.SCASB() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.SCASvow() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.SCASvod() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.STOSB() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.STOSvow() [name] =>
		stmts = instantiate(pc, name);

	| REPNE.STOSvod() [name] =>
		stmts = instantiate(pc, name);

	| RDMSR() =>
		stmts = instantiate(pc, "RDMSR");

	| SARB.Ev.Ibod(Eaddr, i8) =>
		stmts = instantiate(pc, "SARB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| SARB.Ev.Ibow(Eaddr, i8) =>
		stmts = instantiate(pc, "SARB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| SHRB.Ev.Ibod(Eaddr, i8) =>
		stmts = instantiate(pc, "SHRB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| SHRB.Ev.Ibow(Eaddr, i8) =>
		stmts = instantiate(pc, "SHRB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| SHLSALB.Ev.Ibod(Eaddr, i8) =>
		stmts = instantiate(pc, "SHLSALB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| SHLSALB.Ev.Ibow(Eaddr, i8) =>
		stmts = instantiate(pc, "SHLSALB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| RCRB.Ev.Ibod(Eaddr, i8) =>
		stmts = instantiate(pc, "RCRB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| RCRB.Ev.Ibow(Eaddr, i8) =>
		stmts = instantiate(pc, "RCRB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| RCLB.Ev.Ibod(Eaddr, i8) =>
		stmts = instantiate(pc, "RCLB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| RCLB.Ev.Ibow(Eaddr, i8) =>
		stmts = instantiate(pc, "RCLB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| RORB.Ev.Ibod(Eaddr, i8) =>
		stmts = instantiate(pc, "RORB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| RORB.Ev.Ibow(Eaddr, i8) =>
		stmts = instantiate(pc, "RORB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| ROLB.Ev.Ibod(Eaddr, i8) =>
		stmts = instantiate(pc, "ROLB.Ev.Ibod", DIS_EADDR32, DIS_I8);

	| ROLB.Ev.Ibow(Eaddr, i8) =>
		stmts = instantiate(pc, "ROLB.Ev.Ibow", DIS_EADDR16, DIS_I8);

	| SARB.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "SARB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| SHRB.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "SHRB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| SHLSALB.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "SHLSALB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| RCRB.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "RCRB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| RCLB.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "RCLB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| RORB.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "RORB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| ROLB.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "ROLB.Eb.Ib", DIS_EADDR8, DIS_I8);

	| SARB.Ev.CLod(Eaddr) =>
		stmts = instantiate(pc, "SARB.Ev.CLod", DIS_EADDR32);

	| SARB.Ev.CLow(Eaddr) =>
		stmts = instantiate(pc, "SARB.Ev.CLow", DIS_EADDR16);

	| SARB.Ev.1od(Eaddr) =>
		stmts = instantiate(pc, "SARB.Ev.1od", DIS_EADDR32);

	| SARB.Ev.1ow(Eaddr) =>
		stmts = instantiate(pc, "SARB.Ev.1ow", DIS_EADDR16);

	| SHRB.Ev.CLod(Eaddr) =>
		stmts = instantiate(pc, "SHRB.Ev.CLod", DIS_EADDR32);

	| SHRB.Ev.CLow(Eaddr) =>
		stmts = instantiate(pc, "SHRB.Ev.CLow", DIS_EADDR16);

	| SHRB.Ev.1od(Eaddr) =>
		stmts = instantiate(pc, "SHRB.Ev.1od", DIS_EADDR32);

	| SHRB.Ev.1ow(Eaddr) =>
		stmts = instantiate(pc, "SHRB.Ev.1ow", DIS_EADDR16);

	| SHLSALB.Ev.CLod(Eaddr) =>
		stmts = instantiate(pc, "SHLSALB.Ev.CLod", DIS_EADDR32);

	| SHLSALB.Ev.CLow(Eaddr) =>
		stmts = instantiate(pc, "SHLSALB.Ev.CLow", DIS_EADDR16);

	| SHLSALB.Ev.1od(Eaddr) =>
		stmts = instantiate(pc, "SHLSALB.Ev.1od", DIS_EADDR32);

	| SHLSALB.Ev.1ow(Eaddr) =>
		stmts = instantiate(pc, "SHLSALB.Ev.1ow", DIS_EADDR16);

	| RCRB.Ev.CLod(Eaddr) =>
		stmts = instantiate(pc, "RCRB.Ev.CLod", DIS_EADDR32);

	| RCRB.Ev.CLow(Eaddr) =>
		stmts = instantiate(pc, "RCRB.Ev.CLow", DIS_EADDR16);

	| RCRB.Ev.1od(Eaddr) =>
		stmts = instantiate(pc, "RCRB.Ev.1od", DIS_EADDR32);

	| RCRB.Ev.1ow(Eaddr) =>
		stmts = instantiate(pc, "RCRB.Ev.1ow", DIS_EADDR16);

	| RCLB.Ev.CLod(Eaddr) =>
		stmts = instantiate(pc, "RCLB.Ev.CLod", DIS_EADDR32);

	| RCLB.Ev.CLow(Eaddr) =>
		stmts = instantiate(pc, "RCLB.Ev.CLow", DIS_EADDR16);

	| RCLB.Ev.1od(Eaddr) =>
		stmts = instantiate(pc, "RCLB.Ev.1od", DIS_EADDR32);

	| RCLB.Ev.1ow(Eaddr) =>
		stmts = instantiate(pc, "RCLB.Ev.1ow", DIS_EADDR16);

	| RORB.Ev.CLod(Eaddr) =>
		stmts = instantiate(pc, "RORB.Ev.CLod", DIS_EADDR32);

	| RORB.Ev.CLow(Eaddr) =>
		stmts = instantiate(pc, "RORB.Ev.CLow", DIS_EADDR16);

	| RORB.Ev.1od(Eaddr) =>
		stmts = instantiate(pc, "RORB.Ev.1od", DIS_EADDR32);

	| RORB.Ev.1ow(Eaddr) =>
		stmts = instantiate(pc, "ORB.Ev.1owR", DIS_EADDR16);

	| ROLB.Ev.CLod(Eaddr) =>
		stmts = instantiate(pc, "ROLB.Ev.CLod", DIS_EADDR32);

	| ROLB.Ev.CLow(Eaddr) =>
		stmts = instantiate(pc, "ROLB.Ev.CLow", DIS_EADDR16);

	| ROLB.Ev.1od(Eaddr) =>
		stmts = instantiate(pc, "ROLB.Ev.1od", DIS_EADDR32);

	| ROLB.Ev.1ow(Eaddr) =>
		stmts = instantiate(pc, "ROLB.Ev.1ow", DIS_EADDR16);

	| SARB.Eb.CL(Eaddr) =>
		stmts = instantiate(pc, "SARB.Eb.CL", DIS_EADDR32);

	| SARB.Eb.1(Eaddr) =>
		stmts = instantiate(pc, "SARB.Eb.1", DIS_EADDR16);

	| SHRB.Eb.CL(Eaddr) =>
		stmts = instantiate(pc, "SHRB.Eb.CL", DIS_EADDR8);

	| SHRB.Eb.1(Eaddr) =>
		stmts = instantiate(pc, "SHRB.Eb.1", DIS_EADDR8);

	| SHLSALB.Eb.CL(Eaddr) =>
		stmts = instantiate(pc, "SHLSALB.Eb.CL", DIS_EADDR8);

	| SHLSALB.Eb.1(Eaddr) =>
		stmts = instantiate(pc, "SHLSALB.Eb.1", DIS_EADDR8);

	| RCRB.Eb.CL(Eaddr) =>
		stmts = instantiate(pc, "RCRB.Eb.CL", DIS_EADDR8);

	| RCRB.Eb.1(Eaddr) =>
		stmts = instantiate(pc, "RCRB.Eb.1", DIS_EADDR8);

	| RCLB.Eb.CL(Eaddr) =>
		stmts = instantiate(pc, "RCLB.Eb.CL", DIS_EADDR8);

	| RCLB.Eb.1(Eaddr) =>
		stmts = instantiate(pc, "RCLB.Eb.1", DIS_EADDR8);

	| RORB.Eb.CL(Eaddr) =>
		stmts = instantiate(pc, "RORB.Eb.CL", DIS_EADDR8);

	| RORB.Eb.1(Eaddr) =>
		stmts = instantiate(pc, "RORB.Eb.1", DIS_EADDR8);

	| ROLB.Eb.CL(Eaddr) =>
		stmts = instantiate(pc, "ROLB.Eb.CL", DIS_EADDR8);

	| ROLB.Eb.1(Eaddr) =>
		stmts = instantiate(pc, "ROLB.Eb.1", DIS_EADDR8);

	// There is no SSL for these, so don't call instantiate, it will only
	// cause an assert failure. Also, may as well treat these as invalid instr
//	| PUSHFod() =>
//		stmts = instantiate(pc, "PUSHFod");

//	| PUSHFow() =>
//		stmts = instantiate(pc, "PUSHFow");

//	| PUSHAod() =>
//		stmts = instantiate(pc, "PUSHAod");

//	| PUSHAow() =>
//		stmts = instantiate(pc, "PUSHAow");

	| PUSH.GS() =>
		stmts = instantiate(pc, "PUSH.GS");

	| PUSH.FS() =>
		stmts = instantiate(pc, "PUSH.FS");

	| PUSH.ES() =>
		stmts = instantiate(pc, "PUSH.ES");

	| PUSH.DS() =>
		stmts = instantiate(pc, "PUSH.DS");

	| PUSH.SS() =>
		stmts = instantiate(pc, "PUSH.SS");

	| PUSH.CS() =>
		stmts = instantiate(pc, "PUSH.CS");

	| PUSH.Ivod(i32) =>
		stmts = instantiate(pc, "PUSH.Ivod", DIS_I32);

	| PUSH.Ivow(i16) =>
		stmts = instantiate(pc, "PUSH.Ivow", DIS_I16);

	| PUSH.Ixob(i8) =>
		stmts = instantiate(pc, "PUSH.Ixob", DIS_I8);

	| PUSH.Ixow(i8) =>
		stmts = instantiate(pc, "PUSH.Ixow", DIS_I8);

	| PUSHod(r32) =>
		stmts = instantiate(pc, "PUSHod", DIS_R32);

	| PUSHow(r32) =>
		stmts = instantiate(pc, "PUSHow", DIS_R32);  // Check!

	| PUSH.Evod(Eaddr) =>
		stmts = instantiate(pc, "PUSH.Evod", DIS_EADDR32);

	| PUSH.Evow(Eaddr) =>
		stmts = instantiate(pc, "PUSH.Evow", DIS_EADDR16);

//	| POPFod() =>
//		stmts = instantiate(pc, "POPFod");

//	| POPFow() =>
//		stmts = instantiate(pc, "POPFow");

//	| POPAod() =>
//		stmts = instantiate(pc, "POPAod");

//	| POPAow() =>
//		stmts = instantiate(pc, "POPAow");

	| POP.GS() =>
		stmts = instantiate(pc, "POP.GS");

	| POP.FS() =>
		stmts = instantiate(pc, "POP.FS");

	| POP.DS() =>
		stmts = instantiate(pc, "POP.DS");

	| POP.SS() =>
		stmts = instantiate(pc, "POP.SS");

	| POP.ES() =>
		stmts = instantiate(pc, "POP.ES");

	| POPod(r32) =>
		stmts = instantiate(pc, "POPod", DIS_R32);

	| POPow(r32) =>
		stmts = instantiate(pc, "POPow", DIS_R32);  // Check!

	| POP.Evod(Eaddr) =>
		stmts = instantiate(pc, "POP.Evod", DIS_EADDR32);

	| POP.Evow(Eaddr) =>
		stmts = instantiate(pc, "POP.Evow", DIS_EADDR16);

//	| OUTSvod() =>
//		stmts = instantiate(pc, "OUTSvod");

//	| OUTSvow() =>
//		stmts = instantiate(pc, "OUTSvow");

//	| OUTSB() =>
//		stmts = instantiate(pc, "OUTSB");

//	| OUT.DX.eAXod() =>
//		stmts = instantiate(pc, "OUT.DX.eAXod");

//	| OUT.DX.eAXow() =>
//		stmts = instantiate(pc, "OUT.DX.eAXow");

//	| OUT.DX.AL() =>
//		stmts = instantiate(pc, "OUT.DX.AL");

//	| OUT.Ib.eAXod(i8) =>
//		stmts = instantiate(pc, "OUT.Ib.eAXod", DIS_I8);

//	| OUT.Ib.eAXow(i8) =>
//		stmts = instantiate(pc, "OUT.Ib.eAXow", DIS_I8);

//	| OUT.Ib.AL(i8) =>
//		stmts = instantiate(pc, "OUT.Ib.AL", DIS_I8);

	| NOTod(Eaddr) =>
		stmts = instantiate(pc, "NOTod", DIS_EADDR32);

	| NOTow(Eaddr) =>
		stmts = instantiate(pc, "NOTow", DIS_EADDR16);

	| NOTb(Eaddr) =>
		stmts = instantiate(pc, "NOTb", DIS_EADDR8);

	| NEGod(Eaddr) =>
		stmts = instantiate(pc, "NEGod", DIS_EADDR32);

	| NEGow(Eaddr) =>
		stmts = instantiate(pc, "NEGow", DIS_EADDR16);

	| NEGb(Eaddr) =>
		stmts = instantiate(pc, "NEGb", DIS_EADDR8);

	| MUL.AXod(Eaddr) =>
		stmts = instantiate(pc, "MUL.AXod", DIS_EADDR32);

	| MUL.AXow(Eaddr) =>
		stmts = instantiate(pc, "MUL.AXow", DIS_EADDR16);

	| MUL.AL(Eaddr) =>
		stmts = instantiate(pc, "MUL.AL", DIS_EADDR8);

	| MOVZX.Gv.Ew(r32, Eaddr) =>
		stmts = instantiate(pc, "MOVZX.Gv.Ew", DIS_R32, DIS_EADDR16);

	| MOVZX.Gv.Ebod(r32, Eaddr) =>
		stmts = instantiate(pc, "MOVZX.Gv.Ebod", DIS_R32, DIS_EADDR8);

	| MOVZX.Gv.Ebow(r16, Eaddr) =>
		stmts = instantiate(pc, "MOVZX.Gv.Ebow", DIS_R16, DIS_EADDR8);

	| MOVSX.Gv.Ew(r32, Eaddr) =>
		stmts = instantiate(pc, "MOVSX.Gv.Ew", DIS_R32, DIS_EADDR16);

	| MOVSX.Gv.Ebod(r32, Eaddr) =>
		stmts = instantiate(pc, "MOVSX.Gv.Ebod", DIS_R32, DIS_EADDR8);

	| MOVSX.Gv.Ebow(r16, Eaddr) =>
		stmts = instantiate(pc, "MOVZX.Gv.Ebow", DIS_R16, DIS_EADDR8);

	| MOVSvod() =>
		stmts = instantiate(pc, "MOVSvod");

	| MOVSvow() =>
		stmts = instantiate(pc, "MOVSvow");

	| MOVSB() =>
		stmts = instantiate(pc, "MOVSB");

//	| MOV.Rd.Dd(_, _) =>
//	//| MOV.Rd.Dd(reg, dr) =>
//		stmts = instantiate(pc, "UNIMP");

//	| MOV.Dd.Rd(_, _) =>
//	//| MOV.Dd.Rd(dr, reg) =>
//		stmts = instantiate(pc, "UNIMP");

//	| MOV.Rd.Cd(_, _) =>
//	//| MOV.Rd.Cd(reg, cr) =>
//		stmts = instantiate(pc, "UNIMP");

//	| MOV.Cd.Rd(_, _) =>
//	//| MOV.Cd.Rd(cr, reg) =>
//		stmts = instantiate(pc, "UNIMP");

	| MOV.Ed.Ivod(Eaddr, i32) =>
		stmts = instantiate(pc, "MOV.Ed.Ivod", DIS_EADDR32, DIS_I32);

	| MOV.Ew.Ivow(Eaddr, i16) =>
		stmts = instantiate(pc, "MOV.Ew.Ivow", DIS_EADDR16, DIS_I16);

	| MOV.Eb.Ib(Eaddr, i8) =>
		stmts = instantiate(pc, "MOV.Eb.Ib", DIS_EADDR8, DIS_I8);

	| MOVid(r32, i32) =>
		stmts = instantiate(pc, "MOVid", DIS_R32, DIS_I32);

	| MOViw(r16, i16) =>
		stmts = instantiate(pc, "MOViw", DIS_R16, DIS_I16);  // Check!

	| MOVib(r8, i8) =>
		stmts = instantiate(pc, "MOVib", DIS_R8, DIS_I8);

	| MOV.Ov.eAXod(off) =>
		stmts = instantiate(pc, "MOV.Ov.eAXod", DIS_OFF);

	| MOV.Ov.eAXow(off) =>
		stmts = instantiate(pc, "MOV.Ov.eAXow", DIS_OFF);

	| MOV.Ob.AL(off) =>
		stmts = instantiate(pc, "MOV.Ob.AL", DIS_OFF);

	| MOV.eAX.Ovod(off) =>
		stmts = instantiate(pc, "MOV.eAX.Ovod", DIS_OFF);

	| MOV.eAX.Ovow(off) =>
		stmts = instantiate(pc, "MOV.eAX.Ovow", DIS_OFF);

	| MOV.AL.Ob(off) =>
		stmts = instantiate(pc, "MOV.AL.Ob", DIS_OFF);

//	| MOV.Sw.Ew(Mem, sr16) =>
//		stmts = instantiate(pc, "MOV.Sw.Ew", DIS_MEM, DIS_SR16);

//	| MOV.Ew.Sw(Mem, sr16) =>
//		stmts = instantiate(pc, "MOV.Ew.Sw", DIS_MEM, DIS_SR16);

	| MOVrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "MOVrmod", DIS_REG32, DIS_EADDR32);

	| MOVrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "MOVrmow", DIS_REG16, DIS_EADDR16);

	| MOVrmb(reg, Eaddr) =>
		stmts = instantiate(pc, "MOVrmb", DIS_REG8, DIS_EADDR8);

	| MOVmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "MOVmrod", DIS_EADDR32, DIS_REG32);

	| MOVmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "MOVmrow", DIS_EADDR16, DIS_REG16);

	| MOVmrb(Eaddr, reg) =>
		stmts = instantiate(pc, "MOVmrb", DIS_EADDR8, DIS_REG8);

	| LTR(Eaddr) =>
		stmts = instantiate(pc, "LTR", DIS_EADDR32);

	| LSS(reg, Mem) =>
		stmts = instantiate(pc, "LSS", DIS_REG32, DIS_MEM);

	| LSLod(reg, Eaddr) =>
		stmts = instantiate(pc, "LSLod", DIS_REG32, DIS_EADDR32);

	| LSLow(reg, Eaddr) =>
		stmts = instantiate(pc, "LSLow", DIS_REG16, DIS_EADDR16);

	| LOOPNE(relocd) =>
		stmts = instantiate(pc, "LOOPNE", dis_Num(relocd - nextPC));

	| LOOPE(relocd) =>
		stmts = instantiate(pc, "LOOPE", dis_Num(relocd - nextPC));

	| LOOP(relocd) =>
		stmts = instantiate(pc, "LOOP", dis_Num(relocd - nextPC));

	| LGS(reg, Mem) =>
		stmts = instantiate(pc, "LGS", DIS_REG32, DIS_MEM);

	| LFS(reg, Mem) =>
		stmts = instantiate(pc, "LFS", DIS_REG32, DIS_MEM);

	| LES(reg, Mem) =>
		stmts = instantiate(pc, "LES", DIS_REG32, DIS_MEM);

	| LEAVE() =>
		stmts = instantiate(pc, "LEAVE");

	| LEAod(reg, Mem) =>
		stmts = instantiate(pc, "LEA.od", DIS_REG32, DIS_MEM);

	| LEAow(reg, Mem) =>
		stmts = instantiate(pc, "LEA.ow", DIS_REG16, DIS_MEM);

	| LDS(reg, Mem) =>
		stmts = instantiate(pc, "LDS", DIS_REG32, DIS_MEM);

	| LARod(reg, Eaddr) =>
		stmts = instantiate(pc, "LAR.od", DIS_REG32, DIS_EADDR32);

	| LARow(reg, Eaddr) =>
		stmts = instantiate(pc, "LAR.ow", DIS_REG16, DIS_EADDR16);

	| LAHF() =>
		stmts = instantiate(pc, "LAHF");

	/* Branches have been handled in decodeInstruction() now */
	| IRET() =>
		stmts = instantiate(pc, "IRET");

	| INVLPG(Mem) =>
		stmts = instantiate(pc, "INVLPG", DIS_MEM);

	| INVD() =>
		stmts = instantiate(pc, "INVD");

	| INTO() =>
		stmts = instantiate(pc, "INTO");

	| INT.Ib(i8) =>
		stmts = instantiate(pc, "INT.Ib", DIS_I8);

// Removing because an invalid instruction is better than trying to
// instantiate this. -trent
//	| INT3() =>
//		stmts = instantiate(pc, "INT3");

//	| INSvod() =>
//		stmts = instantiate(pc, "INSvod");

//	| INSvow() =>
//		stmts = instantiate(pc, "INSvow");

//	| INSB() =>
//		stmts = instantiate(pc, "INSB");

	| INCod(r32) =>
		stmts = instantiate(pc, "INCod", DIS_R32);

	| INCow(r32) =>
		stmts = instantiate(pc, "INCow", DIS_R32);

	| INC.Evod(Eaddr) =>
		stmts = instantiate(pc, "INC.Evod", DIS_EADDR32);

	| INC.Evow(Eaddr) =>
		stmts = instantiate(pc, "INC.Evow", DIS_EADDR16);

	| INC.Eb(Eaddr) =>
		stmts = instantiate(pc, "INC.Eb", DIS_EADDR8);

//	| IN.eAX.DXod() =>
//		stmts = instantiate(pc, "IN.eAX.DXod");

//	| IN.eAX.DXow() =>
//		stmts = instantiate(pc, "IN.eAX.DXow");

//	| IN.AL.DX() =>
//		stmts = instantiate(pc, "IN.AL.DX");

//	| IN.eAX.Ibod(i8) =>
//		stmts = instantiate(pc, "IN.eAX.Ibod", DIS_I8);

//	| IN.eAX.Ibow(i8) =>
//		stmts = instantiate(pc, "IN.eAX.Ibow", DIS_I8);

//	| IN.AL.Ib(i8) =>
//		stmts = instantiate(pc, "IN.AL.Ib", DIS_I8);

	| IMUL.Ivd(reg, Eaddr, i32) =>
		stmts = instantiate(pc, "IMUL.Ivd", DIS_REG32, DIS_EADDR32, DIS_I32);

	| IMUL.Ivw(reg, Eaddr, i16) =>
		stmts = instantiate(pc, "IMUL.Ivw", DIS_REG16, DIS_EADDR16, DIS_I16);

	| IMUL.Ibod(reg, Eaddr, i8) =>
		stmts = instantiate(pc, "IMUL.Ibod", DIS_REG32, DIS_EADDR32, DIS_I8);

	| IMUL.Ibow(reg, Eaddr, i8) =>
		stmts = instantiate(pc, "IMUL.Ibow", DIS_REG16, DIS_EADDR16, DIS_I8);

	| IMULrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "IMULrmod", DIS_REG32, DIS_EADDR32);

	| IMULrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "IMULrmow", DIS_REG16, DIS_EADDR16);

	| IMULod(Eaddr) =>
		stmts = instantiate(pc, "IMULod", DIS_EADDR32);

	| IMULow(Eaddr) =>
		stmts = instantiate(pc, "IMULow", DIS_EADDR16);

	| IMULb(Eaddr) =>
		stmts = instantiate(pc, "IMULb", DIS_EADDR8);

	| IDIVeAX(Eaddr) =>
		stmts = instantiate(pc, "IDIVeAX", DIS_EADDR32);

	| IDIVAX(Eaddr) =>
		stmts = instantiate(pc, "IDIVAX", DIS_EADDR16);

	| IDIV(Eaddr) =>
		stmts = instantiate(pc, "IDIV", DIS_EADDR8); /* ?? */

//	| HLT() =>
//		stmts = instantiate(pc, "HLT");

	| ENTER(i16, i8) =>
		stmts = instantiate(pc, "ENTER", DIS_I16, DIS_I8);

	| DIVeAX(Eaddr) =>
		stmts = instantiate(pc, "DIVeAX", DIS_EADDR32);

	| DIVAX(Eaddr) =>
		stmts = instantiate(pc, "DIVAX", DIS_EADDR16);

	| DIVAL(Eaddr) =>
		stmts = instantiate(pc, "DIVAL", DIS_EADDR8);

	| DECod(r32) =>
		stmts = instantiate(pc, "DECod", DIS_R32);

	| DECow(r32) =>
		stmts = instantiate(pc, "DECow", DIS_R32);

	| DEC.Evod(Eaddr) =>
		stmts = instantiate(pc, "DEC.Evod", DIS_EADDR32);

	| DEC.Evow(Eaddr) =>
		stmts = instantiate(pc, "DEC.Evow", DIS_EADDR16);

	| DEC.Eb(Eaddr) =>
		stmts = instantiate(pc, "DEC.Eb", DIS_EADDR8);

	| DAS() =>
		stmts = instantiate(pc, "DAS");

	| DAA() =>
		stmts = instantiate(pc, "DAA");

	| CDQ() =>
		stmts = instantiate(pc, "CDQ");

	| CWD() =>
		stmts = instantiate(pc, "CWD");

	| CPUID() =>
		stmts = instantiate(pc, "CPUID");

	| CMPXCHG8B(Mem) =>
		stmts = instantiate(pc, "CMPXCHG8B", DIS_MEM);

	| CMPXCHG.Ev.Gvod(Eaddr, reg) =>
		stmts = instantiate(pc, "CMPXCHG.Ev.Gvod", DIS_EADDR32, DIS_REG32);

	| CMPXCHG.Ev.Gvow(Eaddr, reg) =>
		stmts = instantiate(pc, "CMPXCHG.Ev.Gvow", DIS_EADDR16, DIS_REG16);

	| CMPXCHG.Eb.Gb(Eaddr, reg) =>
		stmts = instantiate(pc, "CMPXCHG.Eb.Gb", DIS_EADDR8, DIS_REG8);

	| CMPSvod() =>
		stmts = instantiate(pc, "CMPSvod");

	| CMPSvow() =>
		stmts = instantiate(pc, "CMPSvow");

	| CMPSB() =>
		stmts = instantiate(pc, "CMPSB");

	| CMC() =>
		stmts = instantiate(pc, "CMC");

	| CLTS() =>
		stmts = instantiate(pc, "CLTS");

	| CLI() =>
		stmts = instantiate(pc, "CLI");

	| CLD() =>
		stmts = instantiate(pc, "CLD");

	| CLC() =>
		stmts = instantiate(pc, "CLC");

	| CWDE() =>
		stmts = instantiate(pc, "CWDE");

	| CBW() =>
		stmts = instantiate(pc, "CBW");

	/* Decode the following as a NOP. We see these in startup code, and anywhere
	 * that calls the OS (as lcall 7, 0) */
	| CALL.aPod(_, _) =>
	//| CALL.aPod(seg, off) =>
		stmts = instantiate(pc, "NOP");

	| CALL.Jvod(relocd) =>
		stmts = instantiate(pc, "CALL.Jvod", dis_Num(relocd));
		ADDRESS nativeDest = relocd;
		if (nativeDest == nextPC) {
			// This is a call $+5
			// Use the standard semantics, except for the last statement
			// (just updates %pc)
			stmts->pop_back();
			// And don't make it a call statement
		} else {
			auto call = new CallStatement;
			// Set the destination
			call->setDest(nativeDest);
			stmts->push_back(call);
			Proc *destProc = prog->setNewProc(nativeDest);
			if (destProc == (Proc *)-1) destProc = nullptr;  // In case a deleted Proc
			call->setDestProc(destProc);
		}
		result.rtl = new RTL(pc, stmts);

	| BTSiod(Eaddr, i8) =>
		stmts = instantiate(pc, "BTSiod", DIS_I8, DIS_EADDR32);

	| BTSiow(Eaddr, i8) =>
		stmts = instantiate(pc, "BTSiow", DIS_I8, DIS_EADDR16);

	| BTSod(Eaddr, reg) =>
		stmts = instantiate(pc, "BTSod", DIS_EADDR32, DIS_REG32);

	| BTSow(Eaddr, reg) =>
		stmts = instantiate(pc, "BTSow", DIS_EADDR16, DIS_REG16);

	| BTRiod(Eaddr, i8) =>
		stmts = instantiate(pc, "BTRiod", DIS_EADDR32, DIS_I8);

	| BTRiow(Eaddr, i8) =>
		stmts = instantiate(pc, "BTRiow", DIS_EADDR16, DIS_I8);

	| BTRod(Eaddr, reg) =>
		stmts = instantiate(pc, "BTRod", DIS_EADDR32, DIS_REG32);

	| BTRow(Eaddr, reg) =>
		stmts = instantiate(pc, "BTRow", DIS_EADDR16, DIS_REG16);

	| BTCiod(Eaddr, i8) =>
		stmts = instantiate(pc, "BTCiod", DIS_EADDR32, DIS_I8);

	| BTCiow(Eaddr, i8) =>
		stmts = instantiate(pc, "BTCiow", DIS_EADDR16, DIS_I8);

	| BTCod(Eaddr, reg) =>
		stmts = instantiate(pc, "BTCod", DIS_EADDR32, DIS_REG32);

	| BTCow(Eaddr, reg) =>
		stmts = instantiate(pc, "BTCow", DIS_EADDR16, DIS_REG16);

	| BTiod(Eaddr, i8) =>
		stmts = instantiate(pc, "BTiod", DIS_EADDR32, DIS_I8);

	| BTiow(Eaddr, i8) =>
		stmts = instantiate(pc, "BTiow", DIS_EADDR16, DIS_I8);

	| BTod(Eaddr, reg) =>
		stmts = instantiate(pc, "BTod", DIS_EADDR32, DIS_REG32);

	| BTow(Eaddr, reg) =>
		stmts = instantiate(pc, "BTow", DIS_EADDR16, DIS_REG16);

	| BSWAP(r32) =>
		stmts = instantiate(pc, "BSWAP", DIS_R32);

	| BSRod(reg, Eaddr) =>
		//stmts = instantiate(pc, "BSRod", DIS_REG32, DIS_EADDR32);
		// Bit Scan Forward: need helper function
		genBSFR(pc, DIS_REG32, DIS_EADDR32, 32, 32, opMinus, nextPC - pc);
		return result;

	| BSRow(reg, Eaddr) =>
		//stmts = instantiate(pc, "BSRow", DIS_REG16, DIS_EADDR16);
		genBSFR(pc, DIS_REG16, DIS_EADDR16, 16, 16, opMinus, nextPC - pc);
		return result;

	| BSFod(reg, Eaddr) =>
		//stmts = instantiate(pc, "BSFod", DIS_REG32, DIS_EADDR32);
		genBSFR(pc, DIS_REG32, DIS_EADDR32, -1, 32, opPlus, nextPC - pc);
		return result;

	| BSFow(reg, Eaddr) =>
		//stmts = instantiate(pc, "BSFow", DIS_REG16, DIS_EADDR16);
		genBSFR(pc, DIS_REG16, DIS_EADDR16, -1, 16, opPlus, nextPC - pc);
		return result;

	// Not "user" instructions:
//	| BOUNDod(reg, Mem) =>
//		stmts = instantiate(pc, "BOUNDod", DIS_REG32, DIS_MEM);

//	| BOUNDow(reg, Mem) =>
//		stmts = instantiate(pc, "BOUNDow", DIS_REG16, DIS_MEM);

//	| ARPL(_, _) =>
//	//| ARPL(Eaddr, reg) =>
//		stmts = instantiate(pc, "UNIMP");

//	| AAS() =>
//		stmts = instantiate(pc, "AAS");

//	| AAM() =>
//		stmts = instantiate(pc, "AAM");

//	| AAD() =>
//		stmts = instantiate(pc, "AAD");

//	| AAA() =>
//		stmts = instantiate(pc, "AAA");

	| CMPrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "CMPrmod", DIS_REG32, DIS_EADDR32);

	| CMPrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "CMPrmow", DIS_REG16, DIS_EADDR16);

	| XORrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "XORrmod", DIS_REG32, DIS_EADDR32);

	| XORrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "XORrmow", DIS_REG16, DIS_EADDR16);

	| SUBrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "SUBrmod", DIS_REG32, DIS_EADDR32);

	| SUBrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "SUBrmow", DIS_REG16, DIS_EADDR16);

	| ANDrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "ANDrmod", DIS_REG32, DIS_EADDR32);

	| ANDrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "ANDrmow", DIS_REG16, DIS_EADDR16);

	| SBBrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "SBBrmod", DIS_REG32, DIS_EADDR32);

	| SBBrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "SBBrmow", DIS_REG16, DIS_EADDR16);

	| ADCrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "ADCrmod", DIS_REG32, DIS_EADDR32);

	| ADCrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "ADCrmow", DIS_REG16, DIS_EADDR16);

	| ORrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "ORrmod", DIS_REG32, DIS_EADDR32);

	| ORrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "ORrmow", DIS_REG16, DIS_EADDR16);

	| ADDrmod(reg, Eaddr) =>
		stmts = instantiate(pc, "ADDrmod", DIS_REG32, DIS_EADDR32);

	| ADDrmow(reg, Eaddr) =>
		stmts = instantiate(pc, "ADDrmow", DIS_REG16, DIS_EADDR16);

	| CMPrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "CMPrmb", DIS_R8, DIS_EADDR8);

	| XORrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "XORrmb", DIS_R8, DIS_EADDR8);

	| SUBrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "SUBrmb", DIS_R8, DIS_EADDR8);

	| ANDrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "ANDrmb", DIS_R8, DIS_EADDR8);

	| SBBrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "SBBrmb", DIS_R8, DIS_EADDR8);

	| ADCrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "ADCrmb", DIS_R8, DIS_EADDR8);

	| ORrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "ORrmb", DIS_R8, DIS_EADDR8);

	| ADDrmb(r8, Eaddr) =>
		stmts = instantiate(pc, "ADDrmb", DIS_R8, DIS_EADDR8);

	| CMPmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "CMPmrod", DIS_EADDR32, DIS_REG32);

	| CMPmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "CMPmrow", DIS_EADDR16, DIS_REG16);

	| XORmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "XORmrod", DIS_EADDR32, DIS_REG32);

	| XORmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "XORmrow", DIS_EADDR16, DIS_REG16);

	| SUBmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "SUBmrod", DIS_EADDR32, DIS_REG32);

	| SUBmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "SUBmrow", DIS_EADDR16, DIS_REG16);

	| ANDmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "ANDmrod", DIS_EADDR32, DIS_REG32);

	| ANDmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "ANDmrow", DIS_EADDR16, DIS_REG16);

	| SBBmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "SBBmrod", DIS_EADDR32, DIS_REG32);

	| SBBmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "SBBmrow", DIS_EADDR16, DIS_REG16);

	| ADCmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "ADCmrod", DIS_EADDR32, DIS_REG32);

	| ADCmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "ADCmrow", DIS_EADDR16, DIS_REG16);

	| ORmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "ORmrod", DIS_EADDR32, DIS_REG32);

	| ORmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "ORmrow", DIS_EADDR16, DIS_REG16);

	| ADDmrod(Eaddr, reg) =>
		stmts = instantiate(pc, "ADDmrod", DIS_EADDR32, DIS_REG32);

	| ADDmrow(Eaddr, reg) =>
		stmts = instantiate(pc, "ADDmrow", DIS_EADDR16, DIS_REG16);

	| CMPmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "CMPmrb", DIS_EADDR8, DIS_R8);

	| XORmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "XORmrb", DIS_EADDR8, DIS_R8);

	| SUBmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "SUBmrb", DIS_EADDR8, DIS_R8);

	| ANDmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "ANDmrb", DIS_EADDR8, DIS_R8);

	| SBBmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "SBBmrb", DIS_EADDR8, DIS_R8);

	| ADCmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "ADCmrb", DIS_EADDR8, DIS_R8);

	| ORmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "ORmrb", DIS_EADDR8, DIS_R8);

	| ADDmrb(Eaddr, r8) =>
		stmts = instantiate(pc, "ADDmrb", DIS_EADDR8, DIS_R8);

	| CMPiodb(Eaddr, i8) =>
		stmts = instantiate(pc, "CMPiodb", DIS_EADDR32, DIS_I8);

	| CMPiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "CMPiowb", DIS_EADDR16, DIS_I8);

	| XORiodb(Eaddr, i8) =>
		stmts = instantiate(pc, "XORiodb", DIS_EADDR32, DIS_I8);

	| XORiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "XORiowb", DIS_EADDR16, DIS_I8);

	| SUBiodb(Eaddr, i8) =>
		stmts = instantiate(pc, "SUBiodb", DIS_EADDR32, DIS_I8);

	| SUBiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "SUBiowb", DIS_EADDR16, DIS_I8);

	| ANDiodb(Eaddr, i8) =>
		// Special hack to ignore and $0xfffffff0, %esp
		Exp *oper = DIS_EADDR32;
		if (i8 != -16 || !(*oper == *Location::regOf(28)))
			stmts = instantiate(pc, "ANDiodb", DIS_EADDR32, DIS_I8);

	| ANDiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "ANDiowb", DIS_EADDR16, DIS_I8);

	| SBBiodb(Eaddr, i8) =>
		stmts = instantiate(pc, "SBBiodb", DIS_EADDR32, DIS_I8);

	| SBBiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "SBBiowb", DIS_EADDR16, DIS_I8);

	| ADCiodb(Eaddr, i8) =>
		stmts = instantiate(pc, "ADCiodb", DIS_EADDR32, DIS_I8);

	| ADCiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "ADCiowb", DIS_EADDR16, DIS_I8);

	| ORiodb(Eaddr, i8) =>
		stmts = instantiate(pc, "ORiodb", DIS_EADDR32, DIS_I8);

	| ORiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "ORiowb", DIS_EADDR16, DIS_I8);

	| ADDiodb(Eaddr, i8) =>
		stmts = instantiate(pc, "ADDiodb", DIS_EADDR32, DIS_I8);

	| ADDiowb(Eaddr, i8) =>
		stmts = instantiate(pc, "ADDiowb", DIS_EADDR16, DIS_I8);

	| CMPid(Eaddr, i32) =>
		stmts = instantiate(pc, "CMPid", DIS_EADDR32, DIS_I32);

	| XORid(Eaddr, i32) =>
		stmts = instantiate(pc, "XORid", DIS_EADDR32, DIS_I32);

	| SUBid(Eaddr, i32) =>
		stmts = instantiate(pc, "SUBid", DIS_EADDR32, DIS_I32);

	| ANDid(Eaddr, i32) =>
		stmts = instantiate(pc, "ANDid", DIS_EADDR32, DIS_I32);

	| SBBid(Eaddr, i32) =>
		stmts = instantiate(pc, "SBBid", DIS_EADDR32, DIS_I32);

	| ADCid(Eaddr, i32) =>
		stmts = instantiate(pc, "ADCid", DIS_EADDR32, DIS_I32);

	| ORid(Eaddr, i32) =>
		stmts = instantiate(pc, "ORid", DIS_EADDR32, DIS_I32);

	| ADDid(Eaddr, i32) =>
		stmts = instantiate(pc, "ADDid", DIS_EADDR32, DIS_I32);

	| CMPiw(Eaddr, i16) =>
		stmts = instantiate(pc, "CMPiw", DIS_EADDR16, DIS_I16);

	| XORiw(Eaddr, i16) =>
		stmts = instantiate(pc, "XORiw", DIS_EADDR16, DIS_I16);

	| SUBiw(Eaddr, i16) =>
		stmts = instantiate(pc, "SUBiw", DIS_EADDR16, DIS_I16);

	| ANDiw(Eaddr, i16) =>
		stmts = instantiate(pc, "ANDiw", DIS_EADDR16, DIS_I16);

	| SBBiw(Eaddr, i16) =>
		stmts = instantiate(pc, "SBBiw", DIS_EADDR16, DIS_I16);

	| ADCiw(Eaddr, i16) =>
		stmts = instantiate(pc, "ADCiw", DIS_EADDR16, DIS_I16);

	| ORiw(Eaddr, i16) =>
		stmts = instantiate(pc, "ORiw", DIS_EADDR16, DIS_I16);

	| ADDiw(Eaddr, i16) =>
		stmts = instantiate(pc, "ADDiw", DIS_EADDR16, DIS_I16);

	| CMPib(Eaddr, i8) =>
		stmts = instantiate(pc, "CMPib", DIS_EADDR8, DIS_I8);

	| XORib(Eaddr, i8) =>
		stmts = instantiate(pc, "XORib", DIS_EADDR8, DIS_I8);

	| SUBib(Eaddr, i8) =>
		stmts = instantiate(pc, "SUBib", DIS_EADDR8, DIS_I8);

	| ANDib(Eaddr, i8) =>
		stmts = instantiate(pc, "ANDib", DIS_EADDR8, DIS_I8);

	| SBBib(Eaddr, i8) =>
		stmts = instantiate(pc, "SBBib", DIS_EADDR8, DIS_I8);

	| ADCib(Eaddr, i8) =>
		stmts = instantiate(pc, "ADCib", DIS_EADDR8, DIS_I8);

	| ORib(Eaddr, i8) =>
		stmts = instantiate(pc, "ORib", DIS_EADDR8, DIS_I8);

	| ADDib(Eaddr, i8) =>
		stmts = instantiate(pc, "ADDib", DIS_EADDR8, DIS_I8);

	| CMPiEAX(i32) =>
		stmts = instantiate(pc, "CMPiEAX", DIS_I32);

	| XORiEAX(i32) =>
		stmts = instantiate(pc, "XORiEAX", DIS_I32);

	| SUBiEAX(i32) =>
		stmts = instantiate(pc, "SUBiEAX", DIS_I32);

	| ANDiEAX(i32) =>
		stmts = instantiate(pc, "ANDiEAX", DIS_I32);

	| SBBiEAX(i32) =>
		stmts = instantiate(pc, "SBBiEAX", DIS_I32);

	| ADCiEAX(i32) =>
		stmts = instantiate(pc, "ADCiEAX", DIS_I32);

	| ORiEAX(i32) =>
		stmts = instantiate(pc, "ORiEAX", DIS_I32);

	| ADDiEAX(i32) =>
		stmts = instantiate(pc, "ADDiEAX", DIS_I32);

	| CMPiAX(i16) =>
		stmts = instantiate(pc, "CMPiAX", DIS_I16);

	| XORiAX(i16) =>
		stmts = instantiate(pc, "XORiAX", DIS_I16);

	| SUBiAX(i16) =>
		stmts = instantiate(pc, "SUBiAX", DIS_I16);

	| ANDiAX(i16) =>
		stmts = instantiate(pc, "ANDiAX", DIS_I16);

	| SBBiAX(i16) =>
		stmts = instantiate(pc, "SBBiAX", DIS_I16);

	| ADCiAX(i16) =>
		stmts = instantiate(pc, "ADCiAX", DIS_I16);

	| ORiAX(i16) =>
		stmts = instantiate(pc, "ORiAX", DIS_I16);

	| ADDiAX(i16) =>
		stmts = instantiate(pc, "ADDiAX", DIS_I16);

	| CMPiAL(i8) =>
		stmts = instantiate(pc, "CMPiAL", DIS_I8);

	| XORiAL(i8) =>
		stmts = instantiate(pc, "XORiAL", DIS_I8);

	| SUBiAL(i8) =>
		stmts = instantiate(pc, "SUBiAL", DIS_I8);

	| ANDiAL(i8) =>
		stmts = instantiate(pc, "ANDiAL", DIS_I8);

	| SBBiAL(i8) =>
		stmts = instantiate(pc, "SBBiAL", DIS_I8);

	| ADCiAL(i8) =>
		stmts = instantiate(pc, "ADCiAL", DIS_I8);

	| ORiAL(i8) =>
		stmts = instantiate(pc, "ORiAL", DIS_I8);

	| ADDiAL(i8) =>
		stmts = instantiate(pc, "ADDiAL", DIS_I8);

	| LODSvod() =>
		stmts = instantiate(pc, "LODSvod");

	| LODSvow() =>
		stmts = instantiate(pc, "LODSvow");

	| LODSB() =>
		stmts = instantiate(pc, "LODSB");

	/* Floating point instructions */
	| F2XM1() =>
		stmts = instantiate(pc, "F2XM1");

	| FABS() =>
		stmts = instantiate(pc, "FABS");

	| FADD.R32(Mem32) =>
		stmts = instantiate(pc, "FADD.R32", DIS_MEM32);

	| FADD.R64(Mem64) =>
		stmts = instantiate(pc, "FADD.R64", DIS_MEM64);

	| FADD.ST.STi(idx) =>
		stmts = instantiate(pc, "FADD.St.STi", DIS_IDX);

	| FADD.STi.ST(idx) =>
		stmts = instantiate(pc, "FADD.STi.ST", DIS_IDX);

	| FADDP.STi.ST(idx) =>
		stmts = instantiate(pc, "FADDP.STi.ST", DIS_IDX);

	| FIADD.I32(Mem32) =>
		stmts = instantiate(pc, "FIADD.I32", DIS_MEM32);

	| FIADD.I16(Mem16) =>
		stmts = instantiate(pc, "FIADD.I16", DIS_MEM16);

	| FBLD(Mem80) =>
		stmts = instantiate(pc, "FBLD", DIS_MEM80);

	| FBSTP(Mem80) =>
		stmts = instantiate(pc, "FBSTP", DIS_MEM80);

	| FCHS() =>
		stmts = instantiate(pc, "FCHS");

	| FNCLEX() =>
		stmts = instantiate(pc, "FNCLEX");

	| FCOM.R32(Mem32) =>
		stmts = instantiate(pc, "FCOM.R32", DIS_MEM32);

	| FCOM.R64(Mem64) =>
		stmts = instantiate(pc, "FCOM.R64", DIS_MEM64);

	| FICOM.I32(Mem32) =>
		stmts = instantiate(pc, "FICOM.I32", DIS_MEM32);

	| FICOM.I16(Mem16) =>
		stmts = instantiate(pc, "FICOM.I16", DIS_MEM16);

	| FCOMP.R32(Mem32) =>
		stmts = instantiate(pc, "FCOMP.R32", DIS_MEM32);

	| FCOMP.R64(Mem64) =>
		stmts = instantiate(pc, "FCOMP.R64", DIS_MEM64);

	| FCOM.ST.STi(idx) =>
		stmts = instantiate(pc, "FCOM.ST.STi", DIS_IDX);

	| FCOMP.ST.STi(idx) =>
		stmts = instantiate(pc, "FCOMP.ST.STi", DIS_IDX);

	| FICOMP.I32(Mem32) =>
		stmts = instantiate(pc, "FICOMP.I32", DIS_MEM32);

	| FICOMP.I16(Mem16) =>
		stmts = instantiate(pc, "FICOMP.I16", DIS_MEM16);

	| FCOMPP() =>
		stmts = instantiate(pc, "FCOMPP");

	| FCOMI.ST.STi(idx) [name] =>
		stmts = instantiate(pc, name, DIS_IDX);

	| FCOMIP.ST.STi(idx) [name] =>
		stmts = instantiate(pc, name, DIS_IDX);

	| FCOS() =>
		stmts = instantiate(pc, "FCOS");

	| FDECSTP() =>
		stmts = instantiate(pc, "FDECSTP");

	| FDIV.R32(Mem32) =>
		stmts = instantiate(pc, "FDIV.R32", DIS_MEM32);

	| FDIV.R64(Mem64) =>
		stmts = instantiate(pc, "FDIV.R64", DIS_MEM64);

	| FDIV.ST.STi(idx) =>
		stmts = instantiate(pc, "FDIV.ST.STi", DIS_IDX);

	| FDIV.STi.ST(idx) =>
		stmts = instantiate(pc, "FDIV.STi.ST", DIS_IDX);

	| FDIVP.STi.ST(idx) =>
		stmts = instantiate(pc, "FDIVP.STi.ST", DIS_IDX);

	| FIDIV.I32(Mem32) =>
		stmts = instantiate(pc, "FIDIV.I32", DIS_MEM32);

	| FIDIV.I16(Mem16) =>
		stmts = instantiate(pc, "FIDIV.I16", DIS_MEM16);

	| FDIVR.R32(Mem32) =>
		stmts = instantiate(pc, "FDIVR.R32", DIS_MEM32);

	| FDIVR.R64(Mem64) =>
		stmts = instantiate(pc, "FDIVR.R64", DIS_MEM64);

	| FDIVR.ST.STi(idx) =>
		stmts = instantiate(pc, "FDIVR.ST.STi", DIS_IDX);

	| FDIVR.STi.ST(idx) =>
		stmts = instantiate(pc, "FDIVR.STi.ST", DIS_IDX);

	| FIDIVR.I32(Mem32) =>
		stmts = instantiate(pc, "FIDIVR.I32", DIS_MEM32);

	| FIDIVR.I16(Mem16) =>
		stmts = instantiate(pc, "FIDIVR.I16", DIS_MEM16);

	| FDIVRP.STi.ST(idx) =>
		stmts = instantiate(pc, "FDIVRP.STi.ST", DIS_IDX);

	| FFREE(idx) =>
		stmts = instantiate(pc, "FFREE", DIS_IDX);

	| FILD.lsI16(Mem16) =>
		stmts = instantiate(pc, "FILD.lsI16", DIS_MEM16);

	| FILD.lsI32(Mem32) =>
		stmts = instantiate(pc, "FILD.lsI32", DIS_MEM32);

	| FILD64(Mem64) =>
		stmts = instantiate(pc, "FILD.lsI64", DIS_MEM64);

	| FINIT() =>
		stmts = instantiate(pc, "FINIT");

	| FIST.lsI16(Mem16) =>
		stmts = instantiate(pc, "FIST.lsI16", DIS_MEM16);

	| FIST.lsI32(Mem32) =>
		stmts = instantiate(pc, "FIST.lsI32", DIS_MEM32);

	| FISTP.lsI16(Mem16) =>
		stmts = instantiate(pc, "FISTP.lsI16", DIS_MEM16);

	| FISTP.lsI32(Mem32) =>
		stmts = instantiate(pc, "FISTP.lsI32", DIS_MEM32);

	| FISTP64(Mem64) =>
		stmts = instantiate(pc, "FISTP64", DIS_MEM64);

	| FLD.lsR32(Mem32) =>
		stmts = instantiate(pc, "FLD.lsR32", DIS_MEM32);

	| FLD.lsR64(Mem64) =>
		stmts = instantiate(pc, "FLD.lsR64", DIS_MEM64);

	| FLD80(Mem80) =>
		stmts = instantiate(pc, "FLD80", DIS_MEM80);

/* This is a bit tricky. The FPUSH logically comes between the read of STi and
 * the write to ST0. In particular, FLD ST0 is supposed to duplicate the TOS.
 * This problem only happens with this load instruction, so there is a work
 * around here that gives us the SSL a value of i that is one more than in
 * the instruction */
	| FLD.STi(idx) =>
		stmts = instantiate(pc, "FLD.STi", DIS_IDXP1);

	| FLD1() =>
		stmts = instantiate(pc, "FLD1");

	| FLDL2T() =>
		stmts = instantiate(pc, "FLDL2T");

	| FLDL2E() =>
		stmts = instantiate(pc, "FLDL2E");

	| FLDPI() =>
		stmts = instantiate(pc, "FLDPI");

	| FLDLG2() =>
		stmts = instantiate(pc, "FLDLG2");

	| FLDLN2() =>
		stmts = instantiate(pc, "FLDLN2");

	| FLDZ() =>
		stmts = instantiate(pc, "FLDZ");

	| FLDCW(Mem16) =>
		stmts = instantiate(pc, "FLDCW", DIS_MEM16);

	| FLDENV(Mem) =>
		stmts = instantiate(pc, "FLDENV", DIS_MEM);

	| FMUL.R32(Mem32) =>
		stmts = instantiate(pc, "FMUL.R32", DIS_MEM32);

	| FMUL.R64(Mem64) =>
		stmts = instantiate(pc, "FMUL.R64", DIS_MEM64);

	| FMUL.ST.STi(idx) =>
		stmts = instantiate(pc, "FMUL.ST.STi", DIS_IDX);

	| FMUL.STi.ST(idx) =>
		stmts = instantiate(pc, "FMUL.STi.ST", DIS_IDX);

	| FMULP.STi.ST(idx) =>
		stmts = instantiate(pc, "FMULP.STi.ST", DIS_IDX);

	| FIMUL.I32(Mem32) =>
		stmts = instantiate(pc, "FIMUL.I32", DIS_MEM32);

	| FIMUL.I16(Mem16) =>
		stmts = instantiate(pc, "FIMUL.I16", DIS_MEM16);

	| FNOP() =>
		stmts = instantiate(pc, "FNOP");

	| FPATAN() =>
		stmts = instantiate(pc, "FPATAN");

	| FPREM() =>
		stmts = instantiate(pc, "FPREM");

	| FPREM1() =>
		stmts = instantiate(pc, "FPREM1");

	| FPTAN() =>
		stmts = instantiate(pc, "FPTAN");

	| FRNDINT() =>
		stmts = instantiate(pc, "FRNDINT");

	| FRSTOR(Mem) =>
		stmts = instantiate(pc, "FRSTOR", DIS_MEM);

	| FNSAVE(Mem) =>
		stmts = instantiate(pc, "FNSAVE", DIS_MEM);

	| FSCALE() =>
		stmts = instantiate(pc, "FSCALE");

	| FSIN() =>
		stmts = instantiate(pc, "FSIN");

	| FSINCOS() =>
		stmts = instantiate(pc, "FSINCOS");

	| FSQRT() =>
		stmts = instantiate(pc, "FSQRT");

	| FST.lsR32(Mem32) =>
		stmts = instantiate(pc, "FST.lsR32", DIS_MEM32);

	| FST.lsR64(Mem64) =>
		stmts = instantiate(pc, "FST.lsR64", DIS_MEM64);

	| FSTP.lsR32(Mem32) =>
		stmts = instantiate(pc, "FSTP.lsR32", DIS_MEM32);

	| FSTP.lsR64(Mem64) =>
		stmts = instantiate(pc, "FSTP.lsR64", DIS_MEM64);

	| FSTP80(Mem80) =>
		stmts = instantiate(pc, "FSTP80", DIS_MEM80);

	| FST.st.STi(idx) =>
		stmts = instantiate(pc, "FST.st.STi", DIS_IDX);

	| FSTP.st.STi(idx) =>
		stmts = instantiate(pc, "FSTP.st.STi", DIS_IDX);

	| FSTCW(Mem16) =>
		stmts = instantiate(pc, "FSTCW", DIS_MEM16);

	| FSTENV(Mem) =>
		stmts = instantiate(pc, "FSTENV", DIS_MEM);

	| FSTSW(Mem16) =>
		stmts = instantiate(pc, "FSTSW", DIS_MEM16);

	| FSTSW.AX() =>
		stmts = instantiate(pc, "FSTSW.AX");

	| FSUB.R32(Mem32) =>
		stmts = instantiate(pc, "FSUB.R32", DIS_MEM32);

	| FSUB.R64(Mem64) =>
		stmts = instantiate(pc, "FSUB.R64", DIS_MEM64);

	| FSUB.ST.STi(idx) =>
		stmts = instantiate(pc, "FSUB.ST.STi", DIS_IDX);

	| FSUB.STi.ST(idx) =>
		stmts = instantiate(pc, "FSUB.STi.ST", DIS_IDX);

	| FISUB.I32(Mem32) =>
		stmts = instantiate(pc, "FISUB.I32", DIS_MEM32);

	| FISUB.I16(Mem16) =>
		stmts = instantiate(pc, "FISUB.I16", DIS_MEM16);

	| FSUBP.STi.ST(idx) =>
		stmts = instantiate(pc, "FSUBP.STi.ST", DIS_IDX);

	| FSUBR.R32(Mem32) =>
		stmts = instantiate(pc, "FSUBR.R32", DIS_MEM32);

	| FSUBR.R64(Mem64) =>
		stmts = instantiate(pc, "FSUBR.R64", DIS_MEM64);

	| FSUBR.ST.STi(idx) =>
		stmts = instantiate(pc, "FSUBR.ST.STi", DIS_IDX);

	| FSUBR.STi.ST(idx) =>
		stmts = instantiate(pc, "FSUBR.STi.ST", DIS_IDX);

	| FISUBR.I32(Mem32) =>
		stmts = instantiate(pc, "FISUBR.I32", DIS_MEM32);

	| FISUBR.I16(Mem16) =>
		stmts = instantiate(pc, "FISUBR.I16", DIS_MEM16);

	| FSUBRP.STi.ST(idx) =>
		stmts = instantiate(pc, "FSUBRP.STi.ST", DIS_IDX);

	| FTST() =>
		stmts = instantiate(pc, "FTST");

	| FUCOM(idx) =>
		stmts = instantiate(pc, "FUCOM", DIS_IDX);

	| FUCOMP(idx) =>
		stmts = instantiate(pc, "FUCOMP", DIS_IDX);

	| FUCOMPP() =>
		stmts = instantiate(pc, "FUCOMPP");

	| FUCOMI.ST.STi(idx) [name] =>
		stmts = instantiate(pc, name, DIS_IDX);

	| FUCOMIP.ST.STi(idx) [name] =>
		stmts = instantiate(pc, name, DIS_IDX);

	| FXAM() =>
		stmts = instantiate(pc, "FXAM");

	| FXCH(idx) =>
		stmts = instantiate(pc, "FXCH", DIS_IDX);

	| FXTRACT() =>
		stmts = instantiate(pc, "FXTRACT");

	| FYL2X() =>
		stmts = instantiate(pc, "FYL2X");

	| FYL2XP1() =>
		stmts = instantiate(pc, "FYL2XP1");

	else
		result.valid = false;  // Invalid instruction
		result.rtl = nullptr;
		result.numBytes = 0;
		return result;
	endmatch

	if (!result.rtl)
		result.rtl = new RTL(pc, stmts);
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
static void
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

	auto stmts = new std::list<Statement *>;
	Statement *s;
	BranchStatement *b;
	switch (BSFRstate) {
	case 0:
		s = new Assign(new IntegerType(1),
		               new Terminal(opZF),
		               new Const(1));
		stmts->push_back(s);
		b = new BranchStatement;
		b->setDest(pc + numBytes);
		b->setCondType(BRANCH_JE);
		b->setCondExpr(new Binary(opEquals,
		                          modrm->clone(),
		                          new Const(0)));
		stmts->push_back(b);
		break;
	case 1:
		s = new Assign(new IntegerType(1),
		               new Terminal(opZF),
		               new Const(0));
		stmts->push_back(s);
		s = new Assign(new IntegerType(size),
		               dest->clone(),
		               new Const(init));
		stmts->push_back(s);
		break;
	case 2:
		s = new Assign(new IntegerType(size),
		               dest->clone(),
		               new Binary(incdec,
		                          dest->clone(),
		                          new Const(1)));
		stmts->push_back(s);
		b = new BranchStatement;
		b->setDest(pc + 2);
		b->setCondType(BRANCH_JE);
		b->setCondExpr(new Binary(opEquals,
		                          new Ternary(opAt,
		                                      modrm->clone(),
		                                      dest->clone(),
		                                      dest->clone()),
		                          new Const(0)));
		stmts->push_back(b);
		break;
	default:
		// Should never happen
		assert(BSFRstate - BSFRstate);
	}
	result.rtl = new RTL(pc + BSFRstate, stmts);
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
}

Exp *
PentiumDecoder::addReloc(Exp *e)
{
	if (lastDwordLc != (unsigned)-1)
		e = prog->addReloc(e, lastDwordLc);
	return e;
}
