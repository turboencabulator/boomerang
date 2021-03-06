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

#include "decoder.h"

#include "boomerang.h"
#include "exp.h"

#include <cstdarg>  // For varargs
//#include <cassert>

NJMCDecoder::NJMCDecoder(Prog *prog) :
	prog(prog)
{
}

int
NJMCDecoder::getRegNum(const std::string &name) const
{
	return RTLDict.RegMap.at(name);
}

/**
 * \brief Returns a symbolic name for a register index.
 */
const char *
NJMCDecoder::getRegName(int idx) const
{
	for (const auto &reg : RTLDict.RegMap)
		if (reg.second == idx)
			return reg.first.c_str();
	return nullptr;
}

int
NJMCDecoder::getRegSize(int idx) const
{
	return RTLDict.DetRegMap.at(idx).g_size();
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
RTL *
NJMCDecoder::instantiate(ADDRESS pc, const std::string &name, ...)
{
	// Get the signature of the instruction and extract its parts
	auto sig = RTLDict.getSignature(name);
	auto &opcode = sig.first;
	auto actuals = std::vector<Exp *>(sig.second);

	// Put the operands into a vector
	va_list ap;
	va_start(ap, name);
	for (auto &operand : actuals)
		operand = va_arg(ap, Exp *);
	va_end(ap);

	if (DEBUG_DECODER) {
		// Display a disassembly of this instruction if requested
		std::cout << std::hex << pc << std::dec << ": " << name << " ";
		bool first = true;
		for (const auto &operand : actuals) {
			if (first)
				first = false;
			else
				std::cout << ", ";

			if (operand->isIntConst()) {
				int val = ((Const *)operand)->getInt();
				if (val > 100 || val < -100)
					std::cout << std::hex << "0x" << val << std::dec;
				else
					std::cout << val;
			} else
				operand->print(std::cout);
		}
		std::cout << std::endl;
	}

	return RTLDict.instantiateRTL(pc, opcode, actuals);
}

#if 0 // Cruft?  Used by hppa, probably obsoleted by the above.
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
Exp *
NJMCDecoder::instantiateNamedParam(const std::string &name, ...)
{
	if (!RTLDict.ParamSet.count(name)) {
		std::cerr << "No entry for named parameter '" << name << "'\n";
		return nullptr;
	}
	auto it = RTLDict.DetParamMap.find(name);
	assert(it != RTLDict.DetParamMap.end());
	auto &ent = it->second;
	if (ent.kind != ParamEntry::ASGN && ent.kind != ParamEntry::LAMBDA) {
		std::cerr << "Attempt to instantiate expressionless parameter '" << name << "'\n";
		return nullptr;
	}
	// Start with the RHS
	auto as = dynamic_cast<Assign *>(ent.asgn);
	assert(as);
	auto result = as->getRight()->clone();

	va_list args;
	va_start(args, name);
	for (const auto &param : ent.params) {
		auto formal = Location::param(param);
		auto actual = va_arg(args, Exp *);
		bool change;
		result = result->searchReplaceAll(formal, actual, change);
		delete formal;
	}
	va_end(args);
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
void
NJMCDecoder::substituteCallArgs(const std::string &name, Exp *&exp, ...)
{
	if (!RTLDict.ParamSet.count(name)) {
		std::cerr << "No entry for named parameter '" << name << "'\n";
		return;
	}
	auto &ent = RTLDict.DetParamMap[name];
#if 0
	if (ent.kind != ParamEntry::ASGN && ent.kind != ParamEntry::LAMBDA) {
		std::cerr << "Attempt to instantiate expressionless parameter '" << name << "'\n";
		return;
	}
#endif

	va_list args;
	va_start(args, exp);
	for (const auto &param : ent.funcParams) {
		auto formal = Location::param(param);
		auto actual = va_arg(args, Exp *);
		bool change;
		exp = exp->searchReplaceAll(formal, actual, change);
		delete formal;
	}
	va_end(args);
}
#endif

/**
 * Resets the fields of a DecodeResult to their default values.
 */
void
DecodeResult::reset()
{
	numBytes = 0;
	type = NCT;
	valid = true;
	rtl = nullptr;
	//reDecode = 0;  // Decoder will use this as a state variable
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
Exp *
NJMCDecoder::dis_Reg(int regNum)
{
	return Location::regOf(regNum);
}

/**
 * \brief Decodes a number.
 *
 * Converts a number to a Exp* expression.
 *
 * \param num  A number.
 * \returns    The Exp* representation of the given number.
 */
Exp *
NJMCDecoder::dis_Num(unsigned num)
{
	return new Const((int)num);
}

/**
 * Process an unconditional jump instruction.
 * Also check if the destination is a label (MVE: is this done?)
 *
 * \note This used to be the UNCOND_JUMP macro; it's extended to handle jumps
 * to other procedures.
 */
RTL *
NJMCDecoder::unconditionalJump(ADDRESS pc, const std::string &name, ADDRESS dest)
{
	auto jump = new GotoStatement(dest);
	SHOW_ASM(name << " 0x" << std::hex << dest << std::dec);
	return new RTL(pc, jump);
}

/**
 * Process a conditional jump instruction.
 *
 * \note This used to be the COND_JUMP macro.
 */
RTL *
NJMCDecoder::conditionalJump(ADDRESS pc, const std::string &name, ADDRESS dest, BRANCH_TYPE cond)
{
	auto jump = new BranchStatement(dest);
	jump->setCondType(cond);
	SHOW_ASM(name << " 0x" << std::hex << dest << std::dec);
	return new RTL(pc, jump);
}

/**
 * Process an indirect jump instruction.
 *
 * \param pc      Native pc.
 * \param name    Name of instruction (for debugging).
 * \param dest    Destination Exp*.
 */
RTL *
NJMCDecoder::computedJump(ADDRESS pc, const std::string &name, Exp *dest)
{
	auto jump = new GotoStatement(dest);
	jump->setIsComputed();
	SHOW_ASM(name << " " << *dest);
	return new RTL(pc, jump);
}

/**
 * Process an indirect call instruction.
 *
 * \param pc      Native pc.
 * \param name    Name of instruction (for debugging).
 * \param dest    Destination Exp*.
 */
RTL *
NJMCDecoder::computedCall(ADDRESS pc, const std::string &name, Exp *dest)
{
	auto call = new CallStatement(dest);
	call->setIsComputed();
	SHOW_ASM(name << " " << *dest);
	return new RTL(pc, call);
}
