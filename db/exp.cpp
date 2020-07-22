/**
 * \file
 * \brief Implementation of the Exp and related classes.
 *
 * \authors
 * Copyright (C) 2002-2006 Mike Van Emmerik and Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exp.h"

#include "boomerang.h"
#include "operstrings.h"
#include "proc.h"
#include "prog.h"
#include "rtl.h"
#include "statement.h"
//#include "transformer.h"
#include "types.h"
#include "visitor.h"

#include <iostream>     // For std::cout, std::cerr
#include <sstream>      // For std::ostringstream
#include <iomanip>      // For std::setw
#include <numeric>      // For std::accumulate()
#include <list>
#include <string>
#include <utility>

#include <cassert>
#include <cstdlib>
#include <cstring>

Const::Const(int i)                : Exp(opIntConst),  type(new VoidType) { u.i  = i;  }
Const::Const(uint64_t ll)          : Exp(opLongConst), type(new VoidType) { u.ll = ll; }
Const::Const(double d)             : Exp(opFltConst),  type(new VoidType) { u.d  = d;  }
Const::Const(const char *p)        : Exp(opStrConst),  type(new VoidType) { u.p  = p;  }
Const::Const(const std::string &s) : Exp(opStrConst),  type(new VoidType) { u.p  = strdup(s.c_str()); }
/// \remark This is bad. We need a way of constructing true unsigned constants
Const::Const(ADDRESS a)            : Exp(opIntConst),  type(new VoidType) { u.a  = a;  }

// Copy constructor
Const::Const(const Const &o) : Exp(o.op) { u = o.u; conscript = o.conscript; type = o.type; }

Terminal::Terminal(OPER op) : Exp(op) { }
Terminal::Terminal(const Terminal &o) : Exp(o.op) { }  // Copy constructor

Unary::Unary(OPER op) :
	Exp(op)
{
	//assert(!isRegOf());
}
Unary::Unary(OPER op, Exp *e) :
	Exp(op)
{
	assert(e);
	subExp1 = e;
}
Unary::Unary(const Unary &o) :
	Exp(o.op)
{
	assert(o.subExp1);
	subExp1 = o.subExp1->clone();
}

Binary::Binary(OPER op) :
	Unary(op)
{
}
Binary::Binary(OPER op, Exp *e1, Exp *e2) :
	Unary(op, e1)
{
	assert(e2);
	subExp2 = e2;
}
Binary::Binary(const Binary &o) :
	Unary(o.op)
{
	assert(o.subExp1 && o.subExp2);
	subExp1 = o.subExp1->clone();
	subExp2 = o.subExp2->clone();
}

Ternary::Ternary(OPER op) :
	Binary(op)
{
}
Ternary::Ternary(OPER op, Exp *e1, Exp *e2, Exp *e3) :
	Binary(op, e1, e2)
{
	assert(e3);
	subExp3 = e3;
}
Ternary::Ternary(const Ternary &o) :
	Binary(o.op)
{
	assert(o.subExp1 && o.subExp2 && o.subExp3);
	subExp1 = o.subExp1->clone();
	subExp2 = o.subExp2->clone();
	subExp3 = o.subExp3->clone();
}

TypedExp::TypedExp() : Unary(opTypedExp) { }
TypedExp::TypedExp(Exp *e1) : Unary(opTypedExp, e1) { }
TypedExp::TypedExp(Type *ty, Exp *e1) : Unary(opTypedExp, e1), type(ty) { }
TypedExp::TypedExp(const TypedExp &o) :
	Unary(opTypedExp)
{
	subExp1 = o.subExp1->clone();
	type = o.type->clone();
}

FlagDef::FlagDef(Exp *params, RTL *rtl) : Unary(opFlagDef, params), rtl(rtl) { }

RefExp::RefExp(Exp *e, Statement *d) :
	Unary(opSubscript, e),
	def(d)
{
	assert(e);
}

TypeVal::TypeVal(Type *ty) : Terminal(opTypeVal), val(ty) { }

/**
 * Create a new Location expression.
 * \param op Should be \opRegOf, opMemOf, opLocal, opGlobal, opParam or opTemp.
 */
Location::Location(OPER op, Exp *exp, UserProc *proc) :
	Unary(op, exp),
	proc(proc)
{
	assert(op == opRegOf || op == opMemOf || op == opLocal || op == opGlobal || op == opParam || op == opTemp);
	if (!proc) {
		// eep.. this almost always causes problems
		Exp *e = exp;
		if (e) {
			bool giveUp = false;
			while (!this->proc && !giveUp) {
				switch (e->getOper()) {
				case opRegOf:
				case opMemOf:
				case opTemp:
				case opLocal:
				case opGlobal:
				case opParam:
					this->proc = ((Location *)e)->getProc();
					giveUp = true;
					break;
				case opSubscript:
					e = e->getSubExp1();
					break;
				default:
					giveUp = true;
					break;
				}
			}
		}
	}
}

Location::Location(const Location &o) :
	Unary(o.op, o.subExp1->clone()),
	proc(o.proc)
{
}

Unary::~Unary()
{
	// Remember to ;//delete all children
	;//delete subExp1;
}
Binary::~Binary()
{
	;//delete subExp2;
	// Note that the first pointer is destructed in the Exp1 destructor
}
Ternary::~Ternary()
{
	;//delete subExp3;
}
FlagDef::~FlagDef()
{
	;//delete rtl;
}
TypeVal::~TypeVal()
{
	;//delete val;
}

/*==============================================================================
 * FUNCTION:        Unary::setSubExp1 etc
 * OVERVIEW:        Set requested subexpression; 1 is first
 * PARAMETERS:      Pointer to subexpression to set
 * NOTE:            If an expression already exists, it is ;//deleted
 *============================================================================*/
void
Unary::setSubExp1(Exp *e)
{
	;//delete subExp1;
	subExp1 = e;
	assert(subExp1);
}
void
Binary::setSubExp2(Exp *e)
{
	;//delete subExp2;
	subExp2 = e;
	assert(subExp1 && subExp2);
}
void
Ternary::setSubExp3(Exp *e)
{
	;//delete subExp3;
	subExp3 = e;
	assert(subExp1 && subExp2 && subExp3);
}

Exp *
Unary::swapSubExp1(Exp *e)
{
	auto ret = subExp1;
	subExp1 = e;
	return ret;
}
Exp *
Binary::swapSubExp2(Exp *e)
{
	auto ret = subExp2;
	subExp2 = e;
	return ret;
}
Exp *
Ternary::swapSubExp3(Exp *e)
{
	auto ret = subExp3;
	subExp3 = e;
	return ret;
}

/*==============================================================================
 * FUNCTION:        Unary::getSubExp1 etc
 * OVERVIEW:        Get subexpression
 * RETURNS:         Pointer to the requested subexpression
 *============================================================================*/
Exp *
Unary::getSubExp1() const
{
	assert(subExp1);
	return subExp1;
}
Exp *
Binary::getSubExp2() const
{
	assert(subExp1 && subExp2);
	return subExp2;
}
Exp *
Ternary::getSubExp3() const
{
	assert(subExp1 && subExp2 && subExp3);
	return subExp3;
}

/**
 * \fn Exp *Exp::clone() const
 * \brief Make copy of self that can be deleted without affecting self.
 *
 * Make a clone of myself, i.e. to create a new Exp with the same contents as
 * myself, but not sharing any memory.  Deleting the clone will not affect
 * this object.  Pointers to subexpressions are not copied, but also cloned.
 *
 * \returns Pointer to cloned object.
 */
Exp *
Const::clone() const
{
	// Note: not actually cloning the Type* type pointer. Probably doesn't matter with GC
	return new Const(*this);
}
Exp *
Terminal::clone() const
{
	return new Terminal(*this);
}
Exp *
Unary::clone() const
{
	return new Unary(*this);
}
Exp *
Binary::clone() const
{
	return new Binary(*this);
}
Exp *
Ternary::clone() const
{
	return new Ternary(*this);
}
Exp *
TypedExp::clone() const
{
	return new TypedExp(type, subExp1->clone());
}
Exp *
RefExp::clone() const
{
	return new RefExp(subExp1->clone(), def);
}
Exp *
TypeVal::clone() const
{
	return new TypeVal(val->clone());
}
Exp *
Location::clone() const
{
	return new Location(op, subExp1->clone(), proc);
}

/**
 * \fn bool Exp::operator ==(const Exp &e) const
 * \brief Type sensitive equality.
 *
 * Compare myself for equality with another Exp.
 *
 * \param[in] e  Ref to other Exp.
 * \returns      true if equal.
 */
bool
Const::operator ==(const Exp &e) const
{
	const Const &o = (const Const &)e;
	if ((o.op == opWild)
	 || (o.op == opWildIntConst && op == opIntConst)
	 || (o.op == opWildStrConst && op == opStrConst))
		return true;
	if (op != o.op)
		return false;
	if ((conscript && conscript != o.conscript) || o.conscript)
		return false;
	switch (op) {
	case opIntConst: return u.i == o.u.i;
	case opFltConst: return u.d == o.u.d;
	case opStrConst: return strcmp(u.p, o.u.p) == 0;
	default:
		LOG << "operator == invalid operator " << operStrings[op] << "\n";
		assert(0);
		return false;
	}
}
bool
Unary::operator ==(const Exp &e) const
{
	const Unary &o = (const Unary &)e;
	if ((o.op == opWild)
	 || (o.op == opWildRegOf  && op == opRegOf)
	 || (o.op == opWildMemOf  && op == opMemOf)
	 || (o.op == opWildAddrOf && op == opAddrOf))
		return true;
	return op == o.op
	    && *subExp1 == *o.subExp1;
}
bool
Binary::operator ==(const Exp &e) const
{
	assert(subExp1 && subExp2);
	const Binary &o = (const Binary &)e;
	if (o.op == opWild)
		return true;
	return op == o.op
	    && *subExp1 == *o.subExp1
	    && *subExp2 == *o.subExp2;
}
bool
Ternary::operator ==(const Exp &e) const
{
	const Ternary &o = (const Ternary &)e;
	if (o.op == opWild)
		return true;
	return op == o.op
	    && *subExp1 == *o.subExp1
	    && *subExp2 == *o.subExp2
	    && *subExp3 == *o.subExp3;
}
bool
Terminal::operator ==(const Exp &e) const
{
	const Terminal &o = (const Terminal &)e;
	if (op == opWildIntConst) return o.op == opIntConst;
	if (op == opWildStrConst) return o.op == opStrConst;
	if (op == opWildRegOf)    return o.op == opRegOf;
	if (op == opWildMemOf)    return o.op == opMemOf;
	if (op == opWildAddrOf)   return o.op == opAddrOf;
	return op == opWild  // Wild matches anything
	    || o.op == opWild
	    || op == o.op;
}
bool
TypedExp::operator ==(const Exp &e) const
{
	const TypedExp &o = (const TypedExp &)e;
	if (o.op == opWild)
		return true;
	return op == o.op
	    && *type == *o.type  // This is the strict type version
	    && *subExp1 == *o.subExp1;
}
bool
RefExp::operator ==(const Exp &e) const
{
	const RefExp &o = (const RefExp &)e;
	if (o.op == opWild)
		return true;
	if (op != o.op
	 || !(*subExp1 == *o.subExp1))
		return false;
	return def == (Statement *)-1  // Allow a def of (Statement *)-1 as a wild card
	    || o.def == (Statement *)-1
	    || (!def && o.isImplicitDef())  // Allow a def of nullptr to match a def of an implicit assignment
	    || (!o.def && dynamic_cast<ImplicitAssign *>(def))
	    || def == o.def;
}
bool
TypeVal::operator ==(const Exp &e) const
{
	const TypeVal &o = (const TypeVal &)e;
	if (o.op == opWild)
		return true;
	return op == o.op
	    && *val == *o.val;
}

/**
 * \fn bool Exp::operator <(const Exp &e) const
 * \brief Type sensitive less than.
 *
 * Compare myself with another Exp.  Type sensitive.
 *
 * \param[in] e  Ref to other Exp.
 * \returns      true if less than.
 */
/**
 * \fn bool Exp::operator <<(const Exp &e) const
 * \brief Type insensitive less than.
 *
 * Compare myself with another Exp.  Type insensitive.
 *
 * \param[in] e  Ref to other Exp.
 * \returns      true if less than.
 */
bool
Const::operator <(const Exp &e) const
{
	const Const &o = (const Const &)e;
	if (op != o.op) return op < o.op;
	if (conscript) {
		if (conscript < o.conscript) return true;
		if (conscript > o.conscript) return false;
	} else if (o.conscript) return true;
	switch (op) {
	case opIntConst: return u.i < o.u.i;
	case opFltConst: return u.d < o.u.d;
	case opStrConst: return strcmp(u.p, o.u.p) < 0;
	default:
		LOG << "operator < invalid operator " << operStrings[op] << "\n";
		assert(0);
		return false;
	}
}
bool
Terminal::operator <(const Exp &e) const
{
	const Terminal &o = (const Terminal &)e;
	return op < o.op;
}
bool
Unary::operator <(const Exp &e) const
{
	const Unary &o = (const Unary &)e;
	if (op != o.op) return op < o.op;
	return *subExp1 < *o.subExp1;
}
bool
Binary::operator <(const Exp &e) const
{
	assert(subExp1 && subExp2);
	const Binary &o = (const Binary &)e;
	if (op != o.op) return op < o.op;
	if (*subExp1 < *o.subExp1) return true;
	if (*o.subExp1 < *subExp1) return false;
	return *subExp2 < *o.subExp2;
}
bool
Ternary::operator <(const Exp &e) const
{
	const Ternary &o = (const Ternary &)e;
	if (op != o.op) return op < o.op;
	if (*subExp1 < *o.subExp1) return true;
	if (*o.subExp1 < *subExp1) return false;
	if (*subExp2 < *o.subExp2) return true;
	if (*o.subExp2 < *subExp2) return false;
	return *subExp3 < *o.subExp3;
}
bool
TypedExp::operator <<(const Exp &e) const
{
	const TypedExp &o = (const TypedExp &)e;
	if (op != o.op) return op < o.op;
	return *subExp1 << *o.subExp1;
}
bool
TypedExp::operator <(const Exp &e) const
{
	const TypedExp &o = (const TypedExp &)e;
	if (op != o.op) return op < o.op;
	if (*type < *o.type) return true;
	if (*o.type < *type) return false;
	return *subExp1 < *o.subExp1;
}
bool
RefExp::operator <(const Exp &e) const
{
	const RefExp &o = (const RefExp &)e;
	if (op != o.op) return op < o.op;
	if (*subExp1 < *o.subExp1) return true;
	if (*o.subExp1 < *subExp1) return false;
	// Allow a wildcard def to match any
	if (def == (Statement *)-1
	 || o.def == (Statement *)-1)
		return false;
	return def < o.def;
}
bool
TypeVal::operator <(const Exp &e) const
{
	const TypeVal &o = (const TypeVal &)e;
	if (op != o.op) return op < o.op;
	return *val < *o.val;
}

/**
 * \fn bool Exp::operator *=(const Exp &e) const
 * \brief Comparison ignoring subscripts.
 *
 * Compare myself for equality with another Exp, *ignoring subscripts*.
 *
 * \param[in] e  Ref to other Exp.
 * \returns      true if equal.
 */
bool
Const::operator *=(const Exp &e) const
{
	const Exp *other = &e;
	if (auto re = dynamic_cast<const RefExp *>(other)) other = re->getSubExp1();
	return *this == *other;
}
bool
Unary::operator *=(const Exp &e) const
{
	const Exp *other = &e;
	if (auto re = dynamic_cast<const RefExp *>(other)) other = re->getSubExp1();
	const Unary *o = (const Unary *)other;
	if ((o->op == opWild)
	 || (o->op == opWildRegOf  && op == opRegOf)
	 || (o->op == opWildMemOf  && op == opMemOf)
	 || (o->op == opWildAddrOf && op == opAddrOf))
		return true;
	return op == o->op
	    && (*subExp1 *= *o->subExp1);
}
bool
Binary::operator *=(const Exp &e) const
{
	assert(subExp1 && subExp2);
	const Exp *other = &e;
	if (auto re = dynamic_cast<const RefExp *>(other)) other = re->getSubExp1();
	const Binary *o = (const Binary *)other;
	if (o->op == opWild)
		return true;
	return op == o->op
	    && (*subExp1 *= *o->subExp1)
	    && (*subExp2 *= *o->subExp2);
}
bool
Ternary::operator *=(const Exp &e) const
{
	const Exp *other = &e;
	if (auto re = dynamic_cast<const RefExp *>(other)) other = re->getSubExp1();
	const Ternary *o = (const Ternary *)other;
	if (o->op == opWild)
		return true;
	return op == o->op
	    && (*subExp1 *= *o->subExp1)
	    && (*subExp2 *= *o->subExp2)
	    && (*subExp3 *= *o->subExp3);
}
bool
Terminal::operator *=(const Exp &e) const
{
	const Exp *other = &e;
	if (auto re = dynamic_cast<const RefExp *>(other)) other = re->getSubExp1();
	return *this == *other;
}
bool
TypedExp::operator *=(const Exp &e) const
{
	const Exp *other = &e;
	if (auto re = dynamic_cast<const RefExp *>(other)) other = re->getSubExp1();
	const TypedExp *o = (const TypedExp *)other;
	if (o->op == opWild)
		return true;
	return op == o->op
	    && *type == *o->type  // This is the strict type version
	    && (*subExp1 *= *o->subExp1);
}
bool
RefExp::operator *=(const Exp &e) const
{
	const Exp *other = &e;
	if (auto re = dynamic_cast<const RefExp *>(other)) other = re->getSubExp1();
	return (*subExp1 *= *other);
}

/**
 * \fn void Exp::print(std::ostream &os, bool html) const
 * \brief Print the expression to the given stream.
 *
 * "Print" in infix notation the expression to a stream.  Mainly for
 * debugging, or maybe some low level windows.
 *
 * \param[in] os    Ref to an output stream.
 * \param[in] html  Print in HTML format.
 */
/**
 * \fn void Exp::printr(std::ostream &os, bool html) const
 * \brief Wrapper for print that adds outer parentheses when necessary.
 *
 * The "r" is for recursive:  The idea is that we don't want parentheses at
 * the outer level, but a subexpression (recursed from a higher level), we
 * want the parens (at least for standard infix operators).
 *
 * Call printr() when recursing over subexpressions, else call print() for any
 * exceptional situations where the extra parens are not desired.
 *
 * operator << calls print(), which does not have outer parens.
 *
 * \param[in] os    Ref to an output stream.
 * \param[in] html  Print in HTML format.
 */
void
Const::print(std::ostream &os, bool html) const
{
	switch (op) {
	case opIntConst:
		if (u.i < -1000 || u.i > 1000)
			os << "0x" << std::hex << u.i << std::dec;
		else
			os << u.i;
		break;
	case opLongConst:
		if ((long long)u.ll < -1000LL || (long long)u.ll > 1000LL)
			os << "0x" << std::hex << u.ll << std::dec << "LL";
		else
			os << u.ll << "LL";
		break;
	case opFltConst:
		char buf[64];
		sprintf(buf, "%.4f", u.d);  // FIXME: needs an intelligent printer
		os << buf;
		break;
	case opStrConst:
		os << "\"" << u.p << "\"";
		break;
	default:
		LOG << "Const::print invalid operator " << operStrings[op] << "\n";
		assert(0);
	}
	if (conscript)
		os << "\\" << conscript << "\\";
}

void
Const::printNoQuotes(std::ostream &os) const
{
	if (isStrConst())
		os << u.p;
	else
		print(os);
}

void
Binary::printr(std::ostream &os, bool html) const
{
	assert(subExp1 && subExp2);
	switch (op) {
	case opSize:
	case opList:  // Otherwise, you get (a, (b, (c, d)))
		// There may be others
		// These are the noparen cases
		print(os, html);
		return;
	default:
		// Normal case: we want the parens
		os << "(";
		print(os, html);
		os << ")";
		return;
	}
}

void
Binary::print(std::ostream &os, bool html) const
{
	assert(subExp1 && subExp2);
	// Special cases
	switch (op) {
	case opSize:
		// This can still be seen after decoding and before type analysis after m[...]
		// *size* is printed after the expression, even though it comes from the first subexpression
		subExp2->printr(os, html);
		os << "*";
		subExp1->printr(os, html);
		os << "*";
		return;
	case opFlagCall:
		// The name of the flag function (e.g. ADDFLAGS) should be enough
		((Const *)subExp1)->printNoQuotes(os);
		os << "(";
		subExp2->print(os, html);
		os << ")";
		return;
	case opExpTable:
		os << "exptable(";
		subExp1->print(os, html);
		os << ", ";
		subExp2->print(os, html);
		os << ")";
		return;

	case opList:
		// Because "," is the lowest precedence operator, we don't need printr here.
		// Also, same as UQBT, so easier to test
		subExp1->print(os, html);
		if (!subExp2->isNil())
			os << ", ";
		subExp2->print(os, html);
		return;

	case opMemberAccess:
		subExp1->printr(os, html);
		os << ".";
		((Const *)subExp2)->printNoQuotes(os);
		return;

	case opArrayIndex:
		subExp1->printr(os, html);
		os << "[";
		subExp2->print(os, html);
		os << "]";
		return;

	default:
		break;
	}

	// Ordinary infix operators. Emit parens around the binary
	if (subExp1) subExp1->printr(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>");
	os << " ";

	switch (op) {
	case opPlus:      os << "+";   break;
	case opMinus:     os << "-";   break;
	case opMult:      os << "*";   break;
	case opMults:     os << "*!";  break;
	case opDiv:       os << "/";   break;
	case opDivs:      os << "/!";  break;
	case opMod:       os << "%";   break;
	case opMods:      os << "%!";  break;
	case opFPlus:     os << "+f";  break;
	case opFMinus:    os << "-f";  break;
	case opFMult:     os << "*f";  break;
	case opFDiv:      os << "/f";  break;
	case opPow:       os << "pow"; break;  // Raising to power

	case opAnd:       os << "and"; break;
	case opOr:        os << "or";  break;
	case opBitAnd:    os << (html ? "&amp;" : "&"); break;
	case opBitOr:     os << "|";   break;
	case opBitXor:    os << "^";   break;
	case opEqual:     os << "=";   break;
	case opNotEqual:  os << "~=";  break;
	case opLess:      os << (html ? "&lt;"   : "<");   break;
	case opGtr:       os << (html ? "&gt;"   : ">");   break;
	case opLessEq:    os << (html ? "&lt;="  : "<=");  break;
	case opGtrEq:     os << (html ? "&gt;="  : ">=");  break;
	case opLessUns:   os << (html ? "&lt;u"  : "<u");  break;
	case opGtrUns:    os << (html ? "&gt;u"  : ">u");  break;
	case opLessEqUns: os << (html ? "&lt;u"  : "<=u"); break;
	case opGtrEqUns:  os << (html ? "&gt;=u" : ">=u"); break;
	case opUpper:     os << "GT"; break;
	case opLower:     os << "LT"; break;

	case opShiftL:    os << (html ? "&lt;&lt;"  : "<<");  break;
	case opShiftR:    os << (html ? "&gt;&gt;"  : ">>");  break;
	case opShiftRA:   os << (html ? "&gt;&gt;A" : ">>A"); break;
	case opRotateL:   os << "rl";  break;
	case opRotateR:   os << "rr";  break;
	case opRotateLC:  os << "rlc"; break;
	case opRotateRC:  os << "rrc"; break;

	default:
		LOG << "Binary::print invalid operator " << operStrings[op] << "\n";
		assert(0);
	}

	os << " ";
	if (subExp2) subExp2->printr(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>");
}

void
Terminal::print(std::ostream &os, bool html) const
{
	switch (op) {
	case opPC:           os << "%pc";     break;
	case opFlags:        os << "%flags";  break;
	case opFflags:       os << "%fflags"; break;
	case opCF:           os << "%CF";     break;
	case opZF:           os << "%ZF";     break;
	case opOF:           os << "%OF";     break;
	case opNF:           os << "%NF";     break;
	case opDF:           os << "%DF";     break;
	case opAFP:          os << "%afp";    break;
	case opAGP:          os << "%agp";    break;
	case opFpush:        os << "FPUSH";   break;
	case opFpop:         os << "FPOP";    break;
	case opWild:         os << "WILD";    break;
	case opWildRegOf:    os << "r[WILD]"; break;
	case opWildMemOf:    os << "m[WILD]"; break;
	case opWildAddrOf:   os << "a[WILD]"; break;
	case opWildIntConst: os << "WILDINT"; break;
	case opWildStrConst: os << "WILDSTR"; break;
	case opNil:                           break;
	case opTrue:         os << "true";    break;
	case opFalse:        os << "false";   break;
	case opDefineAll:    os << (html ? "&lt;all&gt;" : "<all>"); break;
	default:
		LOG << "Terminal::print invalid operator " << operStrings[op] << "\n";
		assert(0);
	}
}

void
Unary::print(std::ostream &os, bool html) const
{
	switch (op) {
	//  //  //  //  //  //  //
	//  x[ subexpression ]  //
	//  //  //  //  //  //  //
	case opRegOf:
		// Make a special case for the very common case of r[intConst]
		if (subExp1->isIntConst()) {
			os << "r" << ((Const *)subExp1)->getInt();
			break;
		} else if (subExp1->isTemp()) {
			// Just print the temp {   // balance }s
			subExp1->print(os, html);
			break;
		}
		// Else fall through
	case opMemOf:
	case opAddrOf:
	case opVar:
	case opTypeOf:
	case opKindOf:
		switch (op) {
		case opRegOf:  os << "r["; break;  // e.g. r[r2]
		case opMemOf:  os << "m["; break;
		case opAddrOf: os << "a["; break;
		case opVar:    os << "v["; break;
		case opTypeOf: os << "T["; break;
		case opKindOf: os << "K["; break;
		default:                   break;  // Suppress compiler warning
		}
		if (op == opVar)
			((Const *)subExp1)->printNoQuotes(os);
		// Use print, not printr, because this is effectively the top level again (because the [] act as
		// parentheses)
		else
			subExp1->print(os, html);
		os << "]";
		break;

	case opTemp:
		if (subExp1->getOper() == opWildStrConst) {
			os << "t[";
			subExp1->print(os, html);
			os << "]";
			return;
		}
		// Temp: just print the string, no quotes
	case opGlobal:
	case opLocal:
	case opParam:
		// Print a more concise form than param["foo"] (just foo)
		((Const *)subExp1)->printNoQuotes(os);
		return;

	//  //  //  //  //  //  //
	//    Unary operators   //
	//  //  //  //  //  //  //

	case opNot:
	case opLNot:
	case opNeg:
	case opFNeg:
		     if (op == opNot)  os << "~";
		else if (op == opLNot) os << "L~";
		else if (op == opFNeg) os << "~f ";
		else                   os << "-";
		subExp1->printr(os, html);
		return;

	case opSignExt:
	case opInitValueOf:
	case opPostVar:
		subExp1->printr(os, html);
		if (op == opSignExt) os << "!";  // Operator after expression
		else                 os << "'";
		return;

	//  //  //  //  //  //  //  //
	//  Function-like operators //
	//  //  //  //  //  //  //  //

	case opFtrunc:
	case opFabs:
	case opSQRTs:
	case opSQRTd:
	case opSQRTq:
	case opSqrt:
	case opSin:
	case opCos:
	case opTan:
	case opArcTan:
	case opLog2:
	case opLog10:
	case opLoge:
	case opExecute:
	case opMachFtr:
	case opSuccessor:
	case opPhi:
		switch (op) {
		case opFtrunc:    os << "ftrunc(";  break;
		case opFabs:      os << "fabs(";    break;
		case opSQRTs:     os << "SQRTs(";   break;
		case opSQRTd:     os << "SQRTd(";   break;
		case opSQRTq:     os << "SQRTq(";   break;
		case opSqrt:      os << "sqrt(";    break;
		case opSin:       os << "sin(";     break;
		case opCos:       os << "cos(";     break;
		case opTan:       os << "tan(";     break;
		case opArcTan:    os << "arctan(";  break;
		case opLog2:      os << "log2(";    break;
		case opLog10:     os << "log10(";   break;
		case opLoge:      os << "loge(";    break;
		case opExecute:   os << "execute("; break;
		case opMachFtr:   os << "machine("; break;
		case opSuccessor: os << "succ(";    break;
		case opPhi:       os << "phi(";     break;
		default:                            break;  // For warning
		}
		subExp1->print(os, html);
		os << ")";
		return;

	default:
		LOG << "Unary::print invalid operator " << operStrings[op] << "\n";
		assert(0);
	}
}

void
Ternary::printr(std::ostream &os, bool html) const
{
	// The function-like operators don't need parentheses
	switch (op) {
	// The "function-like" ternaries
	case opTruncu:
	case opTruncs:
	case opZfill:
	case opSgnEx:
	case opFsize:
	case opItof:
	case opFtoi:
	case opFround:
	case opOpTable:
		// No paren case
		print(os, html);
		return;
	default:
		// All other cases, we use the parens
		os << "(";
		print(os, html);
		os << ")";
		return;
	}
}

void
Ternary::print(std::ostream &os, bool html) const
{
	switch (op) {
	// The "function-like" ternaries
	case opTruncu:
	case opTruncs:
	case opZfill:
	case opSgnEx:
	case opFsize:
	case opItof:
	case opFtoi:
	case opFround:
	case opOpTable:
		switch (op) {
		case opTruncu:  os << "truncu(";  break;
		case opTruncs:  os << "truncs(";  break;
		case opZfill:   os << "zfill(";   break;
		case opSgnEx:   os << "sgnex(";   break;
		case opFsize:   os << "fsize(";   break;
		case opItof:    os << "itof(";    break;
		case opFtoi:    os << "ftoi(";    break;
		case opFround:  os << "fround(";  break;
		case opOpTable: os << "optable("; break;
		default:                          break;  // For warning
		}
		// Use print not printr here, since , has the lowest precendence of all.
		// Also it makes it the same as UQBT, so it's easier to test
		if (subExp1) subExp1->print(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>"); os << ", ";
		if (subExp2) subExp2->print(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>"); os << ", ";
		if (subExp3) subExp3->print(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>"); os << ")";
		return;
	default:
		break;
	}
	// Else must be ?: or @ (traditional ternary operators)
	if (subExp1) subExp1->printr(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>");
	if (op == opTern) {
		os << " ? ";
		if (subExp2) subExp2->printr(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>");
		os << " : ";  // Need wide spacing here
		if (subExp3) subExp3->print(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>");
	} else if (op == opAt) {
		os << "@[";
		if (subExp2) subExp2->printr(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>");
		os << ":";
		if (subExp3) subExp3->printr(os, html); else os << (html ? "&lt;NULL&gt;" : "<NULL>");
		os << "]";
	} else {
		LOG << "Ternary::print invalid operator " << operStrings[op] << "\n";
		assert(0);
	}
}

void
TypedExp::print(std::ostream &os, bool html) const
{
	os << " ";
	type->starPrint(os);
	subExp1->print(os, html);
}

void
RefExp::print(std::ostream &os, bool html) const
{
	if (subExp1)
		subExp1->print(os, html);
	else
		os << (html ? "&lt;NULL&gt;" : "<NULL>");
	os << (html ? "<sub>" : "{");
	if (def == (Statement *)-1) {
		os << "WILD";
	} else if (def) {
		auto num = def->getNumber();
		if (html)
			os << "<a href=\"#stmt" << num << "\">" << num << "</a>";
		else
			os << num;
	} else {
		os << "-";  // So you can tell the difference with {0}
	}
	os << (html ? "</sub>" : "}");
}

void
TypeVal::print(std::ostream &os, bool html) const
{
	if (val)
		os << (html ? "&lt;" : "<") << val->getCtype() << (html ? "&gt;" : ">");
	else
		os << (html ? "&lt;NULL&gt;" : "<NULL>");
}

/**
 * \brief Print to a string (for debugging and logging).
 * \returns  The string.
 */
std::string
Exp::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

/**
 * \brief Display as a dotty graph.
 *
 * Create a dotty file (use dotty to display the file; search the web for
 * "graphviz").  Mainly for debugging.
 *
 * \param[in] os  The stream to write.
 */
void
Exp::createDot(std::ostream &os) const
{
	os << "digraph Exp {\n";
	appendDot(os);
	os << "}\n";
}

/**
 * \fn void Exp::appendDot(std::ostream &os) const
 * \brief Recursively append this Exp to a dotty graph.
 *
 * \param[in] os  The stream to write.
 */
void
Const::appendDot(std::ostream &os) const
{
	// We define a unique name for each node as "e0x123456" if the address of "this" == 0x123456
	os << "\te" << (void *)this
	   << " [shape=record,label=\"{ " << operStrings[op]
	   << "\\n" << (void *)this
	   << " | ";
	switch (op) {
	case opIntConst:  os << u.i; break;
	case opFltConst:  os << u.d; break;
	case opStrConst:  os << "\\\"" << u.p << "\\\""; break;
	default:
		break;
	}
	os << " }\"];\n";
}

void
Terminal::appendDot(std::ostream &os) const
{
	os << "\te" << (void *)this
	// Note: opWild value is -1, so can't index array
	   << " [shape=parallelogram,label=\"" << (op == opWild ? "WILD" : operStrings[op])
	   << "\\n" << (void *)this
	   << "\"];\n";
}

void
Unary::appendDot(std::ostream &os) const
{
	// First a node for this Unary object
	os << "\te" << (void *)this
	   << " [shape=record,label=\"{ " << operStrings[op]
	   << "\\n" << (void *)this
	   << " | <p1> }\"];\n";

	// Now recurse to the subexpression.
	subExp1->appendDot(os);

	// Finally an edge for the subexpression
	os << "\te" << (void *)this << ":p1 -> e" << (void *)subExp1 << ";\n";
}

void
Binary::appendDot(std::ostream &os) const
{
	// First a node for this Binary object
	os << "\te" << (void *)this
	   << " [shape=record,label=\"{ " << operStrings[op]
	   << "\\n" << (void *)this
	   << " | {<p1> | <p2>} }\"];\n";

	subExp1->appendDot(os);
	subExp2->appendDot(os);

	// Now an edge for each subexpression
	os << "\te" << (void *)this << ":p1 -> e" << (void *)subExp1 << ";\n";
	os << "\te" << (void *)this << ":p2 -> e" << (void *)subExp2 << ";\n";
}

void
Ternary::appendDot(std::ostream &os) const
{
	// First a node for this Ternary object
	os << "\te" << (void *)this
	   << " [shape=record,label=\"{ " << operStrings[op]
	   << "\\n" << (void *)this
	   << " | {<p1> | <p2> | <p3>} }\"];\n";

	subExp1->appendDot(os);
	subExp2->appendDot(os);
	subExp3->appendDot(os);

	// Now an edge for each subexpression
	os << "\te" << (void *)this << ":p1 -> e" << (void *)subExp1 << ";\n";
	os << "\te" << (void *)this << ":p2 -> e" << (void *)subExp2 << ";\n";
	os << "\te" << (void *)this << ":p3 -> e" << (void *)subExp3 << ";\n";
}

void
TypedExp::appendDot(std::ostream &os) const
{
	os << "\te" << (void *)this
	   << " [shape=record,label=\"{ " << operStrings[op]
	   << "\\n" << (void *)this
	// Just display the C type for now
	   << " | " << type->getCtype() << " | <p1> }\"];\n";

	subExp1->appendDot(os);

	os << "\te" << (void *)this << ":p1 -> e" << (void *)subExp1 << ";\n";
}

void
FlagDef::appendDot(std::ostream &os) const
{
	os << "\te" << (void *)this
	   << " [shape=record,label=\"{ " << operStrings[op]
	   << "\\n" << (void *)this
	// Display the RTL as "RTL <r0> <r1>..." vertically (curly brackets)
	   << " | { RTL ";
	int n = rtl->getList().size();
	for (int i = 0; i < n; ++i)
		os << "| <r" << i << "> ";
	os << "} | <p1> }\"];\n";

	subExp1->appendDot(os);

	os << "\te" << (void *)this << ":p1 -> e" << (void *)subExp1 << ";\n";
}

/**
 * \returns  true if the expression is r[K] where K is int const.
 */
bool
Exp::isRegOfK() const
{
	if (!isRegOf()) return false;
	return ((const Location *)this)->getSubExp1()->isIntConst();
}

/**
 * \returns  true if the expression is r[N] where N is the given int const.
 * \param[in] N  The specific register to be tested for.
 */
bool
Exp::isRegN(int N) const
{
	if (!isRegOf()) return false;
	const Exp *sub = ((const Location *)this)->getSubExp1();
	return (sub->isIntConst() && ((const Const *)sub)->getInt() == N);
}

/**
 * \returns  true if the expression is m[K] where K is int const.
 */
bool
Exp::isMemOfK() const
{
	if (!isMemOf()) return false;
	return ((const Location *)this)->getSubExp1()->isIntConst();
}

/**
 * \returns  true if is \%afp, \%afp+k, \%afp-k, or a[m[\<any of these\>]].
 */
bool
Exp::isAfpTerm() const
{
	const Exp *cur = this;
	if (auto te = dynamic_cast<const TypedExp *>(this))
		cur = te->getSubExp1();
	const Exp *p;
	if ((cur->isAddrOf()) && ((p = ((const Unary *)cur)->getSubExp1()), p->isMemOf()))
		cur = ((const Unary *)p)->getSubExp1();

	OPER curOp = cur->getOper();
	if (curOp == opAFP) return true;
	if ((curOp != opPlus) && (curOp != opMinus)) return false;
	// cur must be a Binary* now
	OPER subOp1 = ((const Binary *)cur)->getSubExp1()->getOper();
	OPER subOp2 = ((const Binary *)cur)->getSubExp2()->getOper();
	return ((subOp1 == opAFP) && (subOp2 == opIntConst));
}

bool
Exp::isTrue() const
{
	return op == opTrue || (op == opIntConst && !!((Const *)this)->getInt());
}

bool
Exp::isFalse() const
{
	return op == opFalse || (op == opIntConst && !((Const *)this)->getInt());
}

#if 0 // Cruft?
/**
 * \returns  The index for this var, e.g. if v[2], return 2.
 */
int
Exp::getVarIndex() const
{
	assert(op == opVar);
	const Exp *sub = ((const Unary *)this)->getSubExp1();
	return ((const Const *)sub)->getInt();
}
#endif

#if 0 // Cruft?
/**
 * \returns  A ptr to the guard expression, or nullptr if none.
 */
Exp *
Exp::getGuard() const
{
	if (op == opGuard) return ((const Unary *)this)->getSubExp1();
	return nullptr;
}
#endif

/**
 * Matches this expression to the given pattern.
 *
 * \param[in] pattern  Pattern to match.
 *
 * \returns  List of variable bindings, or nullptr if matching fails.
 */
Exp *
Exp::match(const Exp *pattern) const
{
	if (*this == *pattern)
		return new Terminal(opNil);
	if (pattern->getOper() == opVar) {
		return new Binary(opList,
		                  new Binary(opEqual, pattern->clone(), this->clone()),
		                  new Terminal(opNil));
	}
	return nullptr;
}
Exp *
Unary::match(const Exp *pattern) const
{
	assert(subExp1);
	if (op == pattern->getOper()) {
		return subExp1->match(pattern->getSubExp1());
	}
	return Exp::match(pattern);
}
Exp *
Binary::match(const Exp *pattern) const
{
	assert(subExp1 && subExp2);
	if (op == pattern->getOper()) {
		Exp *b_lhs = subExp1->match(pattern->getSubExp1());
		if (!b_lhs)
			return nullptr;
		Exp *b_rhs = subExp2->match(pattern->getSubExp2());
		if (!b_rhs)
			return nullptr;
		if (b_lhs->isNil())
			return b_rhs;
		if (b_rhs->isNil())
			return b_lhs;
#if 0
		LOG << "got lhs list " << b_lhs << " and rhs list " << b_rhs << "\n";
#endif
		Exp *result = new Terminal(opNil);
		for (Exp *l = b_lhs; !l->isNil(); l = l->getSubExp2())
			for (Exp *r = b_rhs; !r->isNil(); r = r->getSubExp2())
				if (  *l->getSubExp1()->getSubExp1() == *r->getSubExp1()->getSubExp1()
				 && !(*l->getSubExp1()->getSubExp2() == *r->getSubExp1()->getSubExp2())) {
#if 0
					LOG << "disagreement in match: " << l->getSubExp1()->getSubExp2()
					    << " != " << r->getSubExp1()->getSubExp2() << "\n";
#endif
					return nullptr;  // must be agreement between LHS and RHS
				} else
					result = new Binary(opList, l->getSubExp1()->clone(), result);
		for (Exp *r = b_rhs; !r->isNil(); r = r->getSubExp2())
			result = new Binary(opList, r->getSubExp1()->clone(), result);
		return result;
	}
	return Exp::match(pattern);
}
Exp *
RefExp::match(const Exp *pattern) const
{
	Exp *r = Unary::match(pattern);
#if 0
	if (r)
		return r;
	r = subExp1->match(pattern);
	if (r) {
		bool change;
		r = r->searchReplaceAll(subExp1->clone(), this->clone(), change);
		return r;
	}
	return Exp::match(pattern);
#else
	return r;
#endif
}
#if 0  // Suspect ADHOC TA only
Exp *
TypeVal::match(const Exp *pattern) const
{
	if (op == pattern->getOper()) {
		return val->match(pattern->getType());
	}
	return Exp::match(pattern);
}
#endif

#define ISVARIABLE(x) (strspn((x), "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == strlen((x)))
//#define DEBUG_MATCH

static const char *
tlstrchr(const char *str, char ch)
{
	while (str && *str) {
		if (*str == ch)
			return str;
		if (*str == '[' || *str == '{' || *str == '(') {
			char close = ']';
			if (*str == '{')
				close = '}';
			if (*str == '(')
				close = ')';
			while (*str && *str != close)
				++str;
		}
		if (*str)
			++str;
	}
	return nullptr;
}

/**
 * Matches this expression to the given pattern.
 *
 * \param[in] pattern   String pattern to match.
 * \param     bindings  Map of bindings.
 *
 * \returns  true if match, false otherwise.
 */
bool
Exp::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	// most obvious
	if (prints() == pattern)
		return true;

	// alright, is pattern an acceptable variable?
	if (ISVARIABLE(pattern)) {
		bindings[pattern] = this;
		return true;
	}

	// no, fail
	return false;
}
bool
Unary::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	if (Exp::match(pattern, bindings))
		return true;
#ifdef DEBUG_MATCH
	LOG << "unary::match " << this << " to " << pattern << ".\n";
#endif
	if (isAddrOf() && pattern[0] == 'a' && pattern[1] == '['
	 && pattern[strlen(pattern) - 1] == ']') {
		char *sub1 = strdup(pattern + 2);
		sub1[strlen(sub1) - 1] = 0;
		return subExp1->match(sub1, bindings);
	}
	return false;
}
bool
Binary::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	if (Exp::match(pattern, bindings))
		return true;
#ifdef DEBUG_MATCH
	LOG << "binary::match " << this << " to " << pattern << ".\n";
#endif
	if (isMemberOf() && tlstrchr(pattern, '.')) {
		char *sub1 = strdup(pattern);
		char *sub2 = (char *)tlstrchr(sub1, '.');
		*sub2++ = 0;
		if (subExp1->match(sub1, bindings)) {
			assert(subExp2->isStrConst());
			if (!strcmp(sub2, ((Const *)subExp2)->getStr()))
				return true;
			if (ISVARIABLE(sub2)) {
				bindings[sub2] = subExp2;
				return true;
			}
		}
	}
	if (isArrayIndex()) {
		if (pattern[strlen(pattern) - 1] != ']')
			return false;
		char *sub1 = strdup(pattern);
		char *sub2 = strrchr(sub1, '[');
		*sub2++ = 0;
		sub2[strlen(sub2) - 1] = 0;
		if (subExp1->match(sub1, bindings) && subExp2->match(sub2, bindings))
			return true;
	}
	if (op == opPlus && tlstrchr(pattern, '+')) {
		char *sub1 = strdup(pattern);
		char *sub2 = (char *)tlstrchr(sub1, '+');
		*sub2++ = 0;
		while (*sub2 == ' ')
			++sub2;
		while (sub1[strlen(sub1) - 1] == ' ')
			sub1[strlen(sub1) - 1] = 0;
		if (subExp1->match(sub1, bindings) && subExp2->match(sub2, bindings))
			return true;
	}
	if (op == opMinus && tlstrchr(pattern, '-')) {
		char *sub1 = strdup(pattern);
		char *sub2 = (char *)tlstrchr(sub1, '-');
		*sub2++ = 0;
		while (*sub2 == ' ')
			++sub2;
		while (sub1[strlen(sub1) - 1] == ' ')
			sub1[strlen(sub1) - 1] = 0;
		if (subExp1->match(sub1, bindings) && subExp2->match(sub2, bindings))
			return true;
	}
	return false;
}
bool
Ternary::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	if (Exp::match(pattern, bindings))
		return true;
#ifdef DEBUG_MATCH
	LOG << "ternary::match " << this << " to " << pattern << ".\n";
#endif
	return false;
}
bool
RefExp::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	if (Exp::match(pattern, bindings))
		return true;
#ifdef DEBUG_MATCH
	LOG << "refexp::match " << this << " to " << pattern << ".\n";
#endif
	const char *end = pattern + strlen(pattern) - 1;
	if (end > pattern && *end == '}') {
		--end;
		if (*end == '-' && !def) {
			char *sub = strdup(pattern);
			*(sub + (end - 1 - pattern)) = 0;
			return subExp1->match(sub, bindings);
		}
		end = strrchr(end, '{');
		if (end) {
			if (atoi(end + 1) == def->getNumber()) {
				char *sub = strdup(pattern);
				*(sub + (end - pattern)) = 0;
				return subExp1->match(sub, bindings);
			}
		}
	}
	return false;
}
bool
Const::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	if (Exp::match(pattern, bindings))
		return true;
#ifdef DEBUG_MATCH
	LOG << "const::match " << this << " to " << pattern << ".\n";
#endif
	return false;
}
bool
Terminal::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	if (Exp::match(pattern, bindings))
		return true;
#ifdef DEBUG_MATCH
	LOG << "terminal::match " << this << " to " << pattern << ".\n";
#endif
	return false;
}
bool
Location::match(const char *pattern, std::map<std::string, const Exp *> &bindings) const
{
	if (Exp::match(pattern, bindings))
		return true;
#ifdef DEBUG_MATCH
	LOG << "location::match " << this << " to " << pattern << ".\n";
#endif
	if (isMemOf() || isRegOf()) {
		char ch = 'm';
		if (isRegOf())
			ch = 'r';
		if (pattern[0] != ch || pattern[1] != '[')
			return false;
		if (pattern[strlen(pattern) - 1] != ']')
			return false;
		char *sub = strdup(pattern + 2);
		*(sub + strlen(sub) - 1) = 0;
		return subExp1->match(sub, bindings);
	}
	return false;
}

/**
 * Search for the given subexpression.
 *
 * \note Mostly not for public use.
 * \note Caller must free the list li after use, but not the Exp objects that
 *       they point to.
 * \note If the top level expression matches, li will contain search.
 * \note Now a static function. Searches pSrc, not this.
 *
 * \param search  Ptr to Exp we are searching for.
 * \param pSrc    Ref to ptr to Exp to search.  Reason is that we can then
 *                overwrite that pointer to effect a replacement.  So we need
 *                to append &pSrc in the list.  Can't append &this!
 * \param li      List of Exp** where pointers to the matches are found.
 * \param once    true if not all occurrences to be found, false for all.
 */
void
Exp::doSearch(Exp *search, Exp *&pSrc, std::list<Exp **> &li, bool once)
{
	bool compare = (*search == *pSrc);
	if (compare) {
		li.push_back(&pSrc);  // Success
		if (once)
			return;  // No more to do
	}
	// Either want to find all occurrences, or did not match at this level
	// Recurse into children, unless a matching opSubscript
	if (!compare || !pSrc->isSubscript())
		pSrc->doSearchChildren(search, li, once);
}

/**
 * Search for the given subexpression in all children
 *
 * \note Mostly not for public use.
 * \note Virtual function; different implementation for each subclass of Exp.
 * \note Will recurse via doSearch.
 *
 * \param search  Ptr to Exp we are searching for.
 * \param li      List of Exp** where pointers to the matches are found.
 * \param once    true if not all occurrences to be found, false for all.
 */
void
Exp::doSearchChildren(Exp *search, std::list<Exp **> &li, bool once)
{
	return;  // Const and Terminal do not override this
}
void
Unary::doSearchChildren(Exp *search, std::list<Exp **> &li, bool once)
{
	if (op != opInitValueOf)  // don't search child
		doSearch(search, subExp1, li, once);
}
void
Binary::doSearchChildren(Exp *search, std::list<Exp **> &li, bool once)
{
	assert(subExp1 && subExp2);
	doSearch(search, subExp1, li, once);
	if (once && !li.empty()) return;
	doSearch(search, subExp2, li, once);
}
void
Ternary::doSearchChildren(Exp *search, std::list<Exp **> &li, bool once)
{
	doSearch(search, subExp1, li, once);
	if (once && !li.empty()) return;
	doSearch(search, subExp2, li, once);
	if (once && !li.empty()) return;
	doSearch(search, subExp3, li, once);
}

/**
 * Search this Exp for the given subexpression, and replace if found.
 *
 * \note If the top level expression matches, return val != this.
 *
 * \param[in] search   Ptr to Exp we are searching for.
 * \param[in] replace  Ptr to Exp to replace it with.
 * \param[out] change  Set true if a change made; cleared otherwise.
 *
 * \returns  true if a change made.
 */
Exp *
Exp::searchReplace(Exp *search, Exp *replace, bool &change)
{
	return searchReplaceAll(search, replace, change, true);
}

/**
 * Search this Exp for the given subexpression, and replace wherever found.
 *
 * \note If the top level expression matches, something other than "this" will
 *       be returned.
 * \note It is possible with wildcards that in very unusual circumstances a
 *       replacement will be made to something that is already deleted.
 * \note Replacements are cloned.  Caller to delete search and replace.
 * \note change is ALWAYS assigned.  No need to clear beforehand.
 *
 * \param[in] search   Ptr to Exp we are searching for.
 * \param[in] replace  Ptr to Exp to replace it with.
 * \param[out] change  Set true if a change made; cleared otherwise.
 *
 * \returns  The result (often this, but possibly changed).
 */
Exp *
Exp::searchReplaceAll(Exp *search, Exp *replace, bool &change, bool once /* = false */)
{
	std::list<Exp **> li;
	Exp *top = this;  // top may change; that's why we have to return it
	doSearch(search, top, li, false);
	for (const auto &pp : li) {
		;//delete *pp;  // Delete any existing
		*pp = replace->clone();  // Do the replacement
		if (once) {
			change = true;
			return top;
		}
	}
	change = !li.empty();
	return top;
}

/**
 * Search this expression for the given subexpression, and if found, return
 * true and return a pointer to the matched expression in result (useful when
 * there are wildcards, e.g. search pattern is r[?] result is r[2]).
 *
 * \param[in] search   Ptr to Exp we are searching for.
 * \param[out] result  Ptr to Exp that matched.
 *
 * \returns  true if a match was found.
 */
bool
Exp::search(Exp *search, Exp *&result)
{
	std::list<Exp **> li;
	// The search requires a reference to a pointer to this object.
	// This isn't needed for searches, only for replacements, but we want to re-use the same search routine
	Exp *top = this;
	doSearch(search, top, li, true);
	if (!li.empty()) {
		result = *li.front();
		return true;
	}
	result = nullptr;  // In case it fails; don't leave it unassigned
	return false;
}

/**
 * Search this expression for the given subexpression, and for each found,
 * adds a pointer to the matched expression in result (useful with wildcards).
 *
 * \note Does NOT clear result on entry.
 *
 * \param[in] search   Ptr to Exp we are searching for.
 * \param[out] result  List of Exp that matched.
 *
 * \returns  true if a match was found.
 */
bool
Exp::searchAll(Exp *search, std::list<Exp *> &result)
{
	std::list<Exp **> li;
	//result.clear();  // No! Useful when searching for more than one thing
	                   // (add to the same list)
	// The search requires a reference to a pointer to this object.
	// This isn't needed for searches, only for replacements, but we want to re-use the same search routine
	Exp *pSrc = this;
	doSearch(search, pSrc, li, false);
	for (const auto &pp : li) {
		// li is list of Exp**; result is list of Exp*
		result.push_back(*pp);
	}
	return !li.empty();
}

// These simplifying functions don't really belong in class Exp, but they know too much about how Exps work
// They can't go into util.so, since then util.so and db.so would co-depend on each other for testing at least
/**
 * Takes an expression consisting of only + and - operators and partitions its
 * terms into positive non-integer fixed terms, negative non-integer fixed
 * terms and integer terms.
 *
 * For example, given:
 *     \%sp + 108 + n - \%sp - 92
 * the resulting partition will be:
 *     positives = { \%sp, n }
 *     negatives = { \%sp }
 *     integers  = { 108, -92 }
 *
 * \note integers is a vector so we can use the accumulate func.
 * \note Expressions are NOT cloned.  Therefore, do not delete the expressions
 *       in positives or negatives.
 *
 * \param positives  The list of positive terms.
 * \param negatives  The list of negative terms.
 * \param integers   The vector of integer terms.
 * \param negate     Determines whether or not to negate the whole expression,
 *                   i.e. we are on the RHS of an opMinus.
 */
void
Exp::partitionTerms(std::list<Exp *> &positives, std::list<Exp *> &negatives, std::vector<int> &integers, bool negate)
{
	Exp *p1, *p2;
	switch (op) {
	case opPlus:
		p1 = ((Binary *)this)->getSubExp1();
		p2 = ((Binary *)this)->getSubExp2();
		p1->partitionTerms(positives, negatives, integers, negate);
		p2->partitionTerms(positives, negatives, integers, negate);
		break;
	case opMinus:
		p1 = ((Binary *)this)->getSubExp1();
		p2 = ((Binary *)this)->getSubExp2();
		p1->partitionTerms(positives, negatives, integers, negate);
		p2->partitionTerms(positives, negatives, integers, !negate);
		break;
	case opTypedExp:
		p1 = ((Binary *)this)->getSubExp1();
		p1->partitionTerms(positives, negatives, integers, negate);
		break;
	case opIntConst:
		{
			int k = ((Const *)this)->getInt();
			integers.push_back(negate ? -k : k);
		}
		break;
	default:
		// These can be any other expression tree
		if (negate)
			negatives.push_back(this);
		else
			positives.push_back(this);
	}
}

/**
 * \fn Exp *Exp::simplifyArith()
 *
 * This method simplifies an expression consisting of + and - at the top
 * level.  For example, (\%sp + 100) - (\%sp + 92) will be simplified to 8.
 *
 * \note Any expression can be so simplified.
 * \note User must ;//delete result.
 *
 * \returns Ptr to the simplified expression.
 */
Exp *
Unary::simplifyArith()
{
	if (op == opMemOf || op == opRegOf || op == opAddrOf || op == opSubscript) {
		// assume we want to simplify the subexpression
		subExp1 = subExp1->simplifyArith();
	}
	return this;  // Else, do nothing
}

Exp *
Ternary::simplifyArith()
{
	subExp1 = subExp1->simplifyArith();
	subExp2 = subExp2->simplifyArith();
	subExp3 = subExp3->simplifyArith();
	return this;
}

Exp *
Binary::simplifyArith()
{
	subExp1 = subExp1->simplifyArith();
	subExp2 = subExp2->simplifyArith();
	if ((op != opPlus) && (op != opMinus))
		return this;

	// TODO: return this if parent is opPlus or opMinus, let parent do the simplification

	// Partition this expression into positive non-integer terms, negative
	// non-integer terms and integer terms.
	std::list<Exp *> positives;
	std::list<Exp *> negatives;
	std::vector<int> integers;
	partitionTerms(positives, negatives, integers, false);

	// Now reduce these lists by cancelling pairs
	// Note: can't improve this algorithm using multisets, since can't instantiate multisets of type Exp (only Exp*).
	// The Exp* in the multisets would be sorted by address, not by value of the expression.
	// So they would be unsorted, same as lists!
	auto pp = positives.begin();
	auto nn = negatives.begin();
	while (pp != positives.end()) {
		bool inc = true;
		while (nn != negatives.end()) {
			if (**pp == **nn) {
				// A positive and a negative that are equal; therefore they cancel
				pp = positives.erase(pp);  // Erase the pointers, not the Exps
				nn = negatives.erase(nn);
				inc = false;  // Don't increment pp now
				break;
			}
			++nn;
		}
		if (pp == positives.end()) break;
		if (inc) ++pp;
	}

	// Summarise the set of integers to a single number.
	int sum = std::accumulate(integers.begin(), integers.end(), 0);
	auto pos = Exp::Accumulate(positives);
	auto neg = Exp::Accumulate(negatives);

	// Now put all these elements back together and return the result
	if (neg) {
		if (!pos)
			return new Binary(opMinus, new Const(sum), neg);
		// Both positives and negatives, convert to only positives
		pos = new Binary(opMinus, pos, neg);
	}

	if (!pos)
		return new Const(sum);
	if (sum == 0)
		return pos;

	OPER op = opPlus;
	if (sum < 0) {
		op = opMinus;
		sum = -sum;
	}
	return new Binary(op, pos, new Const(sum));
}

/**
 * This method creates an expression that is the sum of all expressions in a
 * list.  E.g. given the list <4,r[8],m[14]> the resulting expression is
 * (4+(r[8]+m[14])).
 *
 * \note Static (non instance) function.
 * \note Exps ARE cloned.
 *
 * \param exprs  A list of expressions.  This list is cleared upon return.
 * \returns      A new Exp with the accumulation.
 */
Exp *
Exp::Accumulate(std::list<Exp *> &exprs)
{
	if (exprs.empty())
		return nullptr;

	auto res = exprs.front()->clone();
	exprs.pop_front();
	if (!exprs.empty())
		res = new Binary(opPlus, res, Accumulate(exprs));
	return res;
}

/**
 * \brief Does the work of simplification.
 *
 * \note Was Exp::polySimplify() ("polymorphic simplify").
 * \note ExpTransformer is/was intended to replace this.
 */
class PolySimplifier : public ExpModifier {
public:
	Exp *postVisit(Unary *) override;
	Exp *postVisit(Binary *) override;
	Exp *postVisit(Ternary *) override;
	Exp *postVisit(TypedExp *) override;
	Exp *postVisit(RefExp *) override;
	Exp *postVisit(Location *) override;
};

#define DEBUG_SIMP 0  // Set to 1 to print every change
/**
 * \brief Simplify the expression.
 *
 * Apply various simplifications such as constant folding.  Also canonicalise
 * by putting integer constants on the right hand side of sums, adding of
 * negative constants changed to subtracting positive constants, etc.  Changes
 * << k to a multiply.
 *
 * \note User must ;//delete result.
 * \note Address simplification (a[m[x]] == x) is done separately.
 *
 * \returns  Ptr to the simplified expression.
 *
 * This code is so big, so weird and so lame it's not funny.  What this boils
 * down to is the process of unification.  We're trying to do it with a simple
 * iterative algorithm, but the algorithm keeps getting more and more complex.
 * Eventually I will replace this with a simple theorem prover and we'll have
 * something powerful, but until then, don't rely on this code to do anything
 * critical. - trent 8/7/2002
 */
Exp *
Exp::simplify()
{
#if DEBUG_SIMP
	Exp *save = clone();
#endif
	Exp *res = this;
	//res = ExpTransformer::applyAllTo(res, bMod);
	//return res;
	PolySimplifier ps;
	do {
		ps.clearMod();
		//Exp *before = res->clone();
		res = res->accept(ps);
#if 0
		if (ps.isMod()) {
			LOG << "simplify hit: " << *before << " to " << *res << "\n";
			// simplify is now redundant, if you see this in the log you need to update one of the files in the
			// transformations directory to include a rule for the reported transform.
		}
#endif
	} while (ps.isMod());  // If modified at this (or a lower) level, redo
	// The below is still important. E.g. want to canonicalise sums, so we know that a + K + b is the same as a + b + K
	// No! This slows everything down, and it's slow enough as it is. Call only where needed:
	// res = res->simplifyArith();
#if DEBUG_SIMP
	if (!(*res == *save)) std::cout << "simplified " << save << "  to  " << res << "\n";
	;//delete save;
#endif
	return res;
}

Exp *
PolySimplifier::postVisit(Unary *e)
{
	auto op = e->getOper();
	auto eSub1 = e->getSubExp1();
	auto opSub1 = eSub1->getOper();

	if (op == opNot || op == opLNot) {
		switch (opSub1) {
		case opEqual:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opNotEqual);
			delete e;
			return eSub1;
		case opNotEqual:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opEqual);
			delete e;
			return eSub1;
		case opLess:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opGtrEq);
			delete e;
			return eSub1;
		case opLessEq:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opGtr);
			delete e;
			return eSub1;
		case opGtr:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opLessEq);
			delete e;
			return eSub1;
		case opGtrEq:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opLess);
			delete e;
			return eSub1;
		case opLessUns:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opGtrEqUns);
			delete e;
			return eSub1;
		case opLessEqUns:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opGtrUns);
			delete e;
			return eSub1;
		case opGtrUns:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opLessEqUns);
			delete e;
			return eSub1;
		case opGtrEqUns:
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			eSub1->setOper(opLessUns);
			delete e;
			return eSub1;
		default:
			break;
		}
	}

	switch (op) {
	case opNeg:
	case opNot:
	case opLNot:
		if (opSub1 == opIntConst) {
			// -k, ~k, or !k
			mod = true;
			auto k = (Const *)e->swapSubExp1(nullptr);
			auto n = k->getInt();
			switch (op) {
			case opNeg:  n = -n; break;
			case opNot:  n = ~n; break;
			case opLNot: n = !n; break;
			default: break;
			}
			k->setInt(n);
			delete e;
			return k;
		} else if (opSub1 == op) {
			// double negation
			mod = true;
			auto x = ((Unary *)eSub1)->swapSubExp1(nullptr);
			delete e;
			return x;
		}
		break;
	case opAddrOf:
		// check for a[m[x]], becomes x
		if (opSub1 == opMemOf) {
			auto x = ((Location *)eSub1)->swapSubExp1(nullptr);
			mod = true;
			delete e;
			return x;
		}
		break;
	default:
		break;
	}

	return e;
}

Exp *
PolySimplifier::postVisit(Binary *e)
{
	auto op = e->getOper();
	auto eSub1 = e->getSubExp1();
	auto eSub2 = e->getSubExp2();
	auto opSub1 = eSub1->getOper();
	auto opSub2 = eSub2->getOper();

	// x & x or x | x: result is x
	if ((op == opBitAnd || op == opBitOr)
	 && *eSub1 == *eSub2) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// x ^ x or x - x: result is zero
	if ((op == opBitXor || op == opMinus)
	 && *eSub1 == *eSub2) {
		mod = true;
		delete e;
		return new Const(0);
	}

	// x == x: result is true
	if (op == opEqual
	 && *eSub1 == *eSub2) {
		mod = true;
		delete e;
		return new Terminal(opTrue);
	}

	if (opSub1 == opIntConst
	 && opSub2 == opIntConst) {
		// k1 op k2, where k1 and k2 are integer constants
		auto k1 = ((Const *)eSub1)->getInt();
		auto k2 = ((Const *)eSub2)->getInt();
		bool change = true;
		switch (op) {
		case opPlus:      k1 = k1 + k2; break;
		case opMinus:     k1 = k1 - k2; break;
		case opDiv:       k1 = (unsigned)k1 / (unsigned)k2; break;
		case opDivs:      k1 = k1 / k2; break;
		case opMod:       k1 = (unsigned)k1 % (unsigned)k2; break;
		case opMods:      k1 = k1 % k2; break;
		case opMult:      k1 = (unsigned)k1 * (unsigned)k2; break;
		case opMults:     k1 = k1 * k2; break;
		case opShiftL:    k1 = k1 << k2; break;
		case opShiftR:    k1 = k1 >> k2; break;
		case opShiftRA:   k1 = (k1 >> k2) | (((1 << k2) - 1) << (32 - k2)); break;
		case opBitOr:     k1 = k1 | k2; break;
		case opBitAnd:    k1 = k1 & k2; break;
		case opBitXor:    k1 = k1 ^ k2; break;
		case opEqual:     k1 = k1 == k2; break;
		case opNotEqual:  k1 = k1 != k2; break;
		case opLess:      k1 = k1 <  k2; break;
		case opGtr:       k1 = k1 >  k2; break;
		case opLessEq:    k1 = k1 <= k2; break;
		case opGtrEq:     k1 = k1 >= k2; break;
		case opLessUns:   k1 = (unsigned)k1 <  (unsigned)k2; break;
		case opGtrUns:    k1 = (unsigned)k1 >  (unsigned)k2; break;
		case opLessEqUns: k1 = (unsigned)k1 <= (unsigned)k2; break;
		case opGtrEqUns:  k1 = (unsigned)k1 >= (unsigned)k2; break;
		default: change = false;
		}
		if (change) {
			mod = true;
			if (e->isComparison()) {
				delete e;
				return new Terminal(k1 ? opTrue : opFalse);
			}
			eSub1 = e->swapSubExp1(nullptr);
			((Const *)eSub1)->setInt(k1);
			delete e;
			return eSub1;
		}
	}

	// Might want to commute to put an integer constant on the RHS
	// Later simplifications can rely on this (ADD other ops as necessary)
	if (opSub1 == opIntConst
	 && opSub2 != opIntConst
	 && (op == opPlus   || op == opMult  || op == opMults
	  || op == opBitAnd || op == opBitOr || op == opBitXor
	  || op == opAnd    || op == opOr    || op == opEqual  || op == opNotEqual)) {
		eSub2 = e->swapSubExp1(eSub2);
		eSub1 = e->swapSubExp2(eSub1);
		// Swap opSub1 and opSub2 as well
		std::swap(opSub1, opSub2);
		// This is not counted as a modification
	}

	// Similarly for boolean constants
	if (eSub1->isBoolConst()
	 && !eSub2->isBoolConst()
	 && (op == opAnd || op == opOr || op == opEqual || op == opNotEqual)) {
		eSub2 = e->swapSubExp1(eSub2);
		eSub1 = e->swapSubExp2(eSub1);
		// Swap opSub1 and opSub2 as well
		std::swap(opSub1, opSub2);
		// This is not counted as a modification
	}

	// Similarly for adding stuff to the addresses of globals
	if (eSub2->isAddrOf()
	 && ((Unary *)eSub2)->getSubExp1()->isSubscript()
	 && ((RefExp *)((Unary *)eSub2)->getSubExp1())->getSubExp1()->isGlobal()
	 && op == opPlus) {
		eSub2 = e->swapSubExp1(eSub2);
		eSub1 = e->swapSubExp2(eSub1);
		// Swap opSub1 and opSub2 as well
		std::swap(opSub1, opSub2);
		// This is not counted as a modification
	}

	// Convenience casts
	auto bSub1 = dynamic_cast<Binary *>(eSub1);
	auto bSub2 = dynamic_cast<Binary *>(eSub2);
	auto kSub1 = dynamic_cast<Const *>(eSub1);
	auto kSub2 = dynamic_cast<Const *>(eSub2);

	// check for (x + a) + b where a and b are constants, becomes x + a+b
	if (op == opPlus
	 && opSub1 == opPlus
	 && opSub2 == opIntConst
	 && bSub1->getSubExp2()->isIntConst()) {
		auto kSub1Sub2 = (Const *)bSub1->getSubExp2();
		auto a = kSub1Sub2->getInt();
		auto b = kSub2->getInt();
		kSub1Sub2->setInt(a + b);
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// check for (x - a) + b where a and b are constants, becomes x + -a+b
	if (op == opPlus
	 && opSub1 == opMinus
	 && opSub2 == opIntConst
	 && bSub1->getSubExp2()->isIntConst()) {
		auto kSub1Sub2 = (Const *)bSub1->getSubExp2();
		auto a = kSub1Sub2->getInt();
		auto b = kSub2->getInt();
		bSub1->setOper(opPlus);
		kSub1Sub2->setInt(-a + b);
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// check for (x * k) - x, becomes x * (k-1)
	// same with +
	if ((op == opMinus || op == opPlus)
	 && (opSub1 == opMults || opSub1 == opMult)
	 && *eSub2 == *bSub1->getSubExp1()) {
		bSub1->setSubExp2(new Binary(op, bSub1->getSubExp2(), new Const(1)));
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// check for x + (x * k), becomes x * (k+1)
	if (op == opPlus
	 && (opSub2 == opMults || opSub2 == opMult)
	 && *eSub1 == *bSub2->getSubExp1()) {
		bSub2->setSubExp2(new Binary(op, bSub2->getSubExp2(), new Const(1)));
		mod = true;
		eSub2 = e->swapSubExp2(nullptr);
		delete e;
		return eSub2;
	}

	// Turn a + -K into a - K (K is int const > 0)
	// Also a - -K into a + K (K is int const > 0)
	// Does not count as a change
	if ((op == opPlus || op == opMinus)
	 && opSub2 == opIntConst
	 && kSub2->getInt() < 0) {
		kSub2->setInt(-kSub2->getInt());
		op = op == opPlus ? opMinus : opPlus;
		e->setOper(op);
	}

	// Check for exp + 0  or  exp - 0  or  exp | 0
	if ((op == opPlus || op == opMinus || op == opBitOr)
	 && opSub2 == opIntConst
	 && kSub2->getInt() == 0) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// Check for exp * 0  or exp & 0
	if ((op == opMult || op == opMults || op == opBitAnd)
	 && opSub2 == opIntConst
	 && kSub2->getInt() == 0) {
		mod = true;
		eSub2 = e->swapSubExp2(nullptr);
		delete e;
		return eSub2;
	}

	// Check for exp * 1 or exp / 1
	if ((op == opMult || op == opMults || op == opDiv || op == opDivs)
	 && opSub2 == opIntConst
	 && kSub2->getInt() == 1) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// Check for exp % 1, becomes 0
	if ((op == opMod || op == opMods)
	 && opSub2 == opIntConst
	 && kSub2->getInt() == 1) {
		mod = true;
		delete e;
		return new Const(0);
	}

	// Check for exp * x / x
	if ((op == opDiv || op == opDivs)
	 && (opSub1 == opMult || opSub1 == opMults)
	 && *eSub2 == *bSub1->getSubExp2()) {
		mod = true;
		auto eSub1Sub1 = bSub1->swapSubExp1(nullptr);
		delete e;
		return eSub1Sub1;
	}

	// Check for exp * x % x, becomes 0
	if ((op == opMod || op == opMods)
	 && (opSub1 == opMult || opSub1 == opMults)
	 && *eSub2 == *bSub1->getSubExp2()) {
		mod = true;
		delete e;
		return new Const(0);
	}

	// Check for exp AND -1 (bitwise AND)
	if ((op == opBitAnd)
	 && opSub2 == opIntConst
	 && kSub2->getInt() == -1) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// Check for [exp] << k where k is a positive integer const
	if (op == opShiftL
	 && opSub2 == opIntConst
	 && kSub2->getInt() >= 0
	 && kSub2->getInt() < 32) {
		e->setOper(opMult);
		kSub2->setInt(1 << kSub2->getInt());
		mod = true;
		return e;
	}

	if (op == opShiftR
	 && opSub2 == opIntConst
	 && kSub2->getInt() >= 0
	 && kSub2->getInt() < 32) {
		e->setOper(opDiv);
		kSub2->setInt(1 << kSub2->getInt());
		mod = true;
		return e;
	}

	// Check for exp || false
	if (op == opOr
	 && eSub2->isFalse()) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// Check for exp && true
	if (op == opAnd
	 && eSub2->isTrue()) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// Check for exp && false
	if (op == opAnd
	 && eSub2->isFalse()) {
		mod = true;
		delete e;
		return new Terminal(opFalse);
	}

	// Check for exp || true
	if (op == opOr
	 && eSub2->isTrue()) {
		mod = true;
		delete e;
		return new Terminal(opTrue);
	}

	// Check for (x compare y) == true, becomes x compare y
	if (op == opEqual
	 && eSub2->isTrue()
	 && eSub1->isComparison()) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// Check for (x compare y) != false, becomes x compare y
	if (op == opNotEqual
	 && eSub2->isFalse()
	 && eSub1->isComparison()) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// Check for (x compare y) != true, becomes !(x compare y)
	if (op == opNotEqual
	 && eSub2->isTrue()
	 && eSub1->isComparison()) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return new Unary(opLNot, eSub1);
	}

	// Check for (x compare y) == false, becomes !(x compare y)
	if (op == opEqual
	 && eSub2->isFalse()
	 && eSub1->isComparison()) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return new Unary(opLNot, eSub1);
	}

#if 0
	// Check for -x compare y, becomes x compare -y
	// FIXME: Likely need to swap < for >, <= for >=, etc.
	// FIXME: If y is also negated, next pass will swap them back.
	if (e->isComparison()
	 && opSub1 == opNeg) {
		auto x = ((Unary *)eSub1)->swapSubExp1(eSub2);
		auto ny = e->swapSubExp1(x);
		e->setSubExp2(ny);
		mod = true;
		return e;
	}

	// Check for (x + y) compare 0, becomes x compare -y
	if (e->isComparison()
	 && opSub2 == opIntConst && kSub2->getInt() == 0
	 && opSub1 == opPlus) {
		auto x = bSub1->swapSubExp1(nullptr);
		auto y = bSub1->swapSubExp2(nullptr);
		delete e->swapSubExp1(x);
		delete e->swapSubExp2(new Unary(opNeg, y));
		mod = true;
		return e;
	}
#endif

	// Check for x + -K == 0, becomes x == k
	// FIXME: eSub1 gets simplified to x - K above, should never execute
	if (op == opEqual
	 && opSub2 == opIntConst
	 && kSub2->getInt() == 0
	 && opSub1 == opPlus
	 && bSub1->getSubExp2()->isIntConst()) {
		auto k = (Const *)bSub1->getSubExp2();
		auto n = k->getInt();
		if (n < 0) {
			auto x = bSub1->swapSubExp1(nullptr);
			delete e->swapSubExp1(x);
			kSub2->setInt(-n);
			mod = true;
			return e;
		}
	}

	// Check for (0 - x) != 0, becomes x != 0
	if (op == opNotEqual
	 && opSub2 == opIntConst
	 && kSub2->getInt() == 0
	 && opSub1 == opMinus
	 && bSub1->getSubExp1()->isIntConst()
	 && ((Const *)bSub1->getSubExp1())->getInt() == 0) {
		auto x = bSub1->swapSubExp2(nullptr);
		delete e->swapSubExp1(x);
		mod = true;
		return e;
	}

	// Check for (x <= y) || (x == y), becomes x <= y
	if (op == opOr
	 && opSub2 == opEqual
	 && (opSub1 == opGtrEq || opSub1 == opLessEq || opSub1 == opGtrEqUns || opSub1 == opLessEqUns)
	 && ((*bSub1->getSubExp1() == *bSub2->getSubExp1() && *bSub1->getSubExp2() == *bSub2->getSubExp2())
	  || (*bSub1->getSubExp1() == *bSub2->getSubExp2() && *bSub1->getSubExp2() == *bSub2->getSubExp1()))) {
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	// check for a*n*m, becomes a*(n*m) where n and m are ints
	if (op == opMult
	 && opSub1 == opMult
	 && opSub2 == opIntConst
	 && bSub1->getSubExp2()->isIntConst()) {
		auto n = (Const *)bSub1->getSubExp2();
		n->setInt(n->getInt() * kSub2->getInt());
		mod = true;
		eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

#if 0  // FIXME! ADHOC TA assumed!
	// check for (exp + x) + n where exp is a pointer to a compound type becomes (exp + n) + x
	if (op == opPlus
	 && opSub1 == opPlus
	 && eSub2->isIntConst()) {
		Type *ty = bSub1->getSubExp1()->getType();
		if (ty
		 && ty->resolvesToPointer()
		 && ty->asPointer()->getPointsTo()->resolvesToCompound()) {
			auto x = bSub1->swapSubExp2(eSub2);
			e->setSubExp2(x);
			mod = true;
			return e;
		}
	}
#endif

	// FIXME: suspect this was only needed for ADHOC TA
	// check for exp + n where exp is a pointer to a compound type
	// becomes &m[exp].m + r where m is the member at offset n and r is n - the offset to member m
	Type *ty = nullptr;  // Type of eSub1
	if (auto re = dynamic_cast<RefExp *>(eSub1))
		if (auto def = re->getDef())
			ty = def->getTypeFor(re->getSubExp1());
	if (op == opPlus
	 && ty
	 && ty->resolvesToPointer()
	 && ty->asPointer()->getPointsTo()->resolvesToCompound()
	 && opSub2 == opIntConst) {
		auto n = (unsigned)kSub2->getInt();
		CompoundType *c = ty->asPointer()->getPointsTo()->asCompound();
		if (n * 8 < c->getSize()) {
			unsigned r = c->getOffsetRemainder(n * 8);
			assert((r % 8) == 0);
			const char *nam = c->getNameAtOffset(n * 8);
			if (nam && std::string("pad") != nam) {
				auto l = Location::memOf(eSub1);
				//l->setType(c);
				e->setSubExp1(new Unary(opAddrOf,
				                        new Binary(opMemberAccess,
				                                   l,
				                                   new Const(strdup(nam)))));
				kSub2->setInt(r / 8);
				mod = true;
				return e;
			}
		}
	}

#if 0  // FIXME: ADHOC TA assumed
	// check for exp + x where exp is a pointer to an array
	// becomes &exp[x / b] + (x % b) where b is the size of the base type in bytes
	if (op == opPlus
	 && eSub1->getType()) {
		auto x = eSub2;
		auto l = eSub1;
		auto ty = l->getType();
		if (ty
		 && ty->resolvesToPointer()
		 && ty->asPointer()->getPointsTo()->resolvesToArray()) {
			ArrayType *a = ty->asPointer()->getPointsTo()->asArray();
			int b = a->getBaseType()->getSize() / 8;
			int br = a->getBaseType()->getSize() % 8;
			assert(br == 0);
			if (!x->isIntConst() || ((Const *)x)->getInt() >= b || a->getBaseType()->isArray()) {
				e->setSubExp1(new Unary(opAddrOf,
				                        new Binary(opArrayIndex,
				                                   Location::memOf(l),
				                                   new Binary(opDiv, x->clone(), new Const(b)))));
				e->setSubExp2(new Binary(opMod, x, new Const(b)));
				if (auto r = dynamic_cast<RefExp *>(l)) {
					if (auto pa = dynamic_cast<PhiAssign *>(r->getDef())) {
						LOG << "argh: " << *pa->getAt(1).def << "\n";
					}
				}
				mod = true;
				return e;
			}
		}
	}
#endif

	// Check for 0.0 - x, becomes -x
	if (op == opFMinus
	 && eSub1->isFltConst()
	 && kSub1->getFlt() == 0.0) {
		mod = true;
		eSub2 = e->swapSubExp2(nullptr);
		delete e;
		return new Unary(opFNeg, eSub2);
	}

	// Check for (x * k) + k, becomes (x + 1) * k
	if ((op == opPlus || op == opMinus)
	 && (opSub1 == opMults || opSub1 == opMult)
	 && eSub2->isIntConst()
	 && bSub1->getSubExp2()->isIntConst()) {
		auto k = (Const *)bSub1->getSubExp2();
		if (k->getInt() == kSub2->getInt()) {
			auto x = bSub1->swapSubExp1(e);
			e->setSubExp1(x);
			kSub2->setInt(1);
			mod = true;
			return e;
		}
	}

	// Check for (x + (y * k)) + k, becomes x + ((y + 1) * k)
	if ((op == opPlus || op == opMinus)
	 && opSub1 == opPlus
	 && eSub2->isIntConst()
	 && (bSub1->getSubExp2()->getOper() == opMults || bSub1->getSubExp2()->getOper() == opMult)
	 && ((Binary *)bSub1->getSubExp2())->getSubExp2()->isIntConst()) {
		auto bSub1Sub2 = (Binary *)bSub1->getSubExp2();
		auto k = (Const *)bSub1Sub2->getSubExp2();
		if (k->getInt() == kSub2->getInt()) {
			auto y = bSub1Sub2->swapSubExp1(e);
			e->setSubExp1(y);
			kSub2->setInt(1);
			mod = true;
			return bSub1;
		}
	}

	// check for ((x * a) + (y * b)) / c where a, b and c are all integers and a and b divide evenly by c
	// becomes: (x * a/c) + (y * b/c)
	if (op == opDiv
	 && eSub1->getOper() == opPlus
	 && eSub2->isIntConst()
	 && bSub1->getSubExp1()->getOper() == opMult
	 && bSub1->getSubExp2()->getOper() == opMult
	 && ((Binary *)bSub1->getSubExp1())->getSubExp2()->isIntConst()
	 && ((Binary *)bSub1->getSubExp2())->getSubExp2()->isIntConst()) {
		auto ka = (Const *)((Binary *)bSub1->getSubExp1())->getSubExp2();
		auto kb = (Const *)((Binary *)bSub1->getSubExp2())->getSubExp2();
		auto a = ka->getInt();
		auto b = kb->getInt();
		auto c = kSub2->getInt();
		if ((a % c) == 0 && (b % c) == 0) {
			ka->setInt(a / c);
			kb->setInt(b / c);
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			delete e;
			return eSub1;
		}
	}

	// check for ((x * a) + (y * b)) % c where a, b and c are all integers
	// becomes: (y * b) % c if a divides evenly by c
	// becomes: (x * a) % c if b divides evenly by c
	// becomes: 0           if both a and b divide evenly by c
	if (op == opMod
	 && eSub1->getOper() == opPlus
	 && eSub2->isIntConst()
	 && bSub1->getSubExp1()->getOper() == opMult
	 && bSub1->getSubExp2()->getOper() == opMult
	 && bSub1->getSubExp1()->getSubExp2()->isIntConst()
	 && bSub1->getSubExp2()->getSubExp2()->isIntConst()) {
		auto ka = (Const *)((Binary *)bSub1->getSubExp1())->getSubExp2();
		auto kb = (Const *)((Binary *)bSub1->getSubExp2())->getSubExp2();
		auto a = ka->getInt();
		auto b = kb->getInt();
		auto c = kSub2->getInt();
		a %= c;
		b %= c;
		if (a == 0) {
			delete bSub1->swapSubExp1(nullptr);
		}
		if (b == 0) {
			delete bSub1->swapSubExp2(nullptr);
		}
		if (a == 0 && b == 0) {
			kSub2->setInt(0);
			mod = true;
			eSub2 = e->swapSubExp2(nullptr);
			delete e;
			return eSub2;
		}
		if (a == 0) {
			delete e->swapSubExp1(bSub1->swapSubExp2(nullptr));
			mod = true;
			return e;
		}
		if (b == 0) {
			delete e->swapSubExp1(bSub1->swapSubExp1(nullptr));
			mod = true;
			return e;
		}
	}

	// Check for 0 - (0 <u exp1) & exp2 => exp2
	if (op == opBitAnd
	 && opSub1 == opMinus) {
		auto leftOfMinus = bSub1->getSubExp1();
		auto rightOfMinus = bSub1->getSubExp2();
		if (leftOfMinus->isIntConst()
		 && ((Const *)leftOfMinus)->getInt() == 0
		 && rightOfMinus->getOper() == opLessUns) {
			auto leftOfLess = ((Binary *)rightOfMinus)->getSubExp1();
			if (leftOfLess->isIntConst()
			 && ((Const *)leftOfLess)->getInt() == 0) {
				mod = true;
				eSub2 = e->swapSubExp2(nullptr);
				delete e;
				return eSub2;
			}
		}
	}

	// Replace opSize(n, loc) with loc and set the type if needed
	if (op == opSize
	 && dynamic_cast<Location *>(eSub2)) {
#if 0  // FIXME: ADHOC TA assumed here
		auto loc = (Location *)eSub2;
		auto n = (unsigned)kSub1->getInt();
		Type *ty = loc->getType();
		if (!ty)
			loc->setType(new SizeType(n));
		else if (ty->getSize() != n)
			ty->setSize(n);
#endif
		mod = true;
		eSub2 = e->swapSubExp2(nullptr);
		delete e;
		return eSub2;
	}

	return e;
}

Exp *
PolySimplifier::postVisit(Ternary *e)
{
	auto op = e->getOper();
	auto eSub1 = e->getSubExp1();
	auto eSub2 = e->getSubExp2();
	auto eSub3 = e->getSubExp3();
	auto opSub1 = eSub1->getOper();
	auto opSub2 = eSub2->getOper();
	auto opSub3 = eSub3->getOper();

	// p ? 1 : 0 -> p
	if (op == opTern
	 && eSub2->isIntConst()
	 && eSub3->isIntConst()) {
		auto k2 = (Const *)eSub2;
		auto k3 = (Const *)eSub3;
		if (k2->getInt() == 1 && k3->getInt() == 0) {
			mod = true;
			eSub1 = e->swapSubExp1(nullptr);
			delete e;
			return eSub1;
		}
	}

	// 1 ? x : y -> x
	if (op == opTern
	 && eSub1->isTrue()) {
		mod = true;
		eSub2 = e->swapSubExp2(nullptr);
		delete e;
		return eSub2;
	}

	// 0 ? x : y -> y
	if (op == opTern
	 && eSub1->isFalse()) {
		mod = true;
		eSub3 = e->swapSubExp3(nullptr);
		delete e;
		return eSub3;
	}

	if ((op == opSgnEx || op == opZfill)
	 && eSub3->isIntConst()) {
		mod = true;
		eSub3 = e->swapSubExp3(nullptr);
		delete e;
		return eSub3;
	}

	if (op == opFsize
	 && opSub3 == opItof
	 && *eSub1 == *eSub3->getSubExp2()
	 && *eSub2 == *eSub3->getSubExp1()) {
		mod = true;
		eSub3 = e->swapSubExp3(nullptr);
		delete e;
		return eSub3;
	}

	if (op == opFsize
	 && eSub3->isFltConst()) {
		mod = true;
		eSub3 = e->swapSubExp3(nullptr);
		delete e;
		return eSub3;
	}

	if (op == opItof
	 && eSub3->isIntConst()
	 && eSub2->isIntConst()
	 && ((Const *)eSub2)->getInt() == 32) {
		mod = true;
		unsigned n = ((Const *)eSub3)->getInt();
		delete e;
		return new Const(*(float *)&n);
	}

	if (op == opFsize
	 && eSub3->isMemOfK()) {
		auto l = static_cast<Location *>(eSub3);
		unsigned u = ((Const *)l->getSubExp1())->getInt();
		if (auto p = l->getProc()) {
			Prog *prog = p->getProg();
			bool ok;
			double d = prog->getFloatConstant(u, ok, ((Const *)eSub1)->getInt());
			if (ok) {
				mod = true;
				eSub3 = e->swapSubExp3(new Const(d));
				delete eSub3;
				return e;
			}
		}
	}

	if (op == opTruncu
	 && eSub3->isIntConst()) {
		int from         = ((Const *)eSub1)->getInt();
		int to           = ((Const *)eSub2)->getInt();
		unsigned int val = ((Const *)eSub3)->getInt();
		if (from == 32) {
			if (to == 16) {
				mod = true;
				((Const *)eSub3)->setInt(val & 0xffff);
				eSub3 = e->swapSubExp3(nullptr);
				delete e;
				return eSub3;
			}
			if (to == 8) {
				mod = true;
				((Const *)eSub3)->setInt(val & 0xff);
				eSub3 = e->swapSubExp3(nullptr);
				delete e;
				return eSub3;
			}
		}
	}

	if (op == opTruncs
	 && eSub3->isIntConst()) {
		int from = ((Const *)eSub1)->getInt();
		int to   = ((Const *)eSub2)->getInt();
		int val  = ((Const *)eSub3)->getInt();
		if (from == 32) {
			if (to == 16) {
				mod = true;
				((Const *)eSub3)->setInt(val & 0xffff);
				eSub3 = e->swapSubExp3(nullptr);
				delete e;
				return eSub3;
			}
			if (to == 8) {
				mod = true;
				((Const *)eSub3)->setInt(val & 0xff);
				eSub3 = e->swapSubExp3(nullptr);
				delete e;
				return eSub3;
			}
		}
	}

	return e;
}

Exp *
PolySimplifier::postVisit(TypedExp *e)
{
	if (e->getSubExp1()->isRegOf()) {
		// type cast on a reg of.. hmm.. let's remove this
		mod = true;
		auto eSub1 = e->swapSubExp1(nullptr);
		delete e;
		return eSub1;
	}

	return e;
}

Exp *
PolySimplifier::postVisit(RefExp *e)
{
	/* This is a nasty hack.  We assume that %DF{0} is 0.  This happens when string instructions are used without first
	 * clearing the direction flag.  By convention, the direction flag is assumed to be clear on entry to a procedure. */
	if (e->getSubExp1()->getOper() == opDF && !e->getDef()) {
		mod = true;
		delete e;
		return new Const(0);
	}

	// another hack, this time for aliasing
	// FIXME: do we really want this now? Pentium specific, and only handles ax/eax (not al or ah)
	auto as = dynamic_cast<Assign *>(e->getDef());
	if (e->getSubExp1()->isRegN(0)  // r0 (ax)
	 && as
	 && as->getLeft()->isRegN(24)) {  // r24 (eax)
		mod = true;
		auto r = (Location *)e->getSubExp1();
		auto N = (Const *)r->getSubExp1();
		N->setInt(24);
		return new TypedExp(new IntegerType(16), e);
	}

	// Was code here for bypassing phi statements that are now redundant

	return e;
}

Exp *
PolySimplifier::postVisit(Location *e)
{
	// m[a[x]] becomes x
	if (e->isMemOf() && e->getSubExp1()->isAddrOf()) {
		mod = true;
		auto x = ((Unary *)e->getSubExp1())->swapSubExp1(nullptr);
		delete e;
		return x;
	}

	return e;
}

/**
 * \note Preserves the existing Exp::simplifyAddr() behavior of top-down
 * search and not recursing into any replacement.  Should reconsider this.
 */
class AddrSimplifier : public ExpModifier {
public:
	Exp *preVisit(Unary *, bool &) override;
	Exp *preVisit(Location *, bool &) override;
};

/**
 * Converts m[a[any]] -> any.
 */
Exp *
AddrSimplifier::preVisit(Location *e, bool &recurse)
{
	if (e->isMemOf()) {
		auto sub = e->getSubExp1();
		if (sub->isAddrOf()) {
			recurse = false;
			auto ret = ((Unary *)sub)->swapSubExp1(nullptr);
			delete e;
			return ret;
		}
	}
	return e;
}

/**
 * Converts a[m[any]] -> any, and also a[size m[any]] -> any.
 */
Exp *
AddrSimplifier::preVisit(Unary *e, bool &recurse)
{
	if (e->isAddrOf()) {
		auto sub = e->getSubExp1();
		if (sub->isMemOf()) {
			recurse = false;
			auto ret = ((Location *)sub)->swapSubExp1(nullptr);
			delete e;
			return ret;
		}
		if (sub->getOper() == opSize) {
			auto sub2 = ((Binary *)sub)->getSubExp2();
			if (sub2->isMemOf()) {
				recurse = false;
				auto ret = ((Location *)sub2)->swapSubExp1(nullptr);
				delete e;
				return ret;
			}
		}
	}
	return e;
}

/**
 * \brief Just the address simplification.
 *
 * \returns  Ptr to the simplified expression.
 */
Exp *
Exp::simplifyAddr()
{
	AddrSimplifier as;
	return accept(as);
}

/**
 * Output operator for Exp*.
 *
 * \param[in] os  Output stream to send to.
 * \param[in] p   Ptr to Exp to print to the stream.
 *
 * \returns os (for concatenation).
 */
std::ostream &
operator <<(std::ostream &os, const Exp *p)
{
	return os << *p;
}
std::ostream &
operator <<(std::ostream &os, const Exp &p)
{
	p.print(os);
	return os;
}

/**
 * Replace succ(r[k]) by r[k+1].
 * Example:  succ(r2) -> r3.
 *
 * \note Could change top level expression.
 *
 * \returns  Fixed expression.
 */
Exp *
Exp::fixSuccessor()
{
	bool change;
	Exp *result;
	// Assume only one successor function in any 1 expression
	if (search(new Unary(opSuccessor, new Terminal(opWildRegOf)), result)) {
		// Result has the matching expression, i.e. succ(r[K])
		auto sub1 = ((Unary *)result)->getSubExp1();
		assert(sub1->isRegOfK());
		// result    sub1   sub2
		// succ(      r[   Const K  ])
		// Note: we need to clone the r[K] part, since it will be ;//deleted as
		// part of the searchReplace below
		auto replace = (Location *)sub1->clone();
		auto c = (Const *)replace->getSubExp1();
		c->setInt(c->getInt() + 1);  // Do the increment
		return searchReplace(result, replace, change);
	}
	return this;
}

static Ternary srch1(opZfill, new Terminal(opWild), new Terminal(opWild), new Terminal(opWild));
static Ternary srch2(opSgnEx, new Terminal(opWild), new Terminal(opWild), new Terminal(opWild));
/**
 * \brief Kill any zero fill, sign extend, or truncates.
 *
 * Remove size operations such as zero fill, sign extend.
 *
 * \note Could change top level expression.
 * \note Does not handle truncation at present.
 *
 * \returns  Fixed expression.
 */
Exp *
Exp::killFill()
{
	Exp *res = this;
	std::list<Exp **> result;
	doSearch(&srch1, res, result, false);
	doSearch(&srch2, res, result, false);
	for (const auto &pp : result) {
		// Kill the sign extend bits
		*pp = ((Ternary *)(*pp))->getSubExp3();
	}
	return res;
}

bool
Exp::isTemp() const
{
	if (op == opTemp) return true;
	if (!isRegOf()) return false;
	// Some old code has r[tmpb] instead of just tmpb
	const Exp *sub = ((const Location *)this)->getSubExp1();
	return sub->op == opTemp;
}

/**
 * \param allZero  Set if all subscripts in the whole expression are null or
 *                 implicit; otherwise cleared.
 */
Exp *
Exp::removeSubscripts(bool &allZero)
{
	Exp *e = this;
	LocationSet locs;
	e->addUsedLocs(locs);
	allZero = true;
	for (const auto &xx : locs) {
		if (auto r1 = dynamic_cast<RefExp *>(xx)) {
			Statement *def = r1->getDef();
			if (!(!def || def->getNumber() == 0)) {
				allZero = false;
			}
			bool change;
			e = e->searchReplaceAll(r1, r1->getSubExp1()/*->clone()*/, change);
		}
	}
	return e;
}

/**
 * Convert from SSA form, where this is not subscripted (but defined at
 * statement d).  Needs the UserProc for the symbol map.
 *
 * FIXME:  If the wrapped expression does not convert to a location, the
 *         result is subscripted, which is probably not what is wanted!
 */
Exp *
Exp::fromSSAleft(UserProc *proc, Statement *d)
{
	auto r = new RefExp(this, d);  // "Wrap" in a ref
	ExpSsaXformer esx(proc);
	return r->accept(esx);
}

// A helper class for comparing Exp*'s sensibly
bool
lessExpStar::operator ()(const Exp *x, const Exp *y) const
{
	return (*x < *y);  // Compare the actual Exps
}

bool
lessTI::operator ()(const Exp *x, const Exp *y) const
{
	return (*x << *y);  // Compare the actual Exps
}

/**
 * Generate constraints for this Exp.
 *
 * \note The behaviour is a bit different depending on whether or not
 * parameter result is a type constant or a type variable.
 *
 * \retval true  if the constraint is always satisfied.
 * \retval false if the constraint can never be satisfied.
 *
 * Example:  This is opMinus and result is \<int\>, constraints are:
 *   sub1 = \<int\> and sub2 = \<int\> or
 *   sub1 = \<ptr\> and sub2 = \<ptr\>
 *
 * Example:  This is opMinus and result is Tr (typeOf r), constraints are:
 *   sub1 = \<int\> and sub2 = \<int\> and Tr = \<int\> or
 *   sub1 = \<ptr\> and sub2 = \<ptr\> and Tr = \<int\> or
 *   sub1 = \<ptr\> and sub2 = \<int\> and Tr = \<ptr\>
 */
Exp *
Exp::genConstraints(Exp *result)
{
	// Default case, no constraints -> return true
	return new Terminal(opTrue);
}

Exp *
Const::genConstraints(Exp *result)
{
	if (auto tv = dynamic_cast<TypeVal *>(result)) {
		// result is a constant type, or possibly a partial type such as ptr(alpha)
		auto t = tv->getType();
		bool match = false;
		switch (op) {
		case opLongConst:
			// An integer constant is compatible with any size of integer, as long is it is in the right range
			// (no test yet) FIXME: is there an endianness issue here?
		case opIntConst:
			match = t->isInteger();
			// An integer constant can also match a pointer to something.  Assume values less than 0x100 can't be a
			// pointer
			if ((unsigned)u.i >= 0x100)
				match |= t->isPointer();
			// We can co-erce 32 bit constants to floats
			match |= t->isFloat();
			break;
		case opStrConst:
			match = (t->isPointer())
			     && (((PointerType *)t)->getPointsTo()->isChar()
			      || (((PointerType *)t)->getPointsTo()->isArray()
			       && ((ArrayType *)((PointerType *)t)->getPointsTo())->getBaseType()->isChar()));
			break;
		case opFltConst:
			match = t->isFloat();
			break;
		default:
			break;
		}
		if (match)
			// This constant may require a cast or a change of format. So we generate a constraint.
			// Don't clone 'this', so it can be co-erced after type analysis
			return new Binary(opEqual,
			                  new Unary(opTypeOf, this),
			                  tv->clone());
		else
			// Doesn't match
			return new Terminal(opFalse);
	}
	// result is a type variable, which is constrained by this constant
	Type *t;
	switch (op) {
	case opIntConst:
		{
			// We have something like local1 = 1234.  Either they are both integer, or both pointer
			Type *intt = new IntegerType(0);
			Type *alph = PointerType::newPtrAlpha();
			return new Binary(opOr,
			                  new Binary(opAnd,
			                             new Binary(opEqual,
			                                        result->clone(),
			                                        new TypeVal(intt)),
			                             new Binary(opEqual,
			                                        new Unary(opTypeOf,
			                                                  // Note: don't clone 'this', so we can change the Const after type analysis!
			                                                  this),
			                                        new TypeVal(intt))),
			                  new Binary(opAnd,
			                             new Binary(opEqual,
			                                        result->clone(),
			                                        new TypeVal(alph)),
			                             new Binary(opEqual,
			                                        new Unary(opTypeOf,
			                                                  this),
			                                        new TypeVal(alph))));
		}
		break;
	case opLongConst:
		t = new IntegerType(64);
		break;
	case opStrConst:
		t = new PointerType(new CharType());
		break;
	case opFltConst:
		t = new FloatType();  // size is not known. Assume double for now
		break;
	default:
		return nullptr;
	}
	return new Binary(opEqual,
	                  result->clone(),
	                  new TypeVal(t));
}

Exp *
Unary::genConstraints(Exp *result)
{
	if (dynamic_cast<TypeVal *>(result)) {
		// TODO: need to check for conflicts
		return new Terminal(opTrue);
	}

	switch (op) {
	case opRegOf:
	case opParam:  // Should be no params at constraint time
	case opGlobal:
	case opLocal:
		return new Binary(opEqual,
		                  new Unary(opTypeOf, this->clone()),
		                  result->clone());
	default:
		break;
	}
	return new Terminal(opTrue);
}

Exp *
Ternary::genConstraints(Exp *result)
{
	Type *argHasToBe = nullptr;
	Type *retHasToBe = nullptr;
	switch (op) {
	case opFsize:
	case opItof:
	case opFtoi:
	case opSgnEx:
		{
			assert(subExp1->isIntConst());
			assert(subExp2->isIntConst());
			int fromSize = ((Const *)subExp1)->getInt();
			int   toSize = ((Const *)subExp2)->getInt();
			// Fall through
			switch (op) {
			case opFsize:
				argHasToBe = new FloatType(fromSize);
				retHasToBe = new FloatType(toSize);
				break;
			case opItof:
				argHasToBe = new IntegerType(fromSize);
				retHasToBe = new FloatType(toSize);
				break;
			case opFtoi:
				argHasToBe = new FloatType(fromSize);
				retHasToBe = new IntegerType(toSize);
				break;
			case opSgnEx:
				argHasToBe = new IntegerType(fromSize);
				retHasToBe = new IntegerType(toSize);
				break;
			default:
				break;
			}
		}
	default:
		break;
	}
	Exp *res = nullptr;
	if (retHasToBe) {
		if (auto tv = dynamic_cast<TypeVal *>(result)) {
			// result is a constant type, or possibly a partial type such as
			// ptr(alpha)
			auto t = tv->getType();
			// Compare broad types
			if (!(*retHasToBe *= *t))
				return new Terminal(opFalse);
			// else just constrain the arg
		} else {
			// result is a type variable, constrained by this Ternary
			res = new Binary(opEqual,
			                 result,
			                 new TypeVal(retHasToBe));
		}
	}
	if (argHasToBe) {
		// Constrain the argument
		Exp *con = subExp3->genConstraints(new TypeVal(argHasToBe));
		if (res) res = new Binary(opAnd, res, con);
		else res = con;
	}
	if (!res)
		return new Terminal(opTrue);
	return res;
}

Exp *
RefExp::genConstraints(Exp *result)
{
	OPER subOp = subExp1->getOper();
	switch (subOp) {
	case opRegOf:
	case opParam:
	case opGlobal:
	case opLocal:
		return new Binary(opEqual,
		                  new Unary(opTypeOf, this->clone()),
		                  result->clone());
	default:
		break;
	}
	return new Terminal(opTrue);
}

// Return a constraint that my subexpressions have to be of type typeval1 and typeval2 respectively
Exp *
Binary::constrainSub(TypeVal *typeVal1, TypeVal *typeVal2)
{
	assert(subExp1 && subExp2);

	Exp *con1 = subExp1->genConstraints(typeVal1);
	Exp *con2 = subExp2->genConstraints(typeVal2);
	return new Binary(opAnd, con1, con2);
}

Exp *
Binary::genConstraints(Exp *result)
{
	assert(subExp1 && subExp2);

	Type *restrictTo = nullptr;
	if (auto tv = dynamic_cast<TypeVal *>(result))
		restrictTo = tv->getType();
	Exp *res = nullptr;
	auto intType = new IntegerType(0);  // Wild size (=0)
	TypeVal intVal(intType);
	switch (op) {
	case opFPlus:
	case opFMinus:
	case opFMult:
	case opFDiv:
		{
			if (restrictTo && !restrictTo->isFloat())
				// Result can only be float
				return new Terminal(opFalse);

			// MVE: what about sizes?
			auto ft = new FloatType();
			auto ftv = new TypeVal(ft);
			res = constrainSub(ftv, ftv);
			if (!restrictTo)
				// Also constrain the result
				res = new Binary(opAnd,
				                 res,
				                 new Binary(opEqual, result->clone(), ftv));
			return res;
		}
		break;

	case opBitAnd:
	case opBitOr:
	case opBitXor:
		{
			if (restrictTo && !restrictTo->isInteger())
				// Result can only be integer
				return new Terminal(opFalse);

			// MVE: What about sizes?
			auto it = new IntegerType();
			auto itv = new TypeVal(it);
			res = constrainSub(itv, itv);
			if (!restrictTo)
				// Also constrain the result
				res = new Binary(opAnd,
				                 res,
				                 new Binary(opEqual, result->clone(), itv));
			return res;
		}
		break;

	case opPlus:
		{
			// A pointer to anything
			Type *ptrType = PointerType::newPtrAlpha();
			TypeVal ptrVal(ptrType);  // Type value of ptr to anything
			if (!restrictTo || restrictTo->isInteger()) {
				// int + int -> int
				res = constrainSub(&intVal, &intVal);
				if (!restrictTo)
					res = new Binary(opAnd,
					                 res,
					                 new Binary(opEqual,
					                            result->clone(),
					                            intVal.clone()));
			}

			if (!restrictTo || restrictTo->isPointer()) {
				// ptr + int -> ptr
				Exp *res2 = constrainSub(&ptrVal, &intVal);
				if (!restrictTo)
					res2 = new Binary(opAnd,
					                  res2,
					                  new Binary(opEqual,
					                             result->clone(),
					                             ptrVal.clone()));
				if (res) res = new Binary(opOr, res, res2);
				else     res = res2;

				// int + ptr -> ptr
				res2 = constrainSub(&intVal, &ptrVal);
				if (!restrictTo)
					res2 = new Binary(opAnd,
					                  res2,
					                  new Binary(opEqual,
					                             result->clone(),
					                             ptrVal.clone()));
				if (res) res = new Binary(opOr, res, res2);
				else     res = res2;
			}

			if (res) return res->simplify();
			else return new Terminal(opFalse);
		}

	case opMinus:
		{
			Type *ptrType = PointerType::newPtrAlpha();
			TypeVal ptrVal(ptrType);
			if (!restrictTo || restrictTo->isInteger()) {
				// int - int -> int
				res = constrainSub(&intVal, &intVal);
				if (!restrictTo)
					res = new Binary(opAnd,
					                 res,
					                 new Binary(opEqual,
					                            result->clone(),
					                            intVal.clone()));

				// ptr - ptr -> int
				Exp *res2 = constrainSub(&ptrVal, &ptrVal);
				if (!restrictTo)
					res2 = new Binary(opAnd,
					                  res2,
					                  new Binary(opEqual,
					                             result->clone(),
					                             intVal.clone()));
				if (res) res = new Binary(opOr, res, res2);
				else     res = res2;
			}

			if (!restrictTo || restrictTo->isPointer()) {
				// ptr - int -> ptr
				Exp *res2 = constrainSub(&ptrVal, &intVal);
				if (!restrictTo)
					res2 = new Binary(opAnd,
					                  res2,
					                  new Binary(opEqual,
					                             result->clone(),
					                             ptrVal.clone()));
				if (res) res = new Binary(opOr, res, res2);
				else     res = res2;
			}

			if (res) return res->simplify();
			else return new Terminal(opFalse);
		}

	case opSize:
		{
			// This used to be considered obsolete, but now, it is used to carry the size of memOf's from the decoder to
			// here
			assert(subExp1->isIntConst());
			int sz = ((Const *)subExp1)->getInt();
			if (restrictTo) {
				int rsz = restrictTo->getSize();
				if (rsz == 0) {
					// This is now restricted to the current restrictTo, but
					// with a known size
					Type *it = restrictTo->clone();
					it->setSize(sz);
					return new Binary(opEqual,
					                  new Unary(opTypeOf, subExp2),
					                  new TypeVal(it));
				}
				return new Terminal((rsz == sz) ? opTrue : opFalse);
			}
			// We constrain the size but not the basic type
			return new Binary(opEqual, result->clone(), new TypeVal(new SizeType(sz)));
		}

	default:
		break;
	}
	return new Terminal(opTrue);
}

void
Location::getDefinitions(LocationSet &defs) const
{
	// This is a hack to fix aliasing (replace with something general)
	// FIXME! This is x86 specific too. Use -O for overlapped registers!
	if (isRegN(24)) {
		defs.insert(Location::regOf(0));
	}
}

class ConstraintSimplifier : public ExpModifier {
public:
	Exp *postVisit(Binary *) override;
};

Exp *
ConstraintSimplifier::postVisit(Binary *e)
{
	switch (e->getOper()) {
	case opEqual:
		{
			auto tv1 = dynamic_cast<TypeVal *>(e->getSubExp1());
			auto tv2 = dynamic_cast<TypeVal *>(e->getSubExp2());
			if (tv1 && tv2) {
				// FIXME: ADHOC TA assumed
				auto t1 = tv1->getType();
				auto t2 = tv2->getType();
				if (!t1->isPointerToAlpha() && !t2->isPointerToAlpha()) {
					delete e;
					return new Terminal(*t1 == *t2 ? opTrue : opFalse);
				}
			}
		}
		break;

	case opOr:
	case opAnd:
	case opNot:
		return e->simplify();
	default:
		break;
	}
	return e;
}

Exp *
Exp::simplifyConstraint()
{
	ConstraintSimplifier cs;
	return accept(cs);
}

//  //  //  //  //  //  //  //
//                          //
//     V i s i t i n g      //
//                          //
//  //  //  //  //  //  //  //
bool
Unary::accept(ExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return subExp1->accept(v);
}
bool
Binary::accept(ExpVisitor &v)
{
	assert(subExp1 && subExp2);

	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return subExp1->accept(v)
	    && subExp2->accept(v);
}
bool
Ternary::accept(ExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return subExp1->accept(v)
	    && subExp2->accept(v)
	    && subExp3->accept(v);
}

// All the Unary derived accept functions look the same, but they have to be repeated because the particular visitor
// function called each time is different for each class (because "this" is different each time)
bool
TypedExp::accept(ExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return subExp1->accept(v);
}
bool
FlagDef::accept(ExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return subExp1->accept(v);
}
bool
RefExp::accept(ExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return subExp1->accept(v);
}
bool
Location::accept(ExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return subExp1->accept(v);
}

// The following are similar, but don't have children that have to accept visitors
bool Terminal::accept(ExpVisitor &v) { return v.visit(this); }
bool    Const::accept(ExpVisitor &v) { return v.visit(this); }
bool  TypeVal::accept(ExpVisitor &v) { return v.visit(this); }

// FixProcVisitor class

void
Exp::fixLocationProc(UserProc *p)
{
	// All locations are supposed to have a pointer to the enclosing UserProc that they are a location of. Sometimes,
	// you have an arbitrary expression that may not have all its procs set. This function fixes the procs for all
	// Location subexpresssions.
	FixProcVisitor fpv;
	fpv.setProc(p);
	accept(fpv);
}

// GetProcVisitor class

UserProc *
Exp::findProc()
{
	GetProcVisitor gpv;
	accept(gpv);
	return gpv.getProc();
}

void
Exp::setConscripts(int n, bool bClear)
{
	SetConscripts sc(n, bClear);
	accept(sc);
}

/**
 * \brief Strip all size casts from an Exp.
 */
Exp *
Exp::stripSizes()
{
	SizeStripper ss;
	return accept(ss);
}

Exp *
Unary::accept(ExpModifier &v)
{
	// This Unary will be changed in *either* the pre or the post visit. If it's changed in the preVisit step, then
	// postVisit doesn't care about the type of ret. So let's call it a Unary, and the type system is happy
	bool recurse = true;
	auto ret = (Unary *)v.preVisit(this, recurse);
	if (recurse) {
		// TODO: Consider adding a way to repeatedly modify each child in turn until unchanged.
		subExp1 = subExp1->accept(v);
	}
	return v.postVisit(ret);
}
Exp *
Binary::accept(ExpModifier &v)
{
	assert(subExp1 && subExp2);

	bool recurse = true;
	auto ret = (Binary *)v.preVisit(this, recurse);
	if (recurse) {
		subExp1 = subExp1->accept(v);
		subExp2 = subExp2->accept(v);
	}
	return v.postVisit(ret);
}
Exp *
Ternary::accept(ExpModifier &v)
{
	bool recurse = true;
	auto ret = (Ternary *)v.preVisit(this, recurse);
	if (recurse) {
		subExp1 = subExp1->accept(v);
		subExp2 = subExp2->accept(v);
		subExp3 = subExp3->accept(v);
	}
	return v.postVisit(ret);
}
Exp *
Location::accept(ExpModifier &v)
{
	// This looks to be the same source code as Unary::accept, but the type of "this" is different, which is all
	// important here!  (it makes a call to a different visitor member function).
	bool recurse = true;
	auto ret = (Location *)v.preVisit(this, recurse);
	if (recurse) {
		subExp1 = subExp1->accept(v);
	}
	return v.postVisit(ret);
}
Exp *
RefExp::accept(ExpModifier &v)
{
	bool recurse = true;
	auto ret = (RefExp *)v.preVisit(this, recurse);
	if (recurse) {
		subExp1 = subExp1->accept(v);
	}
	return v.postVisit(ret);
}
Exp *
FlagDef::accept(ExpModifier &v)
{
	bool recurse = true;
	auto ret = (FlagDef *)v.preVisit(this, recurse);
	if (recurse) {
		subExp1 = subExp1->accept(v);
	}
	return v.postVisit(ret);
}
Exp *
TypedExp::accept(ExpModifier &v)
{
	bool recurse = true;
	auto ret = (TypedExp *)v.preVisit(this, recurse);
	if (recurse) {
		subExp1 = subExp1->accept(v);
	}
	return v.postVisit(ret);
}
Exp *
Terminal::accept(ExpModifier &v)
{
	// This is important if we need to modify terminals
	auto ret = (Terminal *)v.preVisit(this);
	return v.postVisit(ret);
}
Exp *
Const::accept(ExpModifier &v)
{
	auto ret = (Const *)v.preVisit(this);
	return v.postVisit(ret);
}
Exp *
TypeVal::accept(ExpModifier &v)
{
	auto ret = (TypeVal *)v.preVisit(this);
	return v.postVisit(ret);
}

static void
child(const Exp *e, int ind)
{
	ind += 4;
	if (!e) {
		std::cerr << std::setw(ind) << "" << "<NULL>\n";
		return;
	}
	void *vt = *(void **)e;
	if (!vt) {
		std::cerr << std::setw(ind) << "" << "<NULL VT>\n";
		return;
	}
	e->printx(ind);
}

/**
 * \fn void Exp::printx(int ind) const
 * \brief Print in indented hex (for debugging).
 *
 * In gdb:  "p x->printx(0)"
 */
void
Unary::printx(int ind) const
{
	std::cerr << std::setw(ind) << "" << operStrings[op] << "\n";
	child(subExp1, ind);
}
void
Binary::printx(int ind) const
{
	assert(subExp1 && subExp2);

	std::cerr << std::setw(ind) << "" << operStrings[op] << "\n";
	child(subExp1, ind);
	child(subExp2, ind);
}
void
Ternary::printx(int ind) const
{
	std::cerr << std::setw(ind) << "" << operStrings[op] << "\n";
	child(subExp1, ind);
	child(subExp2, ind);
	child(subExp3, ind);
}
void
Const::printx(int ind) const
{
	std::cerr << std::setw(ind) << "" << operStrings[op] << " ";
	switch (op) {
	case opIntConst:
		std::cerr << u.i;
		break;
	case opStrConst:
		std::cerr << "\"" << u.p << "\"";
		break;
	case opFltConst:
		std::cerr << u.d;
		break;
	default:
		std::cerr << std::hex << "?" << (int)op << "?" << std::dec;
	}
	if (conscript)
		std::cerr << " \\" << conscript << "\\";
	std::cerr << "\n";
}
void
TypeVal::printx(int ind) const
{
	std::cerr << std::setw(ind) << "" << operStrings[op] << " " << val->getCtype() << "\n";
}
void
TypedExp::printx(int ind) const
{
	std::cerr << std::setw(ind) << "" << operStrings[op] << " " << type->getCtype() << "\n";
	child(subExp1, ind);
}
void
Terminal::printx(int ind) const
{
	std::cerr << std::setw(ind) << "" << operStrings[op] << "\n";
}
void
RefExp::printx(int ind) const
{
	std::cerr << std::setw(ind) << "" << operStrings[op] << " {";
	if (!def)
		std::cerr << "NULL";
	else
		std::cerr << (void *)def << "=" << def->getNumber();
	std::cerr << "}\n";
	child(subExp1, ind);
}

const char *
Exp::getAnyStrConst() const
{
	const Exp *e = this;
	if (isAddrOf()) {
		e = ((const Location *)this)->getSubExp1();
		if (auto re = dynamic_cast<const RefExp *>(e))
			e = re->getSubExp1();
		if (e->isMemOf())
			e = ((const Location *)e)->getSubExp1();
	}
	if (!e->isStrConst()) return nullptr;
	return ((const Const *)e)->getStr();
}

/**
 * \brief Do the work of finding used locations.
 *
 * Find the locations used by this expression.  Uses the UsedLocsFinder
 * visitor class.
 *
 * \param memOnly  If true, only look inside m[...].
 */
void
Exp::addUsedLocs(LocationSet &used, bool memOnly)
{
	UsedLocsFinder ulf(used, memOnly);
	accept(ulf);
}

/**
 * Subscript any occurrences of e with e{def} in this expression.
 */
Exp *
Exp::expSubscriptVar(Exp *e, Statement *def)
{
	ExpSubscripter es(e, def);
	return accept(es);
}

/**
 * Subscript any occurrences of e with e{-} in this expression.
 *
 * \note Subscript with nullptr, not implicit assignments as above.
 */
Exp *
Exp::expSubscriptValNull(Exp *e)
{
	return expSubscriptVar(e, nullptr);
}

/**
 * Subscript all locations in this expression with their implicit assignments.
 */
Exp *
Exp::expSubscriptAllNull(/*Cfg *cfg*/)
{
	return expSubscriptVar(new Terminal(opWild), nullptr /* was nullptr, nullptr, cfg */);
}

/**
 * Before type analysis, implicit definitions are nullptr.  During and after
 * TA, they point to an implicit assignment statement.
 *
 * \note Don't implement this in exp.h since it would require \#including of
 *       statement.h from exp.h.
 */
bool
RefExp::isImplicitDef() const
{
	return !def || dynamic_cast<ImplicitAssign *>(def);
}

Exp *
Exp::bypass()
{
	CallBypasser cb(nullptr);
	return accept(cb);
}

void
Exp::bypassComp()
{
	if (!isMemOf()) return;
	((Location *)this)->setSubExp1(((Location *)this)->getSubExp1()->bypass());
}

/**
 * Get the complexity depth.  Basically, add one for each Unary, Binary, or
 * Ternary.
 */
int
Exp::getComplexityDepth(UserProc *proc)
{
	ComplexityFinder cf(proc);
	accept(cf);
	return cf.getDepth();
}

/**
 * Get memory depth.  Add one for each m[].
 */
int
Exp::getMemDepth()
{
	MemDepthFinder mdf;
	accept(mdf);
	return mdf.getDepth();
}

/**
 * Propagate all possible statements to this expression.
 */
Exp *
Exp::propagateAll()
{
	ExpPropagator ep;
	return accept(ep);
}

/**
 * Propagate all possible statements to this expression, and repeat until
 * there is no further change.
 */
Exp *
Exp::propagateAllRpt(bool &changed)
{
	ExpPropagator ep;
	changed = false;
	Exp *ret = this;
	while (true) {
		ep.clearChanged();  // Want to know if changed this *last* accept()
		ret = ret->accept(ep);
		if (ep.isChanged())
			changed = true;
		else
			break;
	}
	return ret;
}

bool
Exp::containsFlags()
{
	FlagsFinder ff;
	accept(ff);
	return ff.isFound();
}

/**
 * Check if this expression contains a bare memof (no subscripts) or one that
 * has no symbol (i.e. is not a local variable or a parameter).
 */
bool
Exp::containsBadMemof(UserProc *proc)
{
	BadMemofFinder bmf(proc);
	accept(bmf);
	return bmf.isFound();
}

// No longer used
bool
Exp::containsMemof(UserProc *proc)
{
	ExpHasMemofTester ehmt(proc);
	accept(ehmt);
	return ehmt.getResult();
}

#ifdef USING_MEMO
class ConstMemo : public Memo {
public:
	ConstMemo(int m) : Memo(m) { }

	union {
		int i;
		ADDRESS a;
		uint64_t ll;
		double d;
		char *p;
		Proc *pp;
	} u;
	int conscript;
};

Memo *
Const::makeMemo(int mId)
{
	auto m = new ConstMemo(mId);
	memcpy(&m->u, &u, sizeof u);
	m->conscript = conscript;
	return m;
}

void
Const::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<ConstMemo *>(mm);
	memcpy(&u, &m->u, sizeof u);
	conscript = m->conscript;
}

class TerminalMemo : public Memo {
public:
	TerminalMemo(int m) : Memo(m) { }
};

Memo *
Terminal::makeMemo(int mId)
{
	return new TerminalMemo(mId);
}

void
Terminal::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<TerminalMemo *>(mm);
}

class UnaryMemo : public Memo {
public:
	UnaryMemo(int m) : Memo(m) { }
};

Memo *
Unary::makeMemo(int mId)
{
	return new UnaryMemo(mId);
}

void
Unary::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<UnaryMemo *>(mm);
}

class BinaryMemo : public Memo {
public:
	BinaryMemo(int m) : Memo(m) { }
};

Memo *
Binary::makeMemo(int mId)
{
	return new BinaryMemo(mId);
}

void
Binary::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<BinaryMemo *>(mm);
}

class TernaryMemo : public Memo {
public:
	TernaryMemo(int m) : Memo(m) { }
};

Memo *
Ternary::makeMemo(int mId)
{
	return new TernaryMemo(mId);
}

void
Ternary::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<TernaryMemo *>(mm);
}

class TypedExpMemo : public Memo {
public:
	TypedExpMemo(int m) : Memo(m) { }
};

Memo *
TypedExp::makeMemo(int mId)
{
	return new TypedExpMemo(mId);
}

void
TypedExp::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<TypedExpMemo *>(mm);
}

class FlagDefMemo : public Memo {
public:
	FlagDefMemo(int m) : Memo(m) { }
};

Memo *
FlagDef::makeMemo(int mId)
{
	return new FlagDefMemo(mId);
}

void
FlagDef::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<FlagDefMemo *>(mm);
}

class RefExpMemo : public Memo {
public:
	RefExpMemo(int m) : Memo(m) { }
};

Memo *
RefExp::makeMemo(int mId)
{
	return new RefExpMemo(mId);
}

void
RefExp::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<RefExpMemo *>(mm);
}

class TypeValMemo : public Memo {
public:
	TypeValMemo(int m) : Memo(m) { }
};

Memo *
TypeVal::makeMemo(int mId)
{
	return new TypeValMemo(mId);
}

void
TypeVal::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<TypeValMemo *>(mm);
}

class LocationMemo : public Memo {
public:
	LocationMemo(int m) : Memo(m) { }
};

Memo *
Location::makeMemo(int mId)
{
	return new LocationMemo(mId);
}

void
Location::readMemo(Memo *mm, bool dec)
{
	// FIXME: not completed
	// auto m = dynamic_cast<LocationMemo *>(mm);
}
#endif
