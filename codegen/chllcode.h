/*
 * \file
 * \brief Concrete class for the "C" high level language.
 *
 * This class provides methods which are specific for the C language binding.
 * I guess this will be the most popular output language unless we do C++.
 *
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef CHLLCODE_H
#define CHLLCODE_H

#include "hllcode.h"

#include <ostream>
#include <sstream>
#include <list>
#include <map>
#include <set>
#include <string>

class Assign;
class Exp;
class Proc;

// Operator precedence
/*
Operator Name               Associativity   Operators
Primary scope resolution    left to right   ::
Primary                     left to right   ()  [ ]  .  -> dynamic_cast typeid
Unary                       right to left   ++  --  +  -  !  ~  &  *
                                            (type_name)  sizeof new delete
C++ Pointer to Member       left to right   .* ->*
Multiplicative              left to right   *  /  %
Additive                    left to right   +  -
Bitwise Shift               left to right   <<  >>
Relational                  left to right   <  >  <=  >=
Equality                    left to right   ==  !=
Bitwise AND                 left to right   &
Bitwise Exclusive OR        left to right   ^
Bitwise Inclusive OR        left to right   |
Logical AND                 left to right   &&
Logical OR                  left to right   ||
Conditional                 right to left   ? :
Assignment                  right to left   =  +=  -=  *=   /=  <<=  >>=  %=
                                            &=  ^=  |=
Comma                       left to right   ,
*/

/// Operator precedence
enum PREC {
	PREC_NONE = 0,          ///< Outer level (no parens required)
	PREC_COMMA,             ///< Comma
	PREC_ASSIGN,            ///< Assignment
	PREC_COND,              ///< Conditional
	PREC_LOG_OR,            ///< Logical OR
	PREC_LOG_AND,           ///< Logical AND
	PREC_BIT_IOR,           ///< Bitwise Inclusive OR
	PREC_BIT_XOR,           ///< Bitwise Exclusive OR
	PREC_BIT_AND,           ///< Bitwise AND
	PREC_EQUAL,             ///< Equality
	PREC_REL,               ///< Relational
	PREC_BIT_SHIFT,         ///< Bitwise Shift
	PREC_ADD,               ///< Additive
	PREC_MULT,              ///< Multiplicative
	PREC_PTR_MEM,           ///< C++ Pointer to Member
	PREC_UNARY,             ///< Unary
	PREC_PRIM,              ///< Primary
	PREC_SCOPE              ///< Primary scope resolution
};

/// Outputs C code.
class CHLLCode : public HLLCode {
private:
	/// The generated code.
	std::list<std::string> lines;

	static void indent(std::ostringstream &str, int indLevel);
	void appendExp(std::ostringstream &str, Exp *exp, PREC curPrec, bool uns = false) const;
	static void appendType(std::ostringstream &str, Type *typ);
	static void appendTypeIdent(std::ostringstream &str, Type *typ, const char *ident);
	/// Adds: (
	static void openParen(std::ostringstream &str, PREC outer, PREC inner) {
		if (inner < outer) str << "(";
	}
	/// Adds: )
	static void closeParen(std::ostringstream &str, PREC outer, PREC inner) {
		if (inner < outer) str << ")";
	}

	void appendLine(const std::ostringstream &ostr);
	void appendLine(const std::string &s);

	/// All locals in a Proc
	std::map<std::string, Type *> locals;

	/// All used goto labels.
	std::set<int> usedLabels;

public:
	// constructor
	CHLLCode() = default;
	CHLLCode(UserProc *p);

	// clear this class, calls the base
	void reset() override;

	/*
	 * Functions to add new code
	 */

	// pretested loops (cond is optional because it is in the bb [somewhere])
	void AddPretestedLoopHeader(int indLevel, Exp *cond) override;
	void AddPretestedLoopEnd(int indLevel) override;

	// endless loops
	void AddEndlessLoopHeader(int indLevel) override;
	void AddEndlessLoopEnd(int indLevel) override;

	// posttested loops
	void AddPosttestedLoopHeader(int indLevel) override;
	void AddPosttestedLoopEnd(int indLevel, Exp *cond) override;

	// case conditionals "nways"
	void AddCaseCondHeader(int indLevel, Exp *cond) override;
	void AddCaseCondOption(int indLevel, Exp *opt) override;
	void AddCaseCondOptionEnd(int indLevel) override;
	void AddCaseCondElse(int indLevel) override;
	void AddCaseCondEnd(int indLevel) override;

	// if conditions
	void AddIfCondHeader(int indLevel, Exp *cond) override;
	void AddIfCondEnd(int indLevel) override;

	// if else conditions
	void AddIfElseCondHeader(int indLevel, Exp *cond) override;
	void AddIfElseCondOption(int indLevel) override;
	void AddIfElseCondEnd(int indLevel) override;

	// goto, break, continue, etc
	void AddGoto(int indLevel, int ord) override;
	void AddBreak(int indLevel) override;
	void AddContinue(int indLevel) override;

	// labels
	void AddLabel(int indLevel, int ord) override;
	void RemoveLabel(int ord) override;
	void RemoveUnusedLabels(int maxOrd) override;

	// sequential statements
	void AddAssignmentStatement(int indLevel, Assign *asgn) override;
	void AddCallStatement(int indLevel, Proc *proc, const std::string &name, const StatementList &args, StatementList *results) override;
	void AddIndCallStatement(int indLevel, Exp *exp, const StatementList &args, StatementList *results) override;
	void AddReturnStatement(int indLevel, StatementList *rets) override;

	// proc related
	void AddProcStart(UserProc *proc) override;
	void AddProcEnd() override;
	void AddLocal(const std::string &name, Type *type, bool last = false) override;
	void AddGlobal(const std::string &name, Type *type, Exp *init = nullptr) override;
	void AddPrototype(UserProc *proc) override;
private:
	void AddProcDec(UserProc *proc, bool open);  // Implement AddProcStart and AddPrototype
public:

	// comments
	void AddLineComment(const std::string &cmt) override;

	/*
	 * output functions
	 */
	void print(std::ostream &os) const override;
};

#endif
