/**
 * \file
 * \brief The interface to the instruction decoder.
 *
 * \authors
 * Copyright (C) 1996-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef DECODER_H
#define DECODER_H

#include "rtl.h"
#include "statement.h"
#include "types.h"

#include <iostream>
#include <list>

#include <cstddef>
#include <cstdint>

class Exp;
class Prog;


/**
 * These are the instruction classes defined in "A Transformational Approach
 * to Binary Translation of Delayed Branches" for SPARC instructions.
 * Extended for HPPA.  Ignored by machines with no delay slots.
 */
enum ICLASS {
	NCT,            ///< Non Control Transfer.
	SD,             ///< Static Delayed.
	DD,             ///< Dynamic Delayed.
	SCD,            ///< Static Conditional Delayed.
	SCDAN,          ///< Static Conditional Delayed, Anulled if Not taken.
	SCDAT,          ///< Static Conditional Delayed, Anulled if Taken.
	SU,             ///< Static Unconditional (not delayed).
	SKIP,           ///< Skip successor.
	//TRAP,           ///< Trap.
	NOP,            ///< No operation (e.g. sparc BN,A).
	// HPPA only
	DU,             ///< Dynamic Unconditional (not delayed).
	NCTA            ///< Non Control Transfer, with following instr Anulled.
};


/**
 * The DecodeResult struct contains all the information that results from
 * calling the decoder.  This prevents excessive use of confusing reference
 * parameters.
 */
struct DecodeResult {
	void reset();

	/**
	 * The number of bytes decoded in the main instruction.
	 */
	int numBytes;

	/**
	 * The RTL constructed (if any).
	 */
	RTL *rtl;

	/**
	 * Indicates whether or not a valid instruction was decoded.
	 */
	bool valid;

	/**
	 * The class of the instruction decoded.  Will be one of the classes
	 * described in "A Transformational Approach to Binary Translation of
	 * Delayed Branches" (plus two more HPPA specific entries).  Ignored
	 * by machines with no delay slots.
	 */
	ICLASS type;

	/**
	 * If true, don't add numBytes and decode there; instead, re-decode
	 * the current instruction.  Needed for instructions like the Pentium
	 * BSF/BSR, which emit branches (so numBytes needs to be carefully set
	 * for the fall through out edge after the branch).
	 */
	bool reDecode;

	/**
	 * If non zero, this field represents a new native address to be used
	 * as the out-edge for this instruction's BB.  At present, only used
	 * for the SPARC call/add caller prologue.
	 */
	ADDRESS forceOutEdge;
};

/**
 * The NJMCDecoder class is a class that contains NJMC generated decoding
 * methods.
 */
class NJMCDecoder {
protected:
	Prog *prog;
public:
	NJMCDecoder(Prog *prog);
	virtual ~NJMCDecoder() { };

	/**
	 * Decodes the machine instruction at pc and returns an RTL instance
	 * for the instruction.
	 */
	virtual DecodeResult &decodeInstruction(ADDRESS pc, ptrdiff_t delta) = 0;

#if 0 // Cruft?
	/**
	 * Disassembles the machine instruction at pc and returns the number
	 * of bytes disassembled.  Assembler output goes to global _assembly.
	 */
	virtual int decodeAssemblyInstruction(ADDRESS pc, ptrdiff_t delta) = 0;
#endif

	RTLInstDict &getRTLDict() { return RTLDict; }
#if 0 // Cruft?
	Prog *getProg() { return prog; }
#endif

protected:
	std::list<Statement *> *instantiate(ADDRESS pc, const char *name, ...);
	Exp *instantiateNamedParam(const char *name, ...);
	void substituteCallArgs(const char *name, Exp *&exp, ...);

	static void unconditionalJump(const char *name, ADDRESS relocd, ptrdiff_t delta, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result);
	static void conditionalJump(const char *name, BRANCH_TYPE cond, ADDRESS relocd, ptrdiff_t delta, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result);
	static void computedJump(const char *name, Exp *dest, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result);
	static void computedCall(const char *name, Exp *dest, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result);

#if 0 // Cruft?
	/**
	 * String for the constructor names (displayed with use "-c").
	 */
	char constrName[84];
#endif

	/**
	 * \name Functions to decode instruction operands into Exp*s
	 * \{
	 */
	static Exp *dis_Num(unsigned num);
	static Exp *dis_Reg(int regNum);
	/** \} */

	/**
	 * Public dictionary of instruction patterns, and other information
	 * summarised from the SSL file (e.g. source machine's endianness).
	 */
	RTLInstDict RTLDict;
};

#if 0 // Cruft?
/**
 * Does the instruction at the given offset correspond to a caller prologue?
 *
 * \note Implemented in the decoder.m files.
 */
bool isFuncPrologue(ADDRESS hostPC);
#endif


/**
 * \name Macros that each of the .m files depend upon
 * \{
 */
#define DEBUG_DECODER (Boomerang::get()->debugDecoder)
#define SHOW_ASM(output) \
	if (DEBUG_DECODER) \
		std::cout << std::hex << pc << std::dec << ": " << output << std::endl;
#define DEBUG_STMTS \
	if (DEBUG_DECODER) { \
		const auto &stmts = result.rtl->getList(); \
		for (const auto &stmt : stmts) \
			std::cout << "\t\t\t" << stmt << "\n"; \
	}
/** \} */

/**
 * addressToPC returns the raw number as the address.  PC could be an
 * abstract type, in our case, PC is the raw address.
 */
#define addressToPC(pc) pc

/**
 * \name Macros for branches
 * \note Don't put inside a "match" statement, since the ordering is changed
 * and multiple copies may be made.
 * \{
 */
/// This one is X86 specific.
#define SETS(name, dest, cond) \
	auto bs = new BoolAssign(8); \
	bs->setLeftFromList(stmts); \
	stmts->clear(); \
	result.rtl = new RTL(pc, stmts); \
	result.rtl->appendStmt(bs); \
	bs->setCondType(cond); \
	SHOW_ASM(name << " " << *dest)
/** \} */


#if 0 // Cruft?
/**
 * \name Arrays used to map register numbers to their names
 * \{
 */
extern char *r32_names[];
extern char *sr16_names[];
extern char *r8_names[];
extern char *r16_names[];
extern char *fp_names[];
/** \} */

/**
 * This array decodes scale field values in an index memory expression to the
 * scale factor they represent.
 */
extern int scale[];


/**
 * \name Fetch routines
 * \{
 */
/**
 * \returns The byte (8 bits) starting at the given address.
 */
uint8_t getByte(ADDRESS lc);

/**
 * \returns The word (16 bits) starting at the given address.
 */
uint16_t getWord(ADDRESS lc);

/**
 * \returns The double (32 bits) starting at the given address.
 */
uint32_t getDword(ADDRESS lc);
/** \} */
#endif

#endif
