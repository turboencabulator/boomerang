/**
 * \file
 * \brief Contains the machine independent decoding functionality.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "boomerang.h"
#include "decoder.h"
#include "exp.h"
#include "rtl.h"
#include "statement.h"

#include <cstdarg>  // For varargs
#include <cassert>

NJMCDecoder::NJMCDecoder(Prog *prog) :
	prog(prog)
{
}

/**
 * Given an instruction name and a variable list of Exps representing the
 * actual operands of the instruction, use the RTL template dictionary to
 * return the list of Statements representing the semantics of the
 * instruction. This method also displays a disassembly of the instruction if
 * the relevant compilation flag has been set.
 *
 * \param pc    Native PC.
 * \param name  Instruction name.
 * \param ...   Semantic String ptrs representing actual operands.
 *
 * \returns An instantiated list of Exps.
 */
std::list<Statement *> *NJMCDecoder::instantiate(ADDRESS pc, const char *name, ...)
{
	// Get the signature of the instruction and extract its parts
	std::pair<std::string, unsigned> sig = RTLDict.getSignature(name);
	std::string opcode = sig.first;
	unsigned numOperands = sig.second;

	// Put the operands into a vector
	std::vector<Exp *> actuals(numOperands);
	va_list args;
	va_start(args, name);
	for (unsigned i = 0; i < numOperands; i++)
		actuals[i] = va_arg(args, Exp *);
	va_end(args);

	if (DEBUG_DECODER) {
		// Display a disassembly of this instruction if requested
		std::cout << std::hex << pc << std::dec << ": " << name << " ";
		for (std::vector<Exp *>::iterator itd = actuals.begin(); itd != actuals.end(); itd++) {
			if ((*itd)->isIntConst()) {
				int val = ((Const *)(*itd))->getInt();
				if (val > 100 || val < -100)
					std::cout << std::hex << "0x" << val << std::dec;
				else
					std::cout << val;
			} else
				(*itd)->print(std::cout);
			if (itd != actuals.end() - 1)
				std::cout << ", ";
		}
		std::cout << std::endl;
	}

	std::list<Statement *> *instance = RTLDict.instantiateRTL(opcode, pc, actuals);

	return instance;
}

/**
 * Similarly to the above, given a parameter name and a list of Exp*'s
 * representing sub-parameters, return a fully substituted Exp for the whole
 * expression.
 *
 * \note Caller must delete result.
 *
 * \param name  Parameter name.
 * \param ...   Exp* representing actual operands.
 *
 * \returns An instantiated list of Exps.
 */
Exp *NJMCDecoder::instantiateNamedParam(const char *name, ...)
{
	if (RTLDict.ParamSet.find(name) == RTLDict.ParamSet.end()) {
		std::cerr << "No entry for named parameter '" << name << "'\n";
		return 0;
	}
	assert(RTLDict.DetParamMap.find(name) != RTLDict.DetParamMap.end());
	ParamEntry &ent = RTLDict.DetParamMap[name];
	if (ent.kind != PARAM_ASGN && ent.kind != PARAM_LAMBDA) {
		std::cerr << "Attempt to instantiate expressionless parameter '" << name << "'\n";
		return 0;
	}
	// Start with the RHS
	assert(ent.asgn->getKind() == STMT_ASSIGN);
	Exp *result = ((Assign *)ent.asgn)->getRight()->clone();

	va_list args;
	va_start(args, name);
	for (std::list<std::string>::iterator it = ent.params.begin(); it != ent.params.end(); it++) {
		Exp *formal = new Location(opParam, new Const(it->c_str()), NULL);
		Exp *actual = va_arg(args, Exp *);
		bool change;
		result = result->searchReplaceAll(formal, actual, change);
		delete formal;
	}
	return result;
}

/**
 * In the event that it's necessary to synthesize the call of a named
 * parameter generated with instantiateNamedParam(), this method will
 * substitute the arguments that follow into the expression.
 *
 * \note Should only be used after instantiateNamedParam(name, ...);
 *
 * \note exp (the pointer) could be changed.
 *
 * \param name  Parameter name.
 * \param exp   Expression to instantiate into.
 * \param ...   Exp* representing actual operands.
 *
 * \returns an instantiated list of Exps.
 */
void NJMCDecoder::substituteCallArgs(const char *name, Exp *&exp, ...)
{
	if (RTLDict.ParamSet.find(name) == RTLDict.ParamSet.end()) {
		std::cerr << "No entry for named parameter '" << name << "'\n";
		return;
	}
	ParamEntry &ent = RTLDict.DetParamMap[name];
#if 0
	if (ent.kind != PARAM_ASGN && ent.kind != PARAM_LAMBDA) {
		std::cerr << "Attempt to instantiate expressionless parameter '" << name << "'\n";
		return;
	}
#endif

	va_list args;
	va_start(args, exp);
	for (std::list<std::string>::iterator it = ent.funcParams.begin(); it != ent.funcParams.end(); it++) {
		Exp *formal = new Location(opParam, new Const(it->c_str()), NULL);
		Exp *actual = va_arg(args, Exp *);
		bool change;
		exp = exp->searchReplaceAll(formal, actual, change);
		delete formal;
	}
}

/**
 * Resets the fields of a DecodeResult to their default values.
 */
void DecodeResult::reset()
{
	numBytes = 0;
	type = NCT;
	valid = true;
	rtl = NULL;
	reDecode = false;
	forceOutEdge = 0;
}

/**
 * \brief Decodes a register.
 *
 * Converts a numbered register to a suitable expression.
 *
 * \param regNum  The register number, e.g. 0 for eax.
 * \returns       The Exp* for the register NUMBER (e.g. "int 36" for %f4).
 */
Exp *NJMCDecoder::dis_Reg(int regNum)
{
	Exp *expr = Location::regOf(regNum);
	return expr;
}

/**
 * \brief Decodes a number.
 *
 * Converts a number to a Exp* expression.
 *
 * \param num  A number.
 * \returns    The Exp* representation of the given number.
 */
Exp *NJMCDecoder::dis_Num(unsigned num)
{
	Exp *expr = new Const((int)num);
	return expr;
}

/**
 * Process an unconditional jump instruction.
 * Also check if the destination is a label (MVE: is this done?)
 *
 * \note This used to be the UNCOND_JUMP macro; it's extended to handle jumps
 * to other procedures.
 */
void NJMCDecoder::unconditionalJump(const char *name, int size, ADDRESS relocd, int delta, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result)
{
	result.rtl = new RTL(pc, stmts);
	result.numBytes = size;
	GotoStatement *jump = new GotoStatement();
	jump->setDest(relocd - delta);
	result.rtl->appendStmt(jump);
	SHOW_ASM(name << " 0x" << std::hex << relocd - delta)
}

/**
 * Process an indirect jump instruction.
 *
 * \param name    Name of instruction (for debugging).
 * \param size    Size of instruction in bytes.
 * \param dest    Destination Exp*.
 * \param pc      Native pc.
 * \param stmts   List of statements (?)
 * \param result  Ref to decoder result object.
 */
void NJMCDecoder::computedJump(const char *name, int size, Exp *dest, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result)
{
	result.rtl = new RTL(pc, stmts);
	result.numBytes = size;
	GotoStatement *jump = new GotoStatement();
	jump->setDest(dest);
	jump->setIsComputed(true);
	result.rtl->appendStmt(jump);
	SHOW_ASM(name << " " << dest)
}

/**
 * Process an indirect call instruction.
 *
 * \param name    Name of instruction (for debugging).
 * \param size    Size of instruction in bytes.
 * \param dest    Destination Exp*.
 * \param pc      Native pc.
 * \param stmts   List of statements (?)
 * \param result  Ref to decoder result object.
 */
void NJMCDecoder::computedCall(const char *name, int size, Exp *dest, ADDRESS pc, std::list<Statement *> *stmts, DecodeResult &result)
{
	result.rtl = new RTL(pc, stmts);
	result.numBytes = size;
	CallStatement *call = new CallStatement();
	call->setDest(dest);
	call->setIsComputed(true);
	result.rtl->appendStmt(call);
	SHOW_ASM(name << " " << dest)
}
