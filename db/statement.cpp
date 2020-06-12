/**
 * \file
 * \brief Implementation of the Statement and related classes.  (Was
 *        dataflow.cpp a long time ago)
 *
 * \authors
 * Copyright (C) 2002-2006, Trent Waddington and Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "statement.h"

#include "boomerang.h"
#include "cfg.h"
#include "dataflow.h"
#include "exp.h"
#include "hllcode.h"
#include "proc.h"
#include "prog.h"
#include "rtl.h"        // For debugging code
#include "signature.h"
#include "visitor.h"

#include <iomanip>      // For std::setw
#include <sstream>

#include <cassert>
#include <cstring>

GotoStatement::GotoStatement(Exp *dest) :
	pDest(dest)
{
}
BranchStatement::BranchStatement(Exp *dest) :
	GotoStatement(dest)
{
}
CaseStatement::CaseStatement(Exp *dest) :
	GotoStatement(dest)
{
}
CallStatement::CallStatement(Exp *dest) :
	GotoStatement(dest)
{
}

/**
 * \brief Construct a jump to a fixed address.
 *
 * \param dest  Native address of destination.
 */
GotoStatement::GotoStatement(ADDRESS dest) :
	pDest(new Const(dest))
{
}
BranchStatement::BranchStatement(ADDRESS dest) :
	GotoStatement(dest)
{
}
CallStatement::CallStatement(ADDRESS dest) :
	GotoStatement(dest)
{
}

TypingStatement::TypingStatement(Type *ty) :
	type(ty)
{
}

/**
 * \brief Constructor, subexpression.
 */
Assignment::Assignment(Exp *lhs) :
	TypingStatement(new VoidType),
	lhs(lhs)
{
	if (lhs && lhs->isRegOfK()) {
		int n = ((Const *)lhs->getSubExp1())->getInt();
		if (((Location *)lhs)->getProc()) {
			type = new SizeType(((Location *)lhs)->getProc()->getProg()->getFrontEnd()->getRegSize(n));
		}
	}
}

BoolAssign::BoolAssign() :
	Assignment(nullptr)
{
}

/**
 * \brief Constructor, subexpressions.
 */
Assign::Assign(Exp *lhs, Exp *rhs, Exp *guard) :
	Assignment(lhs),
	rhs(rhs),
	guard(guard)
{
}

/**
 * \brief Constructor, subexpression.
 */
ImplicitAssign::ImplicitAssign(Exp *lhs) :
	Assignment(lhs)
{
}

/**
 * \brief Constructor, type, and subexpression.
 */
Assignment::Assignment(Type *ty, Exp *lhs) :
	TypingStatement(ty),
	lhs(lhs)
{
}

/**
 * Constructor, type and subexpressions.
 */
Assign::Assign(Type *ty, Exp *lhs, Exp *rhs, Exp *guard) :
	Assignment(ty, lhs),
	rhs(rhs),
	guard(guard)
{
}

/**
 * \brief Constructor, type, and subexpression.
 */
ImplicitAssign::ImplicitAssign(Type *ty, Exp *lhs) :
	Assignment(ty, lhs)
{
}

GotoStatement::~GotoStatement()
{
	;//delete pDest;
}

BranchStatement::~BranchStatement()
{
	;//delete pCond;
}

/**
 * \note Don't delete the pSwitchVar; it's always a copy of something else (so
 * don't delete twice).
 */
CaseStatement::~CaseStatement()
{
	;//delete pSwitchInfo;
}

BoolAssign::~BoolAssign()
{
	;//delete pCond;
}

/**
 * \brief Copy constructor.
 */
Assign::Assign(const Assign &o) :
	Assignment(o.lhs->clone())
{
	rhs   = o.rhs->clone();
	type  = o.type  ? o.type->clone()  : nullptr;
	guard = o.guard ? o.guard->clone() : nullptr;
}

/**
 * \brief Copy constructor.
 */
ImplicitAssign::ImplicitAssign(const ImplicitAssign &o) :
	Assignment(o.type ? o.type->clone() : nullptr, o.lhs->clone())
{
}

/**
 * \fn Statement *Statement::clone() const
 * \brief Deep copy clone.
 *
 * Make a deep copy of self.
 *
 * \returns  A new Statement that is a clone of this one.
 */
Statement *
GotoStatement::clone() const
{
	auto ret = new GotoStatement();
	ret->pDest = pDest->clone();
	ret->m_isComputed = m_isComputed;
	// Statement members
	ret->pbb = pbb;
	ret->proc = proc;
	ret->number = number;
	return ret;
}
Statement *
BranchStatement::clone() const
{
	auto ret = new BranchStatement();
	ret->pDest = pDest->clone();
	ret->m_isComputed = m_isComputed;
	ret->jtCond = jtCond;
	ret->pCond = pCond ? pCond->clone() : nullptr;
	ret->bFloat = bFloat;
	// Statement members
	ret->pbb = pbb;
	ret->proc = proc;
	ret->number = number;
	return ret;
}
Statement *
CaseStatement::clone() const
{
	auto ret = new CaseStatement();
	ret->pDest = pDest->clone();
	ret->m_isComputed = m_isComputed;
	ret->pSwitchInfo = new SWITCH_INFO;
	*ret->pSwitchInfo = *pSwitchInfo;
	ret->pSwitchInfo->pSwitchVar = pSwitchInfo->pSwitchVar->clone();
	// Statement members
	ret->pbb = pbb;
	ret->proc = proc;
	ret->number = number;
	return ret;
}
Statement *
CallStatement::clone() const
{
	auto ret = new CallStatement();
	ret->pDest = pDest->clone();
	ret->m_isComputed = m_isComputed;
	for (const auto &arg : arguments)
		ret->arguments.append(arg->clone());
	for (const auto &def : defines)
		ret->defines.append(def->clone());
	// Statement members
	ret->pbb = pbb;
	ret->proc = proc;
	ret->number = number;
	return ret;
}
Statement *
ReturnStatement::clone() const
{
	auto rs = new ReturnStatement();
	for (const auto &mod : modifieds)
		rs->modifieds.append((ImplicitAssign *)mod->clone());
	for (const auto &ret : returns)
		rs->returns.append((Assignment *)ret->clone());
	rs->col.makeCloneOf(col);
	// Statement members
	rs->pbb = pbb;
	rs->proc = proc;
	rs->number = number;
	return rs;
}
Statement *
BoolAssign::clone() const
{
	auto ret = new BoolAssign();
	ret->jtCond = jtCond;
	ret->pCond = pCond ? pCond->clone() : nullptr;
	ret->bFloat = bFloat;
	// Statement members
	ret->pbb = pbb;
	ret->proc = proc;
	ret->number = number;
	return ret;
}
Statement *
Assign::clone() const
{
	auto a = new Assign(*this);
	// Statement members
	a->pbb = pbb;
	a->proc = proc;
	a->number = number;
	return a;
}
Statement *
PhiAssign::clone() const
{
	auto pa = new PhiAssign(type, lhs);
	for (const auto &def : defVec) {
		PhiInfo pi;
		pi.def = def.def;       // Don't clone the Statement pointer (never moves)
		pi.e = def.e->clone();  // Do clone the expression pointer
		pa->defVec.push_back(pi);
	}
	return pa;
}
Statement *
ImplicitAssign::clone() const
{
	return new ImplicitAssign(type, lhs);
}
#if 0 // Cruft?
Statement *
ImpRefStatement::clone() const
{
	return new ImpRefStatement(type->clone(), addressExp->clone());
}
#endif

/**
 * \fn bool Statement::accept(StmtVisitor &v)
 * \brief Accept a StmtVisitor visitor to this Statement.
 *
 * \returns true to continue visiting.
 */
bool
GotoStatement::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
BranchStatement::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
CaseStatement::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
CallStatement::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
ReturnStatement::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
BoolAssign::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
Assign::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
PhiAssign::accept(StmtVisitor &v)
{
	return v.visit(this);
}
bool
ImplicitAssign::accept(StmtVisitor &v)
{
	return v.visit(this);
}
#if 0 // Cruft?
bool
ImpRefStatement::accept(StmtVisitor &v)
{
	return v.visit(this);
}
#endif

/**
 * \fn bool Statement::accept(StmtExpVisitor &v)
 * \brief Accept a StmtExpVisitor visitor to this Statement.
 *
 * Visit all the various expressions in a statement.
 *
 * \returns true to continue visiting.
 */
// Visiting from class StmtExpVisitor
bool
GotoStatement::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return (!pDest || pDest->accept(v.ev));
}
bool
BranchStatement::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	// Destination will always be a const for X86, so the below will never be used in practice
	return (!pDest || pDest->accept(v.ev))
	    && (!pCond || pCond->accept(v.ev));
}
bool
CaseStatement::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return (!pDest || pDest->accept(v.ev))
	    && (!pSwitchInfo || !pSwitchInfo->pSwitchVar || pSwitchInfo->pSwitchVar->accept(v.ev));
}
bool
CallStatement::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	if (pDest && !pDest->accept(v.ev))
		return false;
	for (const auto &arg : arguments)
		if (!arg->accept(v))
			return false;
	// FIXME: why aren't defines counted?
#if 0  // Do we want to accept visits to the defines? Not sure now...
	for (const auto &def : defines)
		if (def.e && !def.e->accept(v.ev))  // Can be nullptr now to line up with other returns
			return false;
#endif
	// FIXME: surely collectors should be counted?
	return true;
}
bool
ReturnStatement::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	if (!v.isIgnoreCol()) {
		for (const auto &def : col)
			if (!def->accept(v))
				return false;
		// EXPERIMENTAL: for now, count the modifieds as if they are a collector (so most, if not all of the time,
		// ignore them). This is so that we can detect better when a definition is used only once, and therefore
		// propagate anything to it
		for (const auto &mod : modifieds)
			if (!mod->accept(v))
				return false;
	}
	for (const auto &ret : returns)
		if (!ret->accept(v))
			return false;
	return true;
}
bool
BoolAssign::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return (!pCond || pCond->accept(v.ev));
}
bool
Assign::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		// The visitor has overridden this functionality.  This is needed for example in UsedLocFinder, where the lhs of
		// an assignment is not used (but if it's m[blah], then blah is used)
		return ret;
	return (!lhs || lhs->accept(v.ev))
	    && (!rhs || rhs->accept(v.ev));
}
bool
PhiAssign::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!recurse) return ret;
	if (ret && lhs)
		ret = lhs->accept(v.ev);
	for (const auto &def : defVec) {
		if (!def.e) continue;
		auto re = new RefExp(def.e, def.def);
		if (!re->accept(v.ev))
			return false;
	}
	return true;
}
bool
ImplicitAssign::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return (!lhs || lhs->accept(v.ev));
}
#if 0 // Cruft?
bool
ImpRefStatement::accept(StmtExpVisitor &v)
{
	bool recurse = true;
	bool ret = v.visit(this, recurse);
	if (!ret || !recurse)
		return ret;
	return addressExp->accept(v.ev);
}
#endif

/**
 * \fn bool Statement::accept(StmtModifier &v)
 * \brief Accept a StmtModifier visitor to this Statement.
 *
 * Modify all the various expressions in a statement.
 *
 * \returns true to continue visiting.
 */
bool
GotoStatement::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
	}
	return true;
}
bool
BranchStatement::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
		if (pCond)
			pCond = pCond->accept(v.mod);
	}
	return true;
}
bool
CaseStatement::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
		if (pSwitchInfo && pSwitchInfo->pSwitchVar)
			pSwitchInfo->pSwitchVar = pSwitchInfo->pSwitchVar->accept(v.mod);
	}
	return true;
}
bool
CallStatement::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
		for (const auto &arg : arguments)
			arg->accept(v);
		// For example: needed for CallBypasser so that a collected definition that happens to be another call gets
		// adjusted
		// I'm thinking no at present... let the bypass and propagate while possible logic take care of it, and leave the
		// collectors as the rename logic set it
		// Well, sort it out with ignoreCollector()
		if (!v.ignoreCollector())
			for (const auto &def : defCol)
				def->accept(v);
		for (const auto &def : defines)
			def->accept(v);
	}
	return true;
}
bool
ReturnStatement::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (!v.ignoreCollector())
			for (const auto &def : col)
				if (!def->accept(v))
					return false;
		for (const auto &mod : modifieds)
			if (!mod->accept(v))
				return false;
		for (const auto &ret : returns)
			if (!ret->accept(v))
				return false;
	}
	return true;
}
bool
BoolAssign::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pCond)
			pCond = pCond->accept(v.mod);
		if (lhs->isMemOf())
			((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(v.mod));
	}
	return true;
}
bool
Assign::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		lhs = lhs->accept(v.mod);
		rhs = rhs->accept(v.mod);
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "Assignment changed: now " << *this << "\n";
	return true;
}
bool
PhiAssign::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		lhs = lhs->accept(v.mod);
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "PhiAssign changed: now " << *this << "\n";
	return true;
}
bool
ImplicitAssign::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		lhs = lhs->accept(v.mod);
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "ImplicitAssign changed: now " << *this << "\n";
	return true;
}
#if 0 // Cruft?
bool
ImpRefStatement::accept(StmtModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		addressExp = addressExp->accept(v.mod);
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "ImplicitRef changed: now " << *this << "\n";
	return true;
}
#endif

/**
 * \fn bool Statement::accept(StmtPartModifier &v)
 * \brief Accept a StmtPartModifier visitor to this Statement.
 *
 * Modify all the various expressions in a statement, except for the top level
 * of the LHS of assignments.
 *
 * \returns true to continue visiting.
 */
bool
GotoStatement::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
	}
	return true;
}
bool
BranchStatement::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
		if (pCond)
			pCond = pCond->accept(v.mod);
	}
	return true;
}
bool
CaseStatement::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
		if (pSwitchInfo && pSwitchInfo->pSwitchVar)
			pSwitchInfo->pSwitchVar = pSwitchInfo->pSwitchVar->accept(v.mod);
	}
	return true;
}
bool
CallStatement::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pDest)
			pDest = pDest->accept(v.mod);
		for (const auto &arg : arguments)
			arg->accept(v);
	}
	// For example: needed for CallBypasser so that a collected definition that happens to be another call gets
	// adjusted
	// But now I'm thinking no, the bypass and propagate while possible logic should take care of it.
	// Then again, what about the use collectors in calls? Best to do it.
	if (!v.ignoreCollector()) {
		for (const auto &def : defCol)
			def->accept(v);
		for (const auto &use : useCol)
			// I believe that these should never change at the top level, e.g. m[esp{30} + 4] -> m[esp{-} - 20]
			use->accept(v.mod);
	}
	if (recurse) {
		for (const auto &def : defines)
			def->accept(v);
	}
	return true;
}
bool
ReturnStatement::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	for (const auto &mod : modifieds)
		if (!mod->accept(v))
			return false;
	for (const auto &ret : returns)
		if (!ret->accept(v))
			return false;
	return true;
}
bool
BoolAssign::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	if (recurse) {
		if (pCond)
			pCond = pCond->accept(v.mod);
		if (lhs)
			lhs = lhs->accept(v.mod);
	}
	return true;
}
bool
Assign::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		if (lhs->isMemOf())
			((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(v.mod));
		rhs = rhs->accept(v.mod);
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "Assignment changed: now " << *this << "\n";
	return true;
}
bool
PhiAssign::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		if (lhs->isMemOf())
			((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(v.mod));
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "PhiAssign changed: now " << *this << "\n";
	return true;
}
bool
ImplicitAssign::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		if (lhs->isMemOf())
			((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(v.mod));
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "ImplicitAssign changed: now " << *this << "\n";
	return true;
}
#if 0 // Cruft?
bool
ImpRefStatement::accept(StmtPartModifier &v)
{
	bool recurse = true;
	v.visit(this, recurse);
	v.mod.clearMod();
	if (recurse) {
		addressExp = addressExp->accept(v.mod);
	}
	if (VERBOSE && v.mod.isMod())
		LOG << "ImplicitRef changed: now " << *this << "\n";
	return true;
}
#endif

/**
 * \fn bool Statement::usesExp(Exp *e) const
 * \brief Returns true if this statement uses the given expression.
 *
 * \note PhiAssign and ImplicitAssign don't override Assignment.
 */
bool
GotoStatement::usesExp(Exp *e) const
{
	Exp *where;
	return (pDest->search(e, where));
}
bool
BranchStatement::usesExp(Exp *e) const
{
	Exp *tmp;
	return pCond && pCond->search(e, tmp);
}
bool
CaseStatement::usesExp(Exp *e) const
{
	// Before a switch statement is recognised, pDest is non null
	if (pDest)
		return *pDest == *e;
	// After a switch statement is recognised, pDest is null, and pSwitchInfo->pSwitchVar takes over
	if (pSwitchInfo->pSwitchVar)
		return *pSwitchInfo->pSwitchVar == *e;
	return false;
}
bool
CallStatement::usesExp(Exp *e) const
{
	if (GotoStatement::usesExp(e)) return true;
	for (const auto &arg : arguments)
		if (arg->usesExp(e)) return true;
	for (const auto &def : defines)
		if (def->usesExp(e)) return true;
	return false;
}
bool
ReturnStatement::usesExp(Exp *e) const
{
	Exp *where;
	for (const auto &ret : returns) {
		if (ret->search(e, where))
			return true;
	}
	return false;
}
bool
Assignment::usesExp(Exp *e) const
{
	Exp *where = nullptr;
	return (lhs->isMemOf() || lhs->isRegOf()) && ((Location *)lhs)->getSubExp1()->search(e, where);
}
bool
BoolAssign::usesExp(Exp *e) const
{
	assert(lhs && pCond);
	Exp *where = nullptr;
	return (pCond->search(e, where) || (lhs->isMemOf() && ((Location *)lhs)->getSubExp1()->search(e, where)));
}
bool
Assign::usesExp(Exp *e) const
{
	Exp *where = nullptr;
	return (rhs->search(e, where) || ((lhs->isMemOf() || lhs->isRegOf()) && ((Location *)lhs)->getSubExp1()->search(e, where)));
}

/**
 * \fn bool Statement::search(Exp *search, Exp *&result)
 * \brief General search.
 */
bool
GotoStatement::search(Exp *search, Exp *&result)
{
	if (pDest)
		return pDest->search(search, result);
	result = nullptr;
	return false;
}
bool
BranchStatement::search(Exp *search, Exp *&result)
{
	if (pCond)
		return pCond->search(search, result);
	result = nullptr;
	return false;
}
bool
CallStatement::search(Exp *search, Exp *&result)
{
	if (GotoStatement::search(search, result))
		return true;
	for (const auto &def : defines) {
		if (def->search(search, result))
			return true;
	}
	for (const auto &arg : arguments) {
		if (arg->search(search, result))
			return true;
	}
	return false;
}
bool
ReturnStatement::search(Exp *search, Exp *&result)
{
	for (const auto &ret : returns) {
		if (ret->search(search, result))
			return true;
	}
	result = nullptr;
	return false;
}
bool
BoolAssign::search(Exp *search, Exp *&result)
{
	assert(lhs);
	assert(pCond);
	return lhs->search(search, result)
	    || pCond->search(search, result);
}
bool
Assign::search(Exp *search, Exp *&result)
{
	return lhs->search(search, result)
	    || rhs->search(search, result);
}
bool
PhiAssign::search(Exp *search, Exp *&result)
{
	if (lhs->search(search, result))
		return true;
	for (const auto &def : defVec) {
		if (!def.e) continue;  // Note: can't match foo{-} because of this
		auto re = new RefExp(def.e, def.def);
		if (re->search(search, result))
			return true;
	}
	return false;
}
bool
ImplicitAssign::search(Exp *search, Exp *&result)
{
	return lhs->search(search, result);
}
#if 0 // Cruft?
bool
ImpRefStatement::search(Exp *search, Exp *&result)
{
	return addressExp->search(search, result);
}
#endif

/**
 * \fn bool Statement::searchAll(Exp *search, std::list<Exp *> &result)
 * \brief Find all instances of the search expression.
 *
 * Searches for all instances of a given subexpression within this statement
 * and adds them to a given list in reverse nesting order.
 *
 * \note search can't easily be made const.
 *
 * \param search  A location to search for.
 * \param result  A list which will have any matching exprs appended to it.
 *
 * \returns  true if there were any matches.
 */
bool
GotoStatement::searchAll(Exp *search, std::list<Exp *> &result)
{
	if (pDest)
		return pDest->searchAll(search, result);
	return false;
}
bool
BranchStatement::searchAll(Exp *search, std::list<Exp *> &result)
{
	if (pCond)
		return pCond->searchAll(search, result);
	return false;
}
bool
CaseStatement::searchAll(Exp *search, std::list<Exp *> &result)
{
	return GotoStatement::searchAll(search, result)
	    || (pSwitchInfo && pSwitchInfo->pSwitchVar && pSwitchInfo->pSwitchVar->searchAll(search, result));
}
bool
CallStatement::searchAll(Exp *search, std::list<Exp *> &result)
{
	bool found = GotoStatement::searchAll(search, result);
	for (const auto &def : defines)
		found |= def->searchAll(search, result);
	for (const auto &arg : arguments)
		found |= arg->searchAll(search, result);
	return found;
}
bool
ReturnStatement::searchAll(Exp *search, std::list<Exp *> &result)
{
	bool found = false;
	for (const auto &ret : returns)
		found |= ret->searchAll(search, result);
	return found;
}
bool
BoolAssign::searchAll(Exp *search, std::list<Exp *> &result)
{
	assert(lhs);
	assert(pCond);
	bool found = lhs->searchAll(search, result);
	found |= pCond->searchAll(search, result);
	return found;
}
bool
Assign::searchAll(Exp *search, std::list<Exp *> &result)
{
	bool found = lhs->searchAll(search, result);
	found |= rhs->searchAll(search, result);
	return found;
}
bool
PhiAssign::searchAll(Exp *search, std::list<Exp *> &result)
{
	// FIXME: is this the right semantics for searching a phi statement, disregarding the RHS?
	return lhs->searchAll(search, result);
}
bool
ImplicitAssign::searchAll(Exp *search, std::list<Exp *> &result)
{
	return lhs->searchAll(search, result);
}
#if 0 // Cruft?
bool
ImpRefStatement::searchAll(Exp *search, std::list<Exp *> &result)
{
	return addressExp->searchAll(search, result);
}
#endif

/**
 * \fn bool Statement::searchAndReplace(Exp *search, Exp *replace, bool cc)
 * \brief General search and replace.
 *
 * Replace all instances of "search" with "replace".
 *
 * Set cc true to change collectors as well (in CallStatements and
 * ReturnStatements, otherwise ignored).
 *
 * \param search   A location to search for.
 * \param replace  The expression with which to replace it.
 * \param cc       true to replace in collectors.
 *
 * \returns  true if any change.
 */
bool
GotoStatement::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool change = false;
	if (pDest)
		pDest = pDest->searchReplaceAll(search, replace, change);
	return change;
}
bool
BranchStatement::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	GotoStatement::searchAndReplace(search, replace, cc);
	bool change = false;
	if (pCond)
		pCond = pCond->searchReplaceAll(search, replace, change);
	return change;
}
bool
CaseStatement::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool ch = GotoStatement::searchAndReplace(search, replace, cc);
	bool ch2 = false;
	if (pSwitchInfo && pSwitchInfo->pSwitchVar)
		pSwitchInfo->pSwitchVar = pSwitchInfo->pSwitchVar->searchReplaceAll(search, replace, ch2);
	return ch | ch2;
}
bool
CallStatement::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool change = GotoStatement::searchAndReplace(search, replace, cc);
	// FIXME: MVE: Check if we ever want to change the LHS of arguments or defines...
	for (const auto &def : defines)
		change |= def->searchAndReplace(search, replace, cc);
	for (const auto &arg : arguments)
		change |= arg->searchAndReplace(search, replace, cc);
	if (cc) {
		for (const auto &def : defCol)
			change |= def->searchAndReplace(search, replace, cc);
	}
	return change;
}
bool
ReturnStatement::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool change = false;
	for (const auto &ret : returns)
		change |= ret->searchAndReplace(search, replace, cc);
	if (cc) {
		for (const auto &def : col)
			change |= def->searchAndReplace(search, replace);
	}
	return change;
}
bool
BoolAssign::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool chl, chr;
	assert(pCond);
	assert(lhs);
	pCond = pCond->searchReplaceAll(search, replace, chl);
	lhs   =   lhs->searchReplaceAll(search, replace, chr);
	return chl | chr;
}
bool
Assign::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool chl, chr, chg = false;
	lhs = lhs->searchReplaceAll(search, replace, chl);
	rhs = rhs->searchReplaceAll(search, replace, chr);
	if (guard)
		guard = guard->searchReplaceAll(search, replace, chg);
	return chl | chr | chg;
}
bool
PhiAssign::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool change;
	lhs = lhs->searchReplaceAll(search, replace, change);
	for (auto &def : defVec) {
		if (!def.e) continue;
		bool ch;
		// Assume that the definitions will also be replaced
		def.e = def.e->searchReplaceAll(search, replace, ch);
		change |= ch;
	}
	return change;
}
bool
ImplicitAssign::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool change;
	lhs = lhs->searchReplaceAll(search, replace, change);
	return change;
}
#if 0 // Cruft?
bool
ImpRefStatement::searchAndReplace(Exp *search, Exp *replace, bool cc)
{
	bool change;
	addressExp = addressExp->searchReplaceAll(search, replace, change);
	return change;
}
#endif

/**
 * \fn void Statement::print(std::ostream &os, bool html) const
 * \brief Display a text reprentation of this Statement to the given stream.
 *
 * \note Usually called from RTL::print(), in which case the first 9 chars of
 * the line (or first table cell, for HTML) have already been output to os.
 *
 * \param os  Stream to write to.
 */
void
GotoStatement::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "<a name=\"stmt" << number << "\">" << number << "</a>"
		   << "</td><td>";
	} else {
		os << std::setw(4) << number << " ";
	}

	os << "GOTO ";
	if (!pDest)
		os << "*no dest*";
	else if (!pDest->isIntConst())
		pDest->print(os, html);
	else
		os << "0x" << std::hex << getFixedDest() << std::dec;
	if (html)
		os << "</td>";
}
void
BranchStatement::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "<a name=\"stmt" << number << "\">" << number << "</a>"
		   << "</td><td>";
	} else {
		os << std::setw(4) << number << " ";
	}

	os << "BRANCH ";
	if (!pDest)
		os << "*no dest*";
	else if (!pDest->isIntConst())
		pDest->print(os, html);
	else
		// Really we'd like to display the destination label here...
		os << "0x" << std::hex << getFixedDest() << std::dec;
	os << ", condition ";
	switch (jtCond) {
	case BRANCH_JE:   os << "equal"; break;
	case BRANCH_JNE:  os << "not equal"; break;
	case BRANCH_JSL:  os << "signed less"; break;
	case BRANCH_JSLE: os << "signed less or equal"; break;
	case BRANCH_JSGE: os << "signed greater or equal"; break;
	case BRANCH_JSG:  os << "signed greater"; break;
	case BRANCH_JUL:  os << "unsigned less"; break;
	case BRANCH_JULE: os << "unsigned less or equal"; break;
	case BRANCH_JUGE: os << "unsigned greater or equal"; break;
	case BRANCH_JUG:  os << "unsigned greater"; break;
	case BRANCH_JMI:  os << "minus"; break;
	case BRANCH_JPOS: os << "plus"; break;
	case BRANCH_JOF:  os << "overflow"; break;
	case BRANCH_JNOF: os << "no overflow"; break;
	case BRANCH_JPAR: os << "even parity"; break;
	}
	if (bFloat) os << " float";
	if (pCond) {
		os << (html ? "<br>" : "\n              ")
		   << "High level: ";
		pCond->print(os, html);
	}
	if (html)
		os << "</td>";
}
void
CaseStatement::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "<a name=\"stmt" << number << "\">" << number << "</a>"
		   << "</td><td>";
	} else {
		os << std::setw(4) << number << " ";
	}

	if (!pSwitchInfo) {
		os << "CASE [";
		if (!pDest)
			os << "*no dest*";
		else
			pDest->print(os, html);
		os << "]";
	} else {
		os << "SWITCH(";
		pSwitchInfo->pSwitchVar->print(os, html);
		os << ")\n";
	}
	if (html)
		os << "</td>";
}
void
CallStatement::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "<a name=\"stmt" << number << "\">" << number << "</a>"
		   << "</td><td>";
	} else {
		os << std::setw(4) << number << " ";
	}

	// Define(s), if any
	if (!defines.empty()) {
		if (defines.size() > 1) os << "{";
		bool first = true;
		for (const auto &def : defines) {
			auto as = dynamic_cast<Assignment *>(def);
			assert(as);
			if (first)
				first = false;
			else
				os << ", ";
			os << "*" << as->getType() << "* ";
			as->getLeft()->print(os, html);
			if (auto asgn = dynamic_cast<Assign *>(as)) {
				os << " := ";
				asgn->getRight()->print(os, html);
			}
		}
		if (defines.size() > 1) os << "}";
		os << " := ";
	} else if (isChildless()) {
		os << (html ? "&lt;all&gt; := " : "<all> := ");
	}

	os << "CALL ";
	if (procDest)
		os << procDest->getName();
	else if (!pDest)
		os << "*no dest*";
	else {
		if (pDest->isIntConst())
			os << "0x" << std::hex << ((Const *)pDest)->getInt() << std::dec;
		else
			pDest->print(os, html);  // Could still be an expression
	}

	// Print the actual arguments of the call
	if (isChildless()) {
		os << (html ? "(&lt;all&gt;)" : "(<all>)");
	} else {
		os << "(\n";
		for (const auto &arg : arguments) {
			os << "                ";
			((Assignment *)arg)->printCompact(os, html);
			os << "\n";
		}
		os << "              )";
	}

#if 1
	// Collected reaching definitions
	os << (html ? "<br>" : "\n              ")
	   << "Reaching definitions: ";
	defCol.print(os, html);
	os << (html ? "<br>" : "\n              ")
	   << "Live variables: ";
	useCol.print(os, html);
#endif

	if (html)
		os << "</td>";
}
#define RETSTMT_COLS 120
void
ReturnStatement::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "<a name=\"stmt" << number << "\">" << number << "</a>"
		   << "</td><td>";
	} else {
		os << std::setw(4) << number << " ";
	}

	os << "RET";
	bool first = true;
	unsigned column = 19;
	for (const auto &ret : returns) {
		std::ostringstream ost;
		((Assignment *)ret)->printCompact(ost, html);
		unsigned len = ost.str().length();
		if (first) {
			first = false;
			os << " ";
		} else if (column + 4 + len > RETSTMT_COLS) {  // 4 for command 3 spaces
			if (column != RETSTMT_COLS - 1) os << ",";  // Comma at end of line
			os << "\n                ";
			column = 16;
		} else {
			os << ",   ";
			column += 4;
		}
		os << ost.str();
		column += len;
	}
	os << (html ? "<br>" : "\n              ")
	   << "Modifieds: ";
	first = true;
	column = 25;
	for (const auto &mod : modifieds) {
		std::ostringstream ost;
		Assign *as = (Assign *)mod;
		if (auto ty = as->getType())
			ost << "*" << *ty << "* ";
		ost << *as->getLeft();
		unsigned len = ost.str().length();
		if (first)
			first = false;
		else if (column + 3 + len > RETSTMT_COLS) {  // 3 for comma and 2 spaces
			if (column != RETSTMT_COLS - 1) os << ",";  // Comma at end of line
			os << "\n                ";
			column = 16;
		} else {
			os << ",  ";
			column += 3;
		}
		os << ost.str();
		column += len;
	}
#if 1
	// Collected reaching definitions
	os << (html ? "<br>" : "\n              ")
	   << "Reaching definitions: ";
	col.print(os, html);
#endif
	if (html)
		os << "</td>";
}
void
Assignment::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "<a name=\"stmt" << number << "\">" << number << "</a>"
		   << "</td><td>";
	} else {
		os << std::setw(4) << number << " ";
	}

	printCompact(os, html);
	//if (!ranges.empty())
	//	os << "\n\t\t\tranges: " << ranges;
	if (html)
		os << "</td>";
}
#if 0 // Cruft?
void
ImpRefStatement::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "</td><td>";
	} else {
		os << "     ";  // No statement number
	}

	os << "*" << type << "* IMP REF ";
	addressExp->print(os, html);
	if (html)
		os << "</td>";
}
void
JunctionStatement::print(std::ostream &os, bool html) const
{
	if (html) {
		os << "<td width=\"50\" align=\"center\">"
		   << "<a name=\"stmt" << number << "\">" << number << "</a>"
		   << "</td><td>";
	} else {
		os << std::setw(4) << number << " ";
	}

	os << "JUNCTION";
	for (const auto &pred : pbb->getInEdges()) {
		os << " " << std::hex << pred->getHiAddr() << std::dec;
		if (pbb->isBackEdge(pred))
			os << "*";
	}
	if (isLoopJunction())
		os << " LOOP";
	os << "\n\t\t\tranges: " << ranges;
	if (html)
		os << "</td>";
}
#endif

/**
 * \fn void Assignment::printCompact(std::ostream &os, bool html) const
 * \brief Without statement number.
 */
void
BoolAssign::printCompact(std::ostream &os, bool html) const
{
	os << "*" << type << "* ";
	lhs->print(os, html);
	os << " := CC(";
	switch (jtCond) {
	case BRANCH_JE:   os << "equal"; break;
	case BRANCH_JNE:  os << "not equal"; break;
	case BRANCH_JSL:  os << "signed less"; break;
	case BRANCH_JSLE: os << "signed less or equal"; break;
	case BRANCH_JSGE: os << "signed greater or equal"; break;
	case BRANCH_JSG:  os << "signed greater"; break;
	case BRANCH_JUL:  os << "unsigned less"; break;
	case BRANCH_JULE: os << "unsigned less or equal"; break;
	case BRANCH_JUGE: os << "unsigned greater or equal"; break;
	case BRANCH_JUG:  os << "unsigned greater"; break;
	case BRANCH_JMI:  os << "minus"; break;
	case BRANCH_JPOS: os << "plus"; break;
	case BRANCH_JOF:  os << "overflow"; break;
	case BRANCH_JNOF: os << "no overflow"; break;
	case BRANCH_JPAR: os << "even parity"; break;
	}
	if (bFloat) os << " float";
	os << ")";
	if (html)
		os << "<br>";
	os << "\n";
	if (pCond) {
		os << "High level: ";
		pCond->print(os, html);
		if (html)
			os << "<br>";
		os << "\n";
	}
}
void
Assign::printCompact(std::ostream &os, bool html) const
{
	os << "*" << type << "* ";
	if (guard) {
		guard->print(os, html);
		os << " => ";
	}
	if (lhs) lhs->print(os, html);
	os << " := ";
	if (rhs) rhs->print(os, html);
}
void
PhiAssign::printCompact(std::ostream &os, bool html) const
{
	os << "*" << type << "* ";
	if (lhs) lhs->print(os, html);
	os << " := phi";
	// Print as lhs := phi{9 17} for the common case where the lhs is the same location as all the referenced
	// locations. When not, print as local4 := phi(r24{9} argc{17})
	bool simple = true;
	for (const auto &def : defVec) {
		// If e is nullptr assume it is meant to match lhs
		if (!def.e) continue;
		if (!(*def.e == *lhs)) {
			// One of the phi parameters has a different base expression to lhs. Use non simple print.
			simple = false;
			break;
		}
	}
	if (simple) {
		os << "{";
		bool first = true;
		for (const auto &def : defVec) {
			if (first)
				first = false;
			else
				os << " ";

			if (def.def) {
				if (html)
					os << "<a href=\"#stmt" << def.def->getNumber() << "\">";
				os << def.def->getNumber();
				if (html)
					os << "</a>";
			} else
				os << "-";
		}
		os << "}";
	} else {
		os << "(";
		bool first = true;
		for (const auto &def : defVec) {
			if (first)
				first = false;
			else
				os << " ";

			if (def.e)
				def.e->print(os, html);
			else
				os << "NULL";
			os << "{";
			if (def.def)
				os << def.def->getNumber();
			else
				os << "-";
			os << "}";
		}
		os << ")";
	}
}
void
ImplicitAssign::printCompact(std::ostream &os, bool html) const
{
	os << "*" << type << "* ";
	if (lhs) lhs->print(os, html);
	os << " := -";
}

/**
 * \fn void Statement::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
 * \brief Code generation.
 */
void
GotoStatement::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
{
	// dont generate any code for jumps, they will be handled by the BB
}
void
BranchStatement::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
{
	// dont generate any code for jconds, they will be handled by the bb
}
void
CaseStatement::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
{
	// dont generate any code for switches, they will be handled by the bb
}
void
CallStatement::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
{
	auto p = getDestProc();

	if (!p && isComputed()) {
		hll->AddIndCallStatement(indLevel, pDest, arguments, calcResults());
		return;
	}
	StatementList *results = calcResults();

#if 0
	LOG << "call: " << this;
	LOG << " in proc " << proc->getName() << "\n";
#endif
	assert(p);
	if (Boomerang::get().noDecompile) {
		if (procDest->getSignature()->getNumReturns() > 0) {
			auto as = new Assign(new IntegerType(), Location::regOf(24), Location::regOf(24));
			as->setProc(proc);
			as->setBB(pbb);
			results->append(as);
		}

		// some hacks
		if (p->getName() == "printf"
		 || p->getName() == "scanf") {
			for (int i = 1; i < 3; ++i) {
				Exp *e = signature->getArgumentExp(i);
				assert(e);
				if (auto l = dynamic_cast<Location *>(e)) {
					l->setProc(proc);  // Needed?
				}
				auto as = new Assign(signature->getParamType(i), e->clone(), e->clone());
				as->setProc(proc);
				as->setBB(pbb);
				as->setNumber(number);  // So fromSSAform will work later
				arguments.append(as);
			}
		}
	}
	if (dynamic_cast<LibProc *>(p) && !p->getSignature()->getPreferedName().empty())
		hll->AddCallStatement(indLevel, p, p->getSignature()->getPreferedName(), arguments, results);
	else
		hll->AddCallStatement(indLevel, p, p->getName(), arguments, results);
}
void
ReturnStatement::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
{
	hll->AddReturnStatement(indLevel, &getReturns());
}
void
BoolAssign::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
{
	assert(lhs);
	assert(pCond);
	// lhs := (pCond) ? 1 : 0
	Assign as(lhs->clone(), new Ternary(opTern, pCond->clone(), new Const(1), new Const(0)));
	hll->AddAssignmentStatement(indLevel, &as);
}
void
Assign::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel)
{
	hll->AddAssignmentStatement(indLevel, this);
}

/**
 * Common to BranchStatement and BoolAssign.
 * \returns  true if this is now a floating point Branch.
 */
static bool
condToRelational(Exp *&pCond, BRANCH_TYPE jtCond)
{
	pCond = pCond->simplifyArith()->simplify();

	std::stringstream os;
	pCond->print(os);
	std::string s = os.str();

	OPER condOp = pCond->getOper();
	if (condOp == opFlagCall && strncmp(((Const *)pCond->getSubExp1())->getStr(), "SUBFLAGS", 8) == 0) {
		OPER op = opWild;
		// Special for PPC unsigned compares; may be other cases in the future
		bool makeUns = strncmp(((Const *)pCond->getSubExp1())->getStr(), "SUBFLAGSNL", 10) == 0;
		switch (jtCond) {
		case BRANCH_JE:   op = opEqual;    break;
		case BRANCH_JNE:  op = opNotEqual; break;
		case BRANCH_JSL:  op = (makeUns ? opLessUns   : opLess);   break;
		case BRANCH_JSLE: op = (makeUns ? opLessEqUns : opLessEq); break;
		case BRANCH_JSGE: op = (makeUns ? opGtrEqUns  : opGtrEq);  break;
		case BRANCH_JSG:  op = (makeUns ? opGtrUns    : opGtr);    break;
		case BRANCH_JUL:  op = opLessUns;   break;
		case BRANCH_JULE: op = opLessEqUns; break;
		case BRANCH_JUGE: op = opGtrEqUns;  break;
		case BRANCH_JUG:  op = opGtrUns;    break;
		case BRANCH_JMI:
			/*   pCond
			     /    \
			Const      opList
			"SUBFLAGS"  /   \
			           P1   opList
			                 /   \
			                P2  opList
			                     /   \
			                    P3   opNil */
			pCond = new Binary(opLess,  // P3 < 0
			                   pCond->getSubExp2()->getSubExp2()->getSubExp2()->getSubExp1()->clone(),
			                   new Const(0));
			break;
		case BRANCH_JPOS:
			pCond = new Binary(opGtrEq,  // P3 >= 0
			                   pCond->getSubExp2()->getSubExp2()->getSubExp2()->getSubExp1()->clone(),
			                   new Const(0));
			break;
		case BRANCH_JOF:
		case BRANCH_JNOF:
		case BRANCH_JPAR:
			break;
		}
		if (op != opWild) {
			pCond = new Binary(op,
			                   pCond->getSubExp2()->getSubExp1()->clone(),                 // P1
			                   pCond->getSubExp2()->getSubExp2()->getSubExp1()->clone());  // P2
		}
	} else if (condOp == opFlagCall && strncmp(((Const *)pCond->getSubExp1())->getStr(), "LOGICALFLAGS", 12) == 0) {
		// Exp *e = pCond;
		OPER op = opWild;
		switch (jtCond) {
		case BRANCH_JE:   op = opEqual; break;
		case BRANCH_JNE:  op = opNotEqual; break;
		case BRANCH_JMI:  op = opLess; break;
		case BRANCH_JPOS: op = opGtrEq; break;
		// FIXME: This next set is quite shakey. Really, we should pull all the individual flag definitions out of
		// the flag definitions, and substitute these into the equivalent conditions for the branches (a big, ugly
		// job).
		case BRANCH_JSL:  op = opLess; break;
		case BRANCH_JSLE: op = opLessEq; break;
		case BRANCH_JSGE: op = opGtrEq; break;
		case BRANCH_JSG:  op = opGtr; break;
		// These next few seem to fluke working fine on architectures like X86, SPARC, and 68K which clear the
		// carry on all logical operations.
		case BRANCH_JUL:  op = opLessUns; break;    // NOTE: this is equivalent to never branching, since nothing
		                                            // can be unsigned less than zero
		case BRANCH_JULE: op = opLessEqUns; break;
		case BRANCH_JUGE: op = opGtrEqUns; break;   // Similarly, this is equivalent to always branching
		case BRANCH_JUG:  op = opGtrUns; break;
		case BRANCH_JPAR:
			{
				// This is pentium specific too; see below for more notes.
				/*                  pCond
				                    /   \
				              Const     opList
				    "LOGICALFLAGS8"     /   \
				                opBitAnd    opNil
				                /       \
				        opFlagCall      opIntConst
				        /       \           mask
				    Const       opList
				"SETFFLAGS"     /   \
				               P1   opList
				                    /   \
				                    P2  opNil
				*/
				Exp *flagsParam = ((Binary *)((Binary *)pCond)->getSubExp2())->getSubExp1();
				Exp *test = flagsParam;
				if (auto re = dynamic_cast<RefExp *>(test))
					test = re->getSubExp1();
				if (test->isTemp())
					return false;  // Just not propagated yet
				int mask = 0;
				if (flagsParam->getOper() == opBitAnd) {
					Exp *setFlagsParam = ((Binary *)flagsParam)->getSubExp2();
					if (setFlagsParam->isIntConst())
						mask = ((Const *)setFlagsParam)->getInt();
				}
				// Sometimes the mask includes the 0x4 bit, but we expect that to be off all the time. So effectively
				// the branch is for any one of the (one or two) bits being on. For example, if the mask is 0x41, we
				// are branching of less (0x1) or equal (0x41).
				mask &= 0x41;
				OPER op = opWild;
				switch (mask) {
				case 0:
					LOG << "WARNING: unhandled pentium branch if parity with pCond = " << *pCond << "\n";
					return false;
				case 1:
					op = opLess;
					break;
				case 0x40:
					op = opEqual;
					break;
				case 0x41:
					op = opLessEq;
					break;
				default:
					break;  // Not possible, but avoid a compiler warning
				}
				pCond = new Binary(op,
				                   flagsParam->getSubExp1()->getSubExp2()->getSubExp1()->clone(),
				                   flagsParam->getSubExp1()->getSubExp2()->getSubExp2()->getSubExp1()->clone());
				return true;  // This is a floating point comparison
			}
		default:
			break;
		}
		if (op != opWild) {
			pCond = new Binary(op,
			                   pCond->getSubExp2()->getSubExp1()->clone(),
			                   new Const(0));
		}
	} else if (condOp == opFlagCall && strncmp(((Const *)pCond->getSubExp1())->getStr(), "SETFFLAGS", 9) == 0) {
		// Exp *e = pCond;
		OPER op = opWild;
		switch (jtCond) {
		case BRANCH_JE:   op = opEqual; break;
		case BRANCH_JNE:  op = opNotEqual; break;
		case BRANCH_JMI:  op = opLess; break;
		case BRANCH_JPOS: op = opGtrEq; break;
		case BRANCH_JSL:  op = opLess; break;
		case BRANCH_JSLE: op = opLessEq; break;
		case BRANCH_JSGE: op = opGtrEq; break;
		case BRANCH_JSG:  op = opGtr; break;
		default:
			break;
		}
		if (op != opWild) {
			pCond = new Binary(op,
			                   pCond->getSubExp2()->getSubExp1()->clone(),
			                   pCond->getSubExp2()->getSubExp2()->getSubExp1()->clone());
		}
	}
	// ICK! This is all PENTIUM SPECIFIC... needs to go somewhere else.
	// Might be of the form (SETFFLAGS(...) & MASK) RELOP INTCONST where MASK could be a combination of 1, 4, and 40,
	// and relop could be == or ~=.  There could also be an XOR 40h after the AND
	// From MSVC 6, we can also see MASK = 0x44, 0x41, 0x5 followed by jump if (even) parity (see above)
	// %fflags = 0..0.0 00 >
	// %fflags = 0..0.1 01 <
	// %fflags = 1..0.0 40 =
	// %fflags = 1..1.1 45 not comparable
	// Example: (SETTFLAGS(...) & 1) ~= 0
	// left = SETFFLAGS(...) & 1
	// left1 = SETFFLAGS(...) left2 = int 1, k = 0, mask = 1
	else if (condOp == opEqual || condOp == opNotEqual) {
		Exp *left  = ((Binary *)pCond)->getSubExp1();
		Exp *right = ((Binary *)pCond)->getSubExp2();
		bool hasXor40 = false;
		if (left->getOper() == opBitXor && right->isIntConst()) {
			Exp *r2 = ((Binary *)left)->getSubExp2();
			if (r2->isIntConst()) {
				int k2 = ((Const *)r2)->getInt();
				if (k2 == 0x40) {
					hasXor40 = true;
					left = ((Binary *)left)->getSubExp1();
				}
			}
		}
		if (left->getOper() == opBitAnd && right->isIntConst()) {
			Exp *left1 = ((Binary *)left)->getSubExp1();
			Exp *left2 = ((Binary *)left)->getSubExp2();
			int k = ((Const *)right)->getInt();
			// Only interested in 40, 1
			k &= 0x41;
			if (left1->isFlagCall() && left2->isIntConst()) {
				int mask = ((Const *)left2)->getInt();
				// Only interested in 1, 40
				mask &= 0x41;
				OPER op = opWild;
				if (hasXor40) {
					assert(k == 0);
					op = condOp;
				} else {
					switch (mask) {
					case 1:
						op = ((condOp == opEqual && k == 0) || (condOp == opNotEqual && k == 1)) ? opGtrEq : opLess;
						break;
					case 0x40:
						op = ((condOp == opEqual && k == 0) || (condOp == opNotEqual && k == 0x40)) ? opNotEqual : opEqual;
						break;
					case 0x41:
						switch (k) {
						case 0:
							op = (condOp == opEqual) ? opGtr : opLessEq;
							break;
						case 1:
							op = (condOp == opEqual) ? opLess : opGtrEq;
							break;
						case 0x40:
							op = (condOp == opEqual) ? opEqual : opNotEqual;
							break;
						default:
							std::cerr << "BranchStatement::simplify: k is " << std::hex << k << std::dec << "\n";
							assert(0);
						}
						break;
					default:
						std::cerr << "BranchStatement::simplify: Mask is " << std::hex << mask << std::dec << "\n";
						assert(0);
					}
				}
				if (op != opWild) {
					pCond = new Binary(op,
					                   left1->getSubExp2()->getSubExp1(),
					                   left1->getSubExp2()->getSubExp2()->getSubExp1());
					return true;  // This is now a float comparison
				}
			}
		}
	}
	return false;
}

/**
 * \fn void Statement::simplify()
 * \brief Simpify internal expressions.
 *
 * Simplify all the uses/defs in this Statement.
 */
void
GotoStatement::simplify()
{
	if (isComputed()) {
		pDest = pDest->simplifyArith();
		pDest = pDest->simplify();
	}
}
void
BranchStatement::simplify()
{
	if (pCond) {
		if (condToRelational(pCond, jtCond))
			bFloat = true;
	}
}
void
CaseStatement::simplify()
{
	if (pDest)
		pDest = pDest->simplify();
	else if (pSwitchInfo && pSwitchInfo->pSwitchVar)
		pSwitchInfo->pSwitchVar = pSwitchInfo->pSwitchVar->simplify();
}
void
CallStatement::simplify()
{
	GotoStatement::simplify();
	for (const auto &arg : arguments)
		arg->simplify();
	for (const auto &def : defines)
		def->simplify();
}
void
ReturnStatement::simplify()
{
	for (const auto &mod : modifieds)
		mod->simplify();
	for (const auto &ret : returns)
		ret->simplify();
}
void
BoolAssign::simplify()
{
	if (pCond)
		condToRelational(pCond, jtCond);
}
void
Assign::simplify()
{
	// simplify arithmetic of assignment
	if (Boomerang::get().noBranchSimplify) {
		OPER leftop = lhs->getOper();
		if (leftop == opZF || leftop == opCF || leftop == opOF || leftop == opNF)
			return;
	}

	lhs = lhs->simplifyArith();
	rhs = rhs->simplifyArith();
	if (guard) guard = guard->simplifyArith();
	// simplify the resultant expression
	lhs = lhs->simplify();
	rhs = rhs->simplify();
	if (guard) guard = guard->simplify();

	// Perhaps the guard can go away
	if (guard && guard->isTrue())
		guard = nullptr;  // No longer a guarded assignment

	if (lhs->isMemOf()) {
		lhs->setSubExp1(lhs->getSubExp1()->simplifyArith());
	}

	// this hack finds address constants.. it should go away when Mike writes some decent type analysis.
#if 0
	if (DFA_TYPE_ANALYSIS) return;
	if (lhs->isMemOf() && lhs->getSubExp1()->isSubscript()) {
		auto ref = (RefExp *)lhs->getSubExp1();
		auto phi = dynamic_cast<PhiAssign *>(ref->getDef());
		if (!phi) return;
		for (const auto &pi : *phi) {
			if (auto def = dynamic_cast<Assign *>(pi.def)) {
				// Look for rX{-} - K or K
				if (def->rhs->isIntConst()
				 || (def->rhs->getOper() == opMinus
				  && def->rhs->getSubExp1()->isSubscript()
				  && ((RefExp *)def->rhs->getSubExp1())->isImplicitDef()
				  && def->rhs->getSubExp1()->getSubExp1()->isRegOf()
				  && def->rhs->getSubExp2()->isIntConst())) {
					Exp *ne = new Unary(opAddrOf, Location::memOf(def->rhs, proc));
					if (VERBOSE)
						LOG << "replacing " << *def->rhs << " with " << *ne << " in " << *def << "\n";
					def->rhs = ne;
				}
				if (def->rhs->isAddrOf()
				 && def->rhs->getSubExp1()->isSubscript()
				 && def->rhs->getSubExp1()->getSubExp1()->isGlobal()
				 && rhs->getOper() != opPhi  // MVE: opPhi!!
				 && rhs->getOper() != opItof
				 && rhs->getOper() != opFltConst) {
					Type *ty = proc->getProg()->getGlobalType(
					            ((Const *)def->rhs->getSubExp1()->
					                                getSubExp1()->
					                                getSubExp1())->getStr());
					if (ty && ty->isArray()) {
						Type *bty = ((ArrayType *)ty)->getBaseType();
						if (bty->isFloat()) {
							if (VERBOSE)
								LOG << "replacing " << *rhs << " with ";
							rhs = new Ternary(opItof, new Const(32), new Const(bty->getSize()), rhs);
							if (VERBOSE)
								LOG << *rhs << " (assign indicates float type)\n";
						}
					}
				}
			}
		}
	}
#endif
}
void
PhiAssign::simplify()
{
	lhs = lhs->simplify();

	if (!defVec.empty()) {
		bool allSame = true;
		auto uu = defVec.begin();
		auto first = uu->def;
		for (++uu; uu != defVec.end(); ++uu) {
			if (uu->def != first) {
				allSame = false;
				break;
			}
		}

		if (allSame) {
			if (VERBOSE)
				LOG << "all the same in " << *this << "\n";
			convertToAssign(new RefExp(lhs, first));
			return;
		}

		int n = 0;
		Statement *notthis = nullptr;
		for (const auto &def : defVec) {
			if (def.def != this) {
				if (++n != 1) break;
				notthis = def.def;
			}
		}

		if (n == 1) {
			if (VERBOSE)
				LOG << "only one not this in " << *this << "\n";
			convertToAssign(new RefExp(lhs, notthis));
			return;
		}
	}
}
#if 0 // Cruft?
void
ImpRefStatement::simplify()
{
	addressExp = addressExp->simplify();
}
#endif

/**
 * \fn void Statement::simplifyAddr()
 * \brief Simplify address expressions.
 *
 * Simplify internal address expressions (a[m[x]] -> x), etc.
 */
void
Assign::simplifyAddr()
{
	lhs = lhs->simplifyAddr();
	rhs = rhs->simplifyAddr();
}
void
Assignment::simplifyAddr()
{
	lhs = lhs->simplifyAddr();
}

#if 0 // Cruft?
Exp *
Statement::getExpAtLex(unsigned int begin, unsigned int end) const
{
	return nullptr;
}

bool
Statement::mayAlias(Exp *e1, Exp *e2, int size)
{
	if (*e1 == *e2) return true;
	// Pass the expressions both ways. Saves checking things like m[exp] vs m[exp+K] and m[exp+K] vs m[exp] explicitly
	// (only need to check one of these cases)
	bool b = (calcMayAlias(e1, e2, size) && calcMayAlias(e2, e1, size));
	if (b && VERBOSE)
		LOG << "May alias: " << *e1 << " and " << *e2 << " size " << size << "\n";
	return b;
}

/**
 * \returns  true if e1 may alias e2.
 */
bool
Statement::calcMayAlias(Exp *e1, Exp *e2, int size)
{
	// currently only considers memory aliasing..
	if (!e1->isMemOf() || !e2->isMemOf()) {
		return false;
	}
	Exp *e1a = e1->getSubExp1();
	Exp *e2a = e2->getSubExp1();
	// constant memory accesses
	if (e1a->isIntConst() && e2a->isIntConst()) {
		ADDRESS a1 = ((Const *)e1a)->getAddr();
		ADDRESS a2 = ((Const *)e2a)->getAddr();
		int diff = a1 - a2;
		if (diff < 0) diff = -diff;
		if (diff * 8 >= size) return false;
	}
	// same left op constant memory accesses
	if (e1a->getArity() == 2
	 && e1a->getOper() == e2a->getOper()
	 && e1a->getSubExp2()->isIntConst()
	 && e2a->getSubExp2()->isIntConst()
	 && *e1a->getSubExp1() == *e2a->getSubExp1()) {
		int i1 = ((Const *)e1a->getSubExp2())->getInt();
		int i2 = ((Const *)e2a->getSubExp2())->getInt();
		int diff = i1 - i2;
		if (diff < 0) diff = -diff;
		if (diff * 8 >= size) return false;
	}
	// [left] vs [left +/- constant] memory accesses
	if ((e2a->getOper() == opPlus || e2a->getOper() == opMinus)
	 && *e1a == *e2a->getSubExp1()
	 && e2a->getSubExp2()->isIntConst()) {
		int i1 = 0;
		int i2 = ((Const *)e2a->getSubExp2())->getInt();
		int diff = i1 - i2;
		if (diff < 0) diff = -diff;
		if (diff * 8 >= size) return false;
	}
	// Don't need [left +/- constant ] vs [left] because called twice with
	// args reversed
	return true;
}

RangeMap
Statement::getInputRanges()
{
	if (!isFirstStatementInBB()) {
		savedInputRanges = getPreviousStatementInBB()->getRanges();
		return savedInputRanges;
	}

	assert(pbb);
	const auto &inedges = pbb->getInEdges();
	RangeMap input;
	if (inedges.empty()) {
		// setup input for start of procedure
		Range ra24(1, 0, 0, new Unary(opInitValueOf, Location::regOf(24)));
		Range ra25(1, 0, 0, new Unary(opInitValueOf, Location::regOf(25)));
		Range ra26(1, 0, 0, new Unary(opInitValueOf, Location::regOf(26)));
		Range ra27(1, 0, 0, new Unary(opInitValueOf, Location::regOf(27)));
		Range ra28(1, 0, 0, new Unary(opInitValueOf, Location::regOf(28)));
		Range ra29(1, 0, 0, new Unary(opInitValueOf, Location::regOf(29)));
		Range ra30(1, 0, 0, new Unary(opInitValueOf, Location::regOf(30)));
		Range ra31(1, 0, 0, new Unary(opInitValueOf, Location::regOf(31)));
		Range rpc(1, 0, 0, new Unary(opInitValueOf, new Terminal(opPC)));
		input.addRange(Location::regOf(24), ra24);
		input.addRange(Location::regOf(25), ra25);
		input.addRange(Location::regOf(26), ra26);
		input.addRange(Location::regOf(27), ra27);
		input.addRange(Location::regOf(28), ra28);
		input.addRange(Location::regOf(29), ra29);
		input.addRange(Location::regOf(30), ra30);
		input.addRange(Location::regOf(31), ra31);
		input.addRange(new Terminal(opPC), rpc);
	} else {
		assert(inedges.size() == 1);
		auto pred = inedges[0];
		Statement *last = pred->getLastStmt();
		assert(last);
		if (pred->getNumOutEdges() != 2) {
			input = last->getRanges();
		} else {
			assert(pred->getNumOutEdges() == 2);
			auto branch = dynamic_cast<BranchStatement *>(last);
			assert(branch);
			input = branch->getRangesForOutEdgeTo(pbb);
		}
	}

	savedInputRanges = input;

	return input;
}

void
Statement::updateRanges(RangeMap &output, std::list<Statement *> &execution_paths, bool notTaken)
{
	if (!output.isSubset(notTaken ? ((BranchStatement *)this)->getRanges2Ref() : ranges)) {
		if (notTaken)
			((BranchStatement *)this)->setRanges2(output);
		else
			ranges = output;
		if (isLastStatementInBB()) {
			const auto &outedges = pbb->getOutEdges();
			if (!outedges.empty()) {
				int arc = 0;
				if (auto branch = dynamic_cast<BranchStatement *>(this)) {
					if (outedges[0]->getLowAddr() != branch->getFixedDest())
						arc = 1;
					if (notTaken)
						arc ^= 1;
				}
				execution_paths.push_back(outedges[arc]->getFirstStmt());
			}
		} else
			execution_paths.push_back(getNextStatementInBB());
	}
}

void
Statement::rangeAnalysis(std::list<Statement *> &execution_paths)
{
	RangeMap output = getInputRanges();
	updateRanges(output, execution_paths);
}

void
Assign::rangeAnalysis(std::list<Statement *> &execution_paths)
{
	RangeMap output = getInputRanges();
	Exp *a_lhs = lhs->clone();
	if (a_lhs->isFlags()) {
		// special hacks for flags
		assert(rhs->isFlagCall());
		Exp *a_rhs = rhs->clone();
		if (a_rhs->getSubExp2()->getSubExp1()->isMemOf())
			a_rhs->getSubExp2()->getSubExp1()->setSubExp1(output.substInto(a_rhs->getSubExp2()->getSubExp1()->getSubExp1()));
		if (!dynamic_cast<Terminal *>(a_rhs->getSubExp2()->getSubExp2())
		 && a_rhs->getSubExp2()->getSubExp2()->getSubExp1()->isMemOf())
			a_rhs->getSubExp2()->getSubExp2()->getSubExp1()->setSubExp1(output.substInto(a_rhs->getSubExp2()->getSubExp2()->getSubExp1()->getSubExp1()));
		Range ra(1, 0, 0, a_rhs);
		output.addRange(a_lhs, ra);
	} else {
		if (a_lhs->isMemOf())
			a_lhs->setSubExp1(output.substInto(a_lhs->getSubExp1()->clone()));
		Exp *a_rhs = output.substInto(rhs->clone());
		if (a_rhs->isMemOf()
		 && a_rhs->getSubExp1()->getOper() == opInitValueOf
		 && a_rhs->getSubExp1()->getSubExp1()->isRegN(28))
			a_rhs = new Unary(opInitValueOf, new Terminal(opPC));   // nice hack
		if (VERBOSE && DEBUG_RANGE_ANALYSIS)
			LOG << "a_rhs is " << *a_rhs << "\n";
		if (a_rhs->isMemOf() && a_rhs->getSubExp1()->isIntConst()) {
			ADDRESS c = ((Const *)a_rhs->getSubExp1())->getInt();
			if (proc->getProg()->isDynamicLinkedProcPointer(c)) {
				if (auto nam = proc->getProg()->getDynamicProcName(c)) {
					a_rhs = new Const(nam);
					if (VERBOSE && DEBUG_RANGE_ANALYSIS)
						LOG << "a_rhs is a dynamic proc pointer to " << nam << "\n";
				}
			} else if (proc->getProg()->isReadOnly(c)) {
				switch (type->getSize()) {
				case 8:
					a_rhs = new Const(proc->getProg()->readNative1(c));
					break;
				case 16:
					a_rhs = new Const(proc->getProg()->readNative2(c));
					break;
				case 32:
					a_rhs = new Const(proc->getProg()->readNative4(c));
					break;
				default:
					LOG << "error: unhandled type size " << type->getSize() << " for reading native address\n";
				}
			} else
				if (VERBOSE && DEBUG_RANGE_ANALYSIS)
					LOG << "0x" << std::hex << c << std::dec << " is not dynamically linked proc pointer or in read only memory\n";
		}
		if ((a_rhs->getOper() == opPlus || a_rhs->getOper() == opMinus)
		 && a_rhs->getSubExp2()->isIntConst()
		 && output.hasRange(a_rhs->getSubExp1())) {
			Range &r = output.getRange(a_rhs->getSubExp1());
			int c = ((Const *)a_rhs->getSubExp2())->getInt();
			if (a_rhs->getOper() == opPlus) {
				Range ra(1,
				         r.getLowerBound() != Range::MIN ? r.getLowerBound() + c : Range::MIN,
				         r.getUpperBound() != Range::MAX ? r.getUpperBound() + c : Range::MAX,
				         r.getBase());
				output.addRange(a_lhs, ra);
			} else {
				Range ra(1,
				         r.getLowerBound() != Range::MIN ? r.getLowerBound() - c : Range::MIN,
				         r.getUpperBound() != Range::MAX ? r.getUpperBound() - c : Range::MAX,
				         r.getBase());
				output.addRange(a_lhs, ra);
			}
		} else {
			if (output.hasRange(a_rhs)) {
				output.addRange(a_lhs, output.getRange(a_rhs));
			} else {
				Exp *result;
				if (a_rhs->getMemDepth() == 0
				 && !a_rhs->search(new Terminal(opWildRegOf), result)
				 && !a_rhs->search(Location::tempOf(new Terminal(opWild)), result)) {
					if (a_rhs->isIntConst()) {
						Range ra(1, ((Const *)a_rhs)->getInt(), ((Const *)a_rhs)->getInt(), new Const(0));
						output.addRange(a_lhs, ra);
					} else {
						Range ra(1, 0, 0, a_rhs);
						output.addRange(a_lhs, ra);
					}
				} else {
					Range empty;
					output.addRange(a_lhs, empty);
				}
			}
		}
	}
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << "added " << *a_lhs << " -> " << output.getRange(a_lhs) << "\n";
	updateRanges(output, execution_paths);
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << *this << "\n";
}

void
BranchStatement::limitOutputWithCondition(RangeMap &output, Exp *e)
{
	assert(e);
	if (output.hasRange(e->getSubExp1())) {
		Range &r = output.getRange(e->getSubExp1());
		if (e->getSubExp2()->isIntConst() && r.getBase()->isIntConst() && ((Const *)r.getBase())->getInt() == 0) {
			int c = ((Const *)e->getSubExp2())->getInt();
			switch (e->getOper()) {
			case opLess:
			case opLessUns:
				{
					Range ra(r.getStride(),
					         r.getLowerBound() >= c ? c - 1 : r.getLowerBound(),
					         r.getUpperBound() >= c ? c - 1 : r.getUpperBound(),
					         r.getBase());
					output.addRange(e->getSubExp1(), ra);
				}
				break;
			case opLessEq:
			case opLessEqUns:
				{
					Range ra(r.getStride(),
					         r.getLowerBound() > c ? c : r.getLowerBound(),
					         r.getUpperBound() > c ? c : r.getUpperBound(),
					         r.getBase());
					output.addRange(e->getSubExp1(), ra);
				}
				break;
			case opGtr:
			case opGtrUns:
				{
					Range ra(r.getStride(),
					         r.getLowerBound() <= c ? c + 1 : r.getLowerBound(),
					         r.getUpperBound() <= c ? c + 1 : r.getUpperBound(),
					         r.getBase());
					output.addRange(e->getSubExp1(), ra);
				}
				break;
			case opGtrEq:
			case opGtrEqUns:
				{
					Range ra(r.getStride(),
					         r.getLowerBound() < c ? c : r.getLowerBound(),
					         r.getUpperBound() < c ? c : r.getUpperBound(),
					         r.getBase());
					output.addRange(e->getSubExp1(), ra);
				}
				break;
			case opEqual:
				{
					Range ra(r.getStride(), c, c, r.getBase());
					output.addRange(e->getSubExp1(), ra);
				}
				break;
			case opNotEqual:
				{
					Range ra(r.getStride(),
					         r.getLowerBound() == c ? c + 1 : r.getLowerBound(),
					         r.getUpperBound() == c ? c - 1 : r.getUpperBound(),
					         r.getBase());
					output.addRange(e->getSubExp1(), ra);
				}
				break;
			default:
				break;
			}
		}
	}
}

void
BranchStatement::rangeAnalysis(std::list<Statement *> &execution_paths)
{
	RangeMap output = getInputRanges();

	Exp *e = nullptr;
	// try to hack up a useful expression for this branch
	OPER op = pCond->getOper();
	if (pCond->isComparison()) {
		if (pCond->getSubExp1()->isFlags() && output.hasRange(pCond->getSubExp1())) {
			Range &r = output.getRange(pCond->getSubExp1());
			if (r.getBase()->isFlagCall()
			 && r.getBase()->getSubExp2()->getOper() == opList
			 && r.getBase()->getSubExp2()->getSubExp2()->getOper() == opList) {
				e = new Binary(op, r.getBase()->getSubExp2()->getSubExp1()->clone(), r.getBase()->getSubExp2()->getSubExp2()->getSubExp1()->clone());
				if (VERBOSE && DEBUG_RANGE_ANALYSIS)
					LOG << "calculated condition " << *e << "\n";
			}
		}
	}

	if (e)
		limitOutputWithCondition(output, e);
	updateRanges(output, execution_paths);
	output = getInputRanges();
	if (e)
		limitOutputWithCondition(output, (new Unary(opNot, e))->simplify());
	updateRanges(output, execution_paths, true);

	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << *this << "\n";
}

void
JunctionStatement::rangeAnalysis(std::list<Statement *> &execution_paths)
{
	RangeMap input;
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << "unioning {\n";
	for (const auto &edge : pbb->getInEdges()) {
		Statement *last = edge->getLastStmt();
		if (VERBOSE && DEBUG_RANGE_ANALYSIS)
			LOG << "  in BB: 0x" << std::hex << edge->getLowAddr() << std::dec << " " << *last << "\n";
		if (auto branch = dynamic_cast<BranchStatement *>(last)) {
			input.unionwith(branch->getRangesForOutEdgeTo(pbb));
		} else {
			if (auto call = dynamic_cast<CallStatement *>(last)) {
				auto d = call->getDestProc();
				auto up = dynamic_cast<UserProc *>(d);
				if (up && !up->getCFG()->findRetNode()) {
					if (VERBOSE && DEBUG_RANGE_ANALYSIS)
						LOG << "ignoring ranges from call to proc with no ret node\n";
				} else
					input.unionwith(call->getRanges());
			} else
				input.unionwith(last->getRanges());
		}
	}
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << "}\n";

	if (!input.isSubset(ranges)) {
		RangeMap output = input;

		if (output.hasRange(Location::regOf(28))) {
			Range &r = output.getRange(Location::regOf(28));
			if (r.getLowerBound() != r.getUpperBound() && r.getLowerBound() != Range::MIN) {
				if (VERBOSE)
					LOG << "stack height assumption violated " << r << " my bb: 0x" << std::hex << pbb->getLowAddr() << std::dec << "\n";
				LOG << *proc;
				assert(false);
			}
		}

		if (isLoopJunction()) {
			output = ranges;
			output.widenwith(input);
		}

		updateRanges(output, execution_paths);
	}

	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << *this << "\n";
}

void
CallStatement::rangeAnalysis(std::list<Statement *> &execution_paths)
{
	RangeMap output = getInputRanges();

	if (!this->procDest) {
		// note this assumes the call is only to one proc.. could be bad.
		Exp *d = output.substInto(getDest()->clone());
		if (d->isIntConst() || d->isStrConst()) {
			if (d->isIntConst()) {
				ADDRESS dest = ((Const *)d)->getInt();
				procDest = proc->getProg()->setNewProc(dest);
			} else {
				procDest = proc->getProg()->getLibraryProc(((Const *)d)->getStr());
			}
			if (procDest) {
				Signature *sig = procDest->getSignature();
				pDest = d;
				arguments.clear();
				for (unsigned i = 0; i < sig->getNumParams(); ++i) {
					Exp *a = sig->getParamExp(i);
					auto as = new Assign(new VoidType(), a->clone(), a->clone());
					as->setProc(proc);
					as->setBB(pbb);
					arguments.append(as);
				}
				signature = procDest->getSignature()->clone();
				m_isComputed = false;
				proc->undoComputedBB(this);
				proc->addCallee(procDest);
				LOG << "replaced indirect call with call to " << procDest->getName() << "\n";
			}
		}
	}

	if (output.hasRange(Location::regOf(28))) {
		Range &r = output.getRange(Location::regOf(28));
		int c = 4;
		if (!procDest) {
			LOG << "using push count hack to guess number of params\n";
			Statement *prev = this->getPreviousStatementInBB();
			while (prev) {
				if (auto as = dynamic_cast<Assign *>(prev)) {
					auto lhs = as->getLeft();
					auto rhs = as->getRight();
					if (lhs->isMemOf()
					 && lhs->getSubExp1()->isRegN(28)
					 && !rhs->isPC()) {
						c += 4;
					}
				}
				prev = prev->getPreviousStatementInBB();
			}
		} else if (procDest->getSignature()->getConvention() == CONV_PASCAL) {
			c += procDest->getSignature()->getNumParams() * 4;
		} else if (procDest->getName().compare(0, 6, "__imp_") == 0) {
			Statement *first = ((UserProc *)procDest)->getCFG()->getEntryBB()->getFirstStmt();
			auto call = dynamic_cast<CallStatement *>(first);
			assert(call);
			auto d = call->getDestProc();
			if (d->getSignature()->getConvention() == CONV_PASCAL)
				c += d->getSignature()->getNumParams() * 4;
		} else if (auto up = dynamic_cast<UserProc *>(procDest)) {
			if (VERBOSE) {
				LOG << "== checking for number of bytes popped ==\n"
				    << *up
				    << "== end it ==\n";
			}
			Exp *eq = up->getProven(Location::regOf(28));
			if (eq) {
				if (VERBOSE)
					LOG << "found proven " << *eq << "\n";
				if (eq->getOper() == opPlus
				 && eq->getSubExp1()->isRegN(28)
				 && eq->getSubExp2()->isIntConst()) {
					c = ((Const *)eq->getSubExp2())->getInt();
				} else
					eq = nullptr;
			}
			BasicBlock *retbb = up->getCFG()->findRetNode();
			if (retbb && !eq) {
				Statement *last = retbb->getLastStmt();
				assert(last);
				if (dynamic_cast<ReturnStatement *>(last)) {
					last->setBB(retbb);
					last = last->getPreviousStatementInBB();
				}
				if (!last) {
					// call followed by a ret, sigh
					for (const auto &edge : retbb->getInEdges()) {
						last = edge->getLastStmt();
						if (dynamic_cast<CallStatement *>(last))
							break;
					}
					if (auto call = dynamic_cast<CallStatement *>(last)) {
						auto d = call->getDestProc();
						if (d && d->getSignature()->getConvention() == CONV_PASCAL)
							c += d->getSignature()->getNumParams() * 4;
					}
					last = nullptr;
				}
				if (auto as = dynamic_cast<Assign *>(last)) {
					//LOG << "checking last statement " << last << " for number of bytes popped\n";
					assert(as->getLeft()->isRegN(28));
					Exp *t = as->getRight()->clone()->simplifyArith();
					assert(t->getOper() == opPlus
					    && t->getSubExp1()->isRegN(28)
					    && t->getSubExp2()->isIntConst());
					c = ((Const *)t->getSubExp2())->getInt();
				}
			}
		}
		Range ra(r.getStride(),
		         r.getLowerBound() == Range::MIN ? Range::MIN : r.getLowerBound() + c,
		         r.getUpperBound() == Range::MAX ? Range::MAX : r.getUpperBound() + c,
		         r.getBase());
		output.addRange(Location::regOf(28), ra);
	}
	updateRanges(output, execution_paths);
}

bool
JunctionStatement::isLoopJunction() const
{
	for (const auto &pred : pbb->getInEdges())
		if (pbb->isBackEdge(pred))
			return true;
	return false;
}

RangeMap &
BranchStatement::getRangesForOutEdgeTo(BasicBlock *out)
{
	assert(this->getFixedDest() != NO_ADDRESS);
	if (out->getLowAddr() == this->getFixedDest())
		return ranges;
	return ranges2;
}
#endif

/**
 * \brief Get *enclosing* proc (not destination proc).
 */
void
Statement::setProc(UserProc *p)
{
	proc = p;
	LocationSet exps;
	addUsedLocs(exps);
	LocationSet defs;
	getDefinitions(defs);
	exps.makeUnion(defs);
	for (const auto &exp : exps) {
		if (auto l = dynamic_cast<Location *>(exp)) {
			l->setProc(p);
		}
	}
}

bool
Statement::isFirstStatementInBB() const
{
	assert(pbb);
	assert(pbb->getRTLs());
	assert(!pbb->getRTLs()->empty());
	assert(pbb->getRTLs()->front());
	assert(!pbb->getRTLs()->front()->getList().empty());
	return this == pbb->getRTLs()->front()->getList().front();
}

bool
Statement::isLastStatementInBB() const
{
	assert(pbb);
	return this == pbb->getLastStmt();
}

Statement *
Statement::getPreviousStatementInBB() const
{
	assert(pbb);
	std::list<RTL *> *rtls = pbb->getRTLs();
	assert(rtls);
	Statement *previous = nullptr;
	for (const auto &rtl : *rtls) {
		for (const auto &stmt : rtl->getList()) {
			if (stmt == this)
				return previous;
			previous = stmt;
		}
	}
	return nullptr;
}

Statement *
Statement::getNextStatementInBB() const
{
	assert(pbb);
	std::list<RTL *> *rtls = pbb->getRTLs();
	assert(rtls);
	bool wantNext = false;
	for (const auto &rtl : *rtls) {
		for (const auto &stmt : rtl->getList()) {
			if (wantNext)
				return stmt;
			if (stmt == this)
				wantNext = true;
		}
	}
	return nullptr;
}

/**
 * \brief Output operator for Statements.
 *
 * Just makes it easier to use e.g. std::cerr << myStmt.
 *
 * \param os  Output stream to send to.
 * \param s   Statement to print to the stream.
 * \returns   os (for concatenation).
 */
std::ostream &
operator <<(std::ostream &os, const Statement *s)
{
	if (!s)
		return os << "NULL ";
	return os << *s;
}
std::ostream &
operator <<(std::ostream &os, const Statement &s)
{
	s.print(os);
	return os;
}

/**
 * \brief For logging, was also for debugging.
 */
std::string
Statement::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

#if 0 // Cruft?
/**
 * This function is designed to find basic flag calls, plus two additional
 * variations seen with Pentium FP code.  These variations involve ANDing
 * and/or XORing with constants.  So it should return true for these values of
 * e:
 * - ADDFLAGS(...)
 * - SETFFLAGS(...) & 0x45
 * - (SETFFLAGS(...) & 0x45) ^ 0x40
 *
 * FIXME: this may not be needed any more...
 */
static bool
hasSetFlags(Exp *e)
{
	if (e->isFlagCall()) return true;
	OPER op = e->getOper();
	if (op != opBitAnd && op != opBitXor) return false;
	Exp *left  = ((Binary *)e)->getSubExp1();
	Exp *right = ((Binary *)e)->getSubExp2();
	if (!right->isIntConst()) return false;
	if (left->isFlagCall()) {
		std::cerr << "hasSetFlags returns true with " << *e << "\n";
		return true;
	}
	op = left->getOper();
	if (op != opBitAnd && op != opBitXor) return false;
	right = ((Binary *)left)->getSubExp2();
	left  = ((Binary *)left)->getSubExp1();
	if (!right->isIntConst()) return false;
	bool ret = left->isFlagCall();
	if (ret)
		std::cerr << "hasSetFlags returns true with " << *e << "\n";
	return ret;
}
#endif

/**
 * Returns true if can propagate to RefExp* e in this Statement.
 *
 * \note Does not consider whether e is able to be renamed (from a memory
 * Primitive point of view), only if the definition can be propagated TO this
 * stmt.
 *
 * \note Static member function.
 */
bool
Statement::canPropagateToExp(RefExp *e)
{
	if (e->isImplicitDef())
		// Can't propagate statement "-" or "0" (implicit assignments)
		return false;
	Statement *def = e->getDef();
#if 0
	if (def == this)
		// Don't propagate to self! Can happen with %pc's (?!)
		return false;
#endif
	if (def->isNullStatement())
		// Don't propagate a null statement! Can happen with %pc's (would have no effect, and would infinitely loop)
		return false;
	auto adef = dynamic_cast<Assign *>(def);
	if (!adef) return false;  // Only propagate ordinary assignments (so far)

	if (adef->getType()->isArray()) {
		// Assigning to an array, don't propagate (Could be alias problems?)
		return false;
	}
	return true;
}

static int progress = 0;
/**
 * \brief Propagate to this statement.  Return true if a change.
 *
 * \param[out] convert  Set if an indirect call statement is converted to
 *                      direct (otherwise, no change).
 * \param destCounts    A map that indicates how may times a statement's
 *                      definition is used in this proc.
 * \param usedByDomPhi  A set of subscripted locations used in phi statements.
 * \param force         Set to true to propagate even memofs
 *                      (for switch analysis).
 *
 * \returns  true if any change.
 */
bool
Statement::propagateTo(bool &convert, std::map<Exp *, int, lessExpStar> *destCounts, LocationSet *usedByDomPhi, bool force)
{
	if (++progress > 1000) {
		std::cerr << 'p' << std::flush;
		progress = 0;
	}
	bool change;
	int changes = 0;
	// int sp = proc->getSignature()->getStackRegister(proc->getProg());
	// Exp *regSp = Location::regOf(sp);
	int propMaxDepth = Boomerang::get().propMaxDepth;
	do {
		LocationSet exps;
		addUsedLocs(exps, true);  // True to also add uses from collectors. For example, want to propagate into
		                          // the reaching definitions of calls. Third parameter defaults to false, to
		                          // find all locations, not just those inside m[...]
		change = false;           // True if changed this iteration of the do/while loop
		// Example: m[r24{10}] := r25{20} + m[r26{30}]
		// exps has r24{10}, r25{30}, m[r26{30}], r26{30}
		for (const auto &e : exps) {
			auto r = dynamic_cast<RefExp *>(e);
			if (!r || !canPropagateToExp(r))
				continue;
			Assign *def = (Assign *)r->getDef();
			Exp *rhs = def->getRight();
			// If force is true, ignore the fact that a memof should not be propagated (for switch analysis)
			if (rhs->containsBadMemof(proc) && !(force && rhs->isMemOf()))
				// Must never propagate unsubscripted memofs, or memofs that don't yet have symbols. You could be
				// propagating past a definition, thereby invalidating the IR
				continue;
			Exp *lhs = def->getLeft();

			if (EXPERIMENTAL) {
#if 0
				// This is the old "don't propagate x=f(x)" heuristic. Hopefully it will work better now that we always
				// propagate into memofs etc. However, it might need a "and we're inside the right kind of loop"
				// condition
				LocationSet used;
				def->addUsedLocs(used);
				RefExp left(def->getLeft(), (Statement *)-1);
				auto right = dynamic_cast<RefExp *>(def->getRight());
				// Beware of x := x{something else} (because we do want to do copy propagation)
				if (used.exists(&left) && !(right && *right->getSubExp1() == *left.getSubExp1()))
					// We have something like eax = eax + 1
					continue;
#else
				// This is Mike's experimental propagation limiting heuristic. At present, it is:
				// for each component of def->rhs
				//   test if the base expression is in the set usedByDomPhi
				//   if so, check if this statement OW overwrites a parameter (like ebx = ebx-1)
				//   if so, check for propagating past this overwriting statement, i.e.
				//      domNum(def) <= domNum(OW) && dimNum(OW) < domNum(def)
				//      if so, don't propagate (heuristic takes effect)
				if (usedByDomPhi) {
					LocationSet rhsComps;
					rhs->addUsedLocs(rhsComps);
					bool doNotPropagate = false;
					for (const auto &rc : rhsComps) {
						auto re = dynamic_cast<RefExp *>(rc);
						if (!re) continue;  // Sometimes %pc sneaks in
						Exp *rhsBase = re->getSubExp1();
						// We don't know the statement number for the one definition in usedInDomPhi that might exist,
						// so we use findNS()
						if (auto OW = usedByDomPhi->findNS(rhsBase)) {
							auto OWdef = dynamic_cast<Assign *>(((RefExp *)OW)->getDef());
							if (!OWdef) continue;
							auto lhsOWdef = OWdef->getLeft();
							LocationSet OWcomps;
							def->addUsedLocs(OWcomps);
							bool isOverwrite = false;
							for (const auto &ow : OWcomps) {
								if (*ow *= *lhsOWdef) {
									isOverwrite = true;
									break;
								}
							}
							if (isOverwrite) {
								// Now check for propagating a component past OWdef
								if (def->getDomNumber() <= OWdef->getDomNumber()
								 && OWdef->getDomNumber() < dominanceNum)
									// The heuristic kicks in
									doNotPropagate = true;
								break;
							}
							std::cerr << "Ow is " << *OW << "\n";
						}
					}
					if (doNotPropagate) {
						if (VERBOSE)
							LOG << "% propagation of " << def->getNumber() << " into " << number
							    << " prevented by the propagate past overwriting statement in loop heuristic\n";
						continue;
					}
				}
#endif
			}

			// Check if the -l flag (propMaxDepth) prevents this propagation
			if (destCounts && !lhs->isFlags()) {  // Always propagate to %flags
				auto ff = destCounts->find(r);
				if (ff != destCounts->end() && ff->second > 1 && rhs->getComplexityDepth(proc) >= propMaxDepth) {
					if (!def->getRight()->containsFlags()) {
						// This propagation is prevented by the -l limit
						continue;
					}
				}
			}
			change |= doPropagateTo(r, def, convert);
		}
	} while (change && ++changes < 10);
	// Simplify is very costly, especially for calls. I hope that doing one simplify at the end will not affect any
	// result...
	simplify();
	return changes > 0;  // Note: change is only for the last time around the do/while loop
}

/**
 * Experimental:  May want to propagate flags first, without tests about
 * complexity or the propagation limiting heuristic.
 */
bool
Statement::propagateFlagsTo()
{
	bool change = false, convert;
	int changes = 0;
	do {
		LocationSet exps;
		addUsedLocs(exps, true);
		for (const auto &e : exps) {
			auto re = dynamic_cast<RefExp *>(e);
			if (!re) continue;  // e.g. %pc
			auto def = dynamic_cast<Assign *>(re->getDef());
			if (!def) continue;
			Exp *base = re->getSubExp1();
			if (base->isFlags() || base->isMainFlag()) {
				change |= doPropagateTo(re, def, convert);
			}
		}
	} while (change && ++changes < 10);
	simplify();
	return change;
}

/**
 * Propagate to e from definition statement def.
 *
 * \note This procedure does not control what part of this statement is
 * propagated to.
 *
 * \param[out] convert  Set to true if an indirect call is converted to
 *                      direct.
 *
 * \returns true if a change made.
 */
bool
Statement::doPropagateTo(RefExp *e, Assign *def, bool &convert)
{
	// Respect the -p N switch
	if (Boomerang::get().numToPropagate >= 0) {
		if (Boomerang::get().numToPropagate == 0) return false;
		--Boomerang::get().numToPropagate;
	}

	if (VERBOSE)
		LOG << "propagating " << *def << "\n"
		    << "       into " << *this << "\n";

	bool change = replaceRef(e, def, convert);

	if (VERBOSE)
		LOG << "     result " << *this << "\n\n";
	return change;
}

/**
 * \brief Replace a use of def->getLeft() by def->getRight() in this
 * statement.
 *
 * Replaces a use in this statement with an expression from an ordinary
 * assignment.  Internal use only.
 *
 * \returns true if change.
 */
bool
Statement::replaceRef(RefExp *e, Assign *def, bool &convert)
{
	Exp *rhs = def->getRight();
	assert(rhs);

	Exp *base = e->getSubExp1();
	// Could be propagating %flags into %CF
	Exp *lhs = def->getLeft();
	if (base->getOper() == opCF && lhs->isFlags()) {
		if (!rhs->isFlagCall())
			return false;
		const char *str = ((Const *)((Binary *)rhs)->getSubExp1())->getStr();
		if (strncmp("SUBFLAGS", str, 8) == 0) {
			/* When the carry flag is used bare, and was defined in a subtract of the form lhs - rhs, then CF has
			   the value (lhs <u rhs).  lhs and rhs are the first and second parameters of the flagcall.
			   Note: the flagcall is a binary, with a Const (the name) and a list of expressions:
			     defRhs
			     /    \
			Const      opList
			"SUBFLAGS"  /   \
			           P1   opList
			                 /   \
			                P2  opList
			                     /   \
			                    P3   opNil
			*/
			Exp *relExp = new Binary(opLessUns,
			                         ((Binary *)rhs)->getSubExp2()->getSubExp1(),
			                         ((Binary *)rhs)->getSubExp2()->getSubExp2()->getSubExp1());
			searchAndReplace(new RefExp(new Terminal(opCF), def), relExp, true);
			return true;
		}
	}
	// need something similar for %ZF
	if (base->getOper() == opZF && lhs->isFlags()) {
		if (!rhs->isFlagCall())
			return false;
		const char *str = ((Const *)((Binary *)rhs)->getSubExp1())->getStr();
		if (strncmp("SUBFLAGS", str, 8) == 0) {
			// for zf we're only interested in if the result part of the subflags is equal to zero
			Exp *relExp = new Binary(opEqual,
			                         ((Binary *)rhs)->getSubExp2()->getSubExp2()->getSubExp2()->getSubExp1(),
			                         new Const(0));
			searchAndReplace(new RefExp(new Terminal(opZF), def), relExp, true);
			return true;
		}
	}

	// do the replacement
	//bool convert = doReplaceRef(re, rhs);
	bool ret = searchAndReplace(e, rhs, true);  // Last parameter true to change collectors
	// assert(ret);

	if (ret)
		if (auto call = dynamic_cast<CallStatement *>(this))
			convert |= call->convertToDirect();
	return ret;
}

/**
 * \fn bool Statement::isNullStatement() const
 * \brief true if is a null statement.
 */
bool
Assign::isNullStatement() const
{
	Exp *right = getRight();
	if (auto re = dynamic_cast<RefExp *>(right))
		// Must refer to self to be null
		return this == re->getDef();
	else
		// Null if left == right
		return *getLeft() == *right;
}

/**
 * \fn bool Statement::isFlagAssgn() const
 * \brief true if this statement is a flags assignment.
 */
bool
Assign::isFlagAssgn() const
{
	return getRight()->isFlagCall();
}

/**
 * \fn bool Statement::isFpush() const
 * \brief true if this statement is a fpush.
 */
bool
Assign::isFpush() const
{
	return getRight()->getOper() == opFpush;
}

/**
 * \fn bool Statement::isFpop() const
 * \brief true if this statement is a fpush.
 */
bool
Assign::isFpop() const
{
	return getRight()->getOper() == opFpop;
}

/**
 * \brief Set the destination of this jump.
 */
void
GotoStatement::setDest(Exp *pd)
{
	pDest = pd;
}

/**
 * \overload
 *
 * The destination is an ADDRESS that is converted to an Exp.
 *
 * \param addr  The new fixed address.
 */
void
GotoStatement::setDest(ADDRESS addr)
{
	// This fails in FrontSparcTest, do you really want it to Mike? -trent
	//assert(addr >= prog.limitTextLow && addr < prog.limitTextHigh);
	// Delete the old destination if there is one
	;//delete pDest;

	pDest = new Const(addr);
}

/**
 * \brief Returns the destination of this CTI.
 *
 * \returns  The Exp representing the destination of this jump.
 */
Exp *
GotoStatement::getDest() const
{
	return pDest;
}

/**
 * \brief Return the fixed destination of this CTI.  For dynamic CTIs, returns
 * NO_ADDRESS.
 *
 * Get the fixed destination of this CTI.  Assumes destination simplification
 * has already been done so that a fixed dest will be of the Exp form:
 *
 *     opIntConst dest
 *
 * \returns Fixed dest, or NO_ADDRESS if there isn't one.
 */
ADDRESS
GotoStatement::getFixedDest() const
{
	if (!pDest->isIntConst()) return NO_ADDRESS;
	return ((Const *)pDest)->getAddr();
}

/**
 * \brief Adjust the fixed destination by a given amount.  Invalid for dynamic
 * CTIs.
 *
 * Adjust the destination of this CTI by a given amount.  Causes an error if
 * this destination is not a fixed destination (i.e. a constant offset).
 *
 * \param delta  The amount to add to the destination (can be negative)
 */
void
GotoStatement::adjustFixedDest(int delta)
{
	// Ensure that the destination is fixed.
	if (!pDest || !pDest->isIntConst())
		LOG << "Can't adjust destination of non-static CTI\n";

	ADDRESS dest = ((Const *)pDest)->getAddr();
	((Const *)pDest)->setAddr(dest + delta);
}

/**
 * \brief Sets whether the destination of this CTI is computed.
 *
 * \note This should really be removed, once CaseStatement and HLNwayCall are
 * implemented properly.
 */
void
GotoStatement::setIsComputed(bool b)
{
	m_isComputed = b;
}

/**
 * \brief Returns whether or not this call is computed.
 *
 * \note This should really be removed, once CaseStatement and HLNwayCall are
 * implemented properly.
 *
 * \returns  this call is computed.
 */
bool
GotoStatement::isComputed() const
{
	return m_isComputed;
}

/**
 * Sets the BRANCH_TYPE of this jcond as well as the flag indicating whether
 * or not the floating point condition codes are used.
 *
 * \param cond       The BRANCH_TYPE.
 * \param usesFloat  This conditional jump checks the floating point condition
 *                   codes.
 */
void
BranchStatement::setCondType(BRANCH_TYPE cond, bool usesFloat)
{
	jtCond = cond;
	bFloat = usesFloat;

	// set pCond to a high level representation of this type
	Exp *p = nullptr;
	switch (cond) {
	case BRANCH_JE:
		p = new Binary(opEqual, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JNE:
		p = new Binary(opNotEqual, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JSL:
		p = new Binary(opLess, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JSLE:
		p = new Binary(opLessEq, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JSGE:
		p = new Binary(opGtrEq, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JSG:
		p = new Binary(opGtr, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JUL:
		p = new Binary(opLessUns, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JULE:
		p = new Binary(opLessEqUns, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JUGE:
		p = new Binary(opGtrEqUns, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JUG:
		p = new Binary(opGtrUns, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JMI:
		p = new Binary(opLess, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JPOS:
		p = new Binary(opGtr, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JOF:
		p = new Binary(opLessUns, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JNOF:
		p = new Binary(opGtrUns, new Terminal(opFlags), new Const(0));
		break;
	case BRANCH_JPAR:
		// Can't handle this properly here; leave an impossible expression involving %flags so propagation will
		// still happen, and we can recognise this later in condToRelational()
		// Update: these expressions seem to get ignored ???
		p = new Binary(opEqual, new Terminal(opFlags), new Const(999));
		break;
	}
	// this is such a hack.. preferably we should actually recognise SUBFLAGS32(..,..,..) > 0 instead of just
	// SUBFLAGS32(..,..,..) but I'll leave this in here for the moment as it actually works.
	if (!Boomerang::get().noDecompile)
		p = new Terminal(usesFloat ? opFflags : opFlags);
	assert(p);
	setCondExpr(p);
}

/**
 * \brief Return the Exp representing the HL condition.
 */
Exp *
BranchStatement::getCondExpr() const
{
	return pCond;
}

/**
 * \brief Set the Exp representing the HL condition.
 */
void
BranchStatement::setCondExpr(Exp *e)
{
	;//delete pCond;
	pCond = e;
}

BasicBlock *
BranchStatement::getFallBB() const
{
	ADDRESS a = getFixedDest();
	if (a == NO_ADDRESS)
		return nullptr;
	if (!pbb)
		return nullptr;
	if (pbb->getNumOutEdges() != 2)
		return nullptr;
	if (pbb->getOutEdge(0)->getLowAddr() == a)
		return pbb->getOutEdge(1);
	return pbb->getOutEdge(0);
}

/**
 * \note If you set the taken BB or fixed dest first, you will not be able to
 * set the fall BB.
 */
void
BranchStatement::setFallBB(BasicBlock *bb)
{
	ADDRESS a = getFixedDest();
	if (a == NO_ADDRESS)
		return;
	if (!pbb)
		return;
	if (pbb->getNumOutEdges() != 2)
		return;
	if (pbb->getOutEdge(0)->getLowAddr() == a) {
		pbb->setOutEdge(1, bb);
	} else {
		pbb->setOutEdge(0, bb);
	}
}

BasicBlock *
BranchStatement::getTakenBB() const
{
	ADDRESS a = getFixedDest();
	if (a == NO_ADDRESS)
		return nullptr;
	if (!pbb)
		return nullptr;
	if (pbb->getNumOutEdges() != 2)
		return nullptr;
	if (pbb->getOutEdge(0)->getLowAddr() == a)
		return pbb->getOutEdge(0);
	return pbb->getOutEdge(1);
}

void
BranchStatement::setTakenBB(BasicBlock *bb)
{
	ADDRESS a = getFixedDest();
	if (a == NO_ADDRESS)
		return;
	if (!pbb)
		return;
	if (pbb->getNumOutEdges() != 2)
		return;
	if (pbb->getOutEdge(0)->getLowAddr() == a) {
		pbb->setOutEdge(0, bb);
	} else {
		pbb->setOutEdge(1, bb);
	}
}

/**
 * \brief Return a struct with switch information in it.
 */
SWITCH_INFO *
CaseStatement::getSwitchInfo() const
{
	return pSwitchInfo;
}

/**
 * \brief Set a pointer to a SWITCH_INFO struct.
 */
void
CaseStatement::setSwitchInfo(SWITCH_INFO *psi)
{
	pSwitchInfo = psi;
}

Exp *
CallStatement::getProven(Exp *e) const
{
	if (procDest)
		return procDest->getProven(e);
	return nullptr;
}

/**
 * \brief Localise only components of e, i.e. xxx if e is m[xxx].
 */
void
CallStatement::localiseComp(Exp *e)
{
	if (e->isMemOf()) {
		((Location *)e)->setSubExp1(localiseExp(((Location *)e)->getSubExp1()));
	}
}

/**
 * \brief Localise the various components of expression e with reaching
 * definitions to this call.
 *
 * Substitute the various components of expression e with the appropriate
 * reaching definitions.  Used in e.g. fixCallBypass (via the CallBypasser).
 * Locations defined in this call are replaced with their proven values, which
 * are in terms of the initial values at the start of the call (reaching
 * definitions at the call).
 *
 * \note Can change e so usually need to clone the argument.
 *
 * \note Was called substituteParams().
 */
Exp *
CallStatement::localiseExp(Exp *e)
{
	if (!defCol.isInitialised()) return e;  // Don't attempt to subscript if the data flow not started yet
	Localiser l(this);
	e = e->clone()->accept(l);

	return e;
}

/**
 * \brief Find the reaching definition for expression e.
 *
 * Find the definition for the given expression, using the embedded Collector
 * object.
 *
 * \note Was called findArgument(), and used implicit arguments and signature
 * parameters.
 *
 * \note Must only operator on unsubscripted locations, otherwise it is
 * invalid.
 */
Exp *
CallStatement::findDefFor(Exp *e) const
{
	return defCol.findDefFor(e);
}

Type *
CallStatement::getArgumentType(int i) const
{
	assert(i < (int)arguments.size());
	auto aa = arguments.begin();
	std::advance(aa, i);
	return ((Assign *)(*aa))->getType();
}

/**
 * \brief Set the arguments of this call.
 *
 * \param args  The list of locations to set the arguments to (for testing).
 */
void
CallStatement::setArguments(StatementList &args)
{
	arguments.clear();
	arguments.append(args);
	for (const auto &arg : arguments) {
		((Assign *)arg)->setProc(proc);
		((Assign *)arg)->setBB(pbb);
	}
}

/**
 * \brief Set arguments based on signature.
 *
 * Set the arguments of this call based on signature info.
 *
 * \note Should only be called for calls to library functions.
 */
void
CallStatement::setSigArguments()
{
	if (signature) return;  // Already done
	if (!procDest)
		// FIXME: Need to check this
		return;
	// Clone here because each call to procDest could have a different signature, modified by ellipsisProcessing
	signature = procDest->getSignature()->clone();
	procDest->addCaller(this);

	if (!dynamic_cast<LibProc *>(procDest))
		return;  // Using dataflow analysis now
	int n = signature->getNumParams();
	int i;
	arguments.clear();
	for (i = 0; i < n; ++i) {
		Exp *e = signature->getArgumentExp(i);
		assert(e);
		if (auto l = dynamic_cast<Location *>(e)) {
			l->setProc(proc);  // Needed?
		}
		auto as = new Assign(signature->getParamType(i)->clone(), e->clone(), e->clone());
		as->setProc(proc);
		as->setBB(pbb);
		as->setNumber(number);  // So fromSSAform will work later. But note: this call is probably not numbered yet!
		as->setParent(this);
		arguments.append(as);
	}

	// initialize returns
	// FIXME: anything needed here?
}

/**
 * Sets a bit that says that this call is effectively followed by a return.
 * This happens e.g. on Sparc when there is a restore in the delay slot of the
 * call.
 *
 * \param b  true if this is to be set; false to clear the bit.
 */
void
CallStatement::setReturnAfterCall(bool b)
{
	returnAfterCall = b;
}

/**
 * Tests a bit that says that this call is effectively followed by a return.
 * This happens e.g. on Sparc when there is a restore in the delay slot of the
 * call.
 *
 * \returns  true if this call is effectively followed by a return.
 */
bool
CallStatement::isReturnAfterCall() const
{
	return returnAfterCall;
}

/**
 * \brief Return the destination proc.
 */
Proc *
CallStatement::getDestProc() const
{
	return procDest;
}

/**
 * \brief Set the destination proc.
 */
void
CallStatement::setDestProc(Proc *dest)
{
	assert(dest);
	// assert(!procDest);  // No: not convenient for unit testing
	procDest = dest;
}

/**
 * \brief Internal function:  Attempt to convert an indirect to a direct call.
 *
 * Attempt to convert this call, if indirect, to a direct call.
 *
 * \note At present, we igore the possibility that some other statement will
 * modify the global.  This is a serious limitation!!
 */
bool
CallStatement::convertToDirect()
{
	if (!m_isComputed)
		return false;
	bool convertIndirect = false;
	Exp *e = pDest;
	if (auto re = dynamic_cast<RefExp *>(e)) {
		if (!re->isImplicitDef())
			return false;  // If an already defined global, don't convert
		e = re->getSubExp1();
	}
	if (e->isArrayIndex()
	 && ((Binary *)e)->getSubExp2()->isIntConst()
	 && ((Const *)(((Binary *)e)->getSubExp2()))->getInt() == 0)
		e = ((Binary *)e)->getSubExp1();
	// Can actually have name{0}[0]{0} !!
	if (auto re = dynamic_cast<RefExp *>(e))
		e = re->getSubExp1();
	if (e->isIntConst()) {
		// ADDRESS u = (ADDRESS)((Const *)e)->getInt();
		// Just convert it to a direct call!
		// FIXME: to be completed
	} else if (e->isMemOf()) {
		// It might be a global that has not been processed yet
		Exp *sub = ((Location *)e)->getSubExp1();
		if (sub->isIntConst()) {
			// m[K]: convert it to a global right here
			ADDRESS u = (ADDRESS)((Const *)sub)->getInt();
			proc->getProg()->globalUsed(u);
			const char *nam = proc->getProg()->getGlobalName(u);
			e = Location::global(nam, proc);
			pDest = new RefExp(e, nullptr);
		}
	}
	if (!e->isGlobal()) {
		return false;
	}
	const char *nam = ((Const *)e->getSubExp1())->getStr();
	Prog *prog = proc->getProg();
	ADDRESS gloAddr = prog->getGlobalAddr(nam);
	ADDRESS dest = prog->readNative4(gloAddr);
	// We'd better do some limit checking on the value. This does not guarantee that it's a valid proc pointer, but it
	// may help
	if (dest < prog->getLimitTextLow() || dest >= prog->getLimitTextHigh())
		return false;  // Not a valid proc pointer
	Proc *p = prog->findProc(nam);
	bool bNewProc = !p;
	if (bNewProc)
		p = prog->setNewProc(dest);
	if (VERBOSE)
		LOG << (bNewProc ? "new" : "existing") << " procedure for call to global '" << nam << " is " << p->getName() << "\n";
	// we need to:
	// 1) replace the current return set with the return set of the new procDest
	// 2) call fixCallBypass (now fixCallAndPhiRefs) on the enclosing procedure
	// 3) fix the arguments (this will only affect the implicit arguments, the regular arguments should
	//    be empty at this point)
	// 3a replace current arguments with those of the new proc
	// 3b copy the signature from the new proc
	// 4) change this to a non-indirect call
	procDest = p;
	Signature *sig = p->getSignature();
	// pDest is currently still global5{-}, but we may as well make it a constant now, since that's how it will be
	// treated now
	pDest = new Const(dest);

	// 1
	// 2
	proc->fixCallAndPhiRefs();

	// 3
	// 3a Do the same with the regular arguments
	arguments.clear();
	for (unsigned i = 0; i < sig->getNumParams(); ++i) {
		Exp *a = sig->getParamExp(i);
		auto as = new Assign(new VoidType(), a->clone(), a->clone());
		as->setProc(proc);
		as->setBB(pbb);
		arguments.append(as);
	}
	// std::cerr << "Step 3a: arguments now: ";
#if 0
	for (const auto &arg : arguments) {
		((Assignment *)arg)->printCompact(std::cerr);
		std::cerr << ", ";
	}
	std::cerr << "\n";
	implicitArguments = newimpargs;
	assert((int)implicitArguments.size() == sig->getNumImplicitParams());
#endif

	// 3b
	signature = p->getSignature()->clone();

	// 4
	m_isComputed = false;
	proc->undoComputedBB(this);
	proc->addCallee(procDest);
	procDest->printDetailsXML();
	convertIndirect = true;

	if (VERBOSE)
		LOG << "Result of convertToDirect: " << *this << "\n";
	return convertIndirect;
}

Exp *
CallStatement::getArgumentExp(int i) const
{
	assert(i < (int)arguments.size());
	auto aa = arguments.begin();
	std::advance(aa, i);
	return ((Assign *)*aa)->getRight();
}

void
CallStatement::setArgumentExp(int i, Exp *e)
{
	assert(i < (int)arguments.size());
	auto aa = arguments.begin();
	std::advance(aa, i);
	((Assign *)*aa)->setRight(e->clone());
}

int
CallStatement::getNumArguments() const
{
	return arguments.size();
}

void
CallStatement::setNumArguments(int n)
{
	int oldSize = arguments.size();
	if (oldSize > n) {
		arguments.resize(n);
	}
	// MVE: check if these need extra propagation
	for (int i = oldSize; i < n; ++i) {
		Exp *a = procDest->getSignature()->getArgumentExp(i);
		Type *ty = procDest->getSignature()->getParamType(i);
		if (!ty && oldSize)
			ty = procDest->getSignature()->getParamType(oldSize - 1);
		if (!ty)
			ty = new VoidType();
		auto as = new Assign(ty, a->clone(), a->clone());
		as->setProc(proc);
		as->setBB(pbb);
		arguments.append(as);
	}
}

void
CallStatement::removeArgument(int i)
{
	auto aa = arguments.begin();
	std::advance(aa, i);
	arguments.erase(aa);
}

#if 0 // Cruft?
/**
 * Processes each argument of a CallStatement, and the RHS of an Assign.
 * Ad-hoc type analysis only.
 */
static Exp *
processConstant(Exp *e, Type *t, Prog *prog, UserProc *proc, ADDRESS stmt)
{
	auto nt = dynamic_cast<NamedType *>(t);
	if (nt) {
		t = nt->resolvesTo();
	}
	if (!t) return e;
	// char* and a constant
	if (e->isIntConst()) {
		if (nt && (nt->getName() == "LPCWSTR")) {
			ADDRESS u = ((Const *)e)->getAddr();
			// TODO
			LOG << "possible wide char string at 0x" << std::hex << u << std::dec << "\n";
		}
		if (auto pt = dynamic_cast<PointerType *>(t)) {
			Type *points_to = pt->getPointsTo();
			if (t->isCString()) {
				ADDRESS u = ((Const *)e)->getAddr();
				if (u != 0) {   // can't do anything with NULL
					if (auto str = prog->getStringConstant(u, true)) {
						e = new Const(str);
						// Check if we may have guessed this global incorrectly (usually as an array of char)
						if (auto nam = prog->getGlobalName(u))
							prog->setGlobalType(nam, new PointerType(new CharType()));
					} else {
						proc->getProg()->globalUsed(u);
						if (auto nam = proc->getProg()->getGlobalName(u))
							e = Location::global(nam, proc);
					}
				}
			}
			if (points_to->resolvesToFunc()) {
				ADDRESS a = ((Const *)e)->getAddr();
				if (VERBOSE || 1)
					LOG << "found function pointer with constant value " << "of type " << pt->getCtype()
					    << " in statement at addr 0x" << std::hex << stmt << std::dec << ".  Decoding address 0x" << std::hex << a << std::dec << "\n";
				// the address can be zero, i.e., NULL, if so, ignore it.
				if (a != 0) {
					if (!Boomerang::get().noDecodeChildren)
						prog->decodeEntryPoint(a);
					if (auto p = prog->findProc(a)) {
						Signature *sig = points_to->asFunc()->getSignature()->clone();
						if (sig->getName().empty()
						 || sig->getName() == "<ANON>"
						 || prog->findProc(sig->getName()))
							sig->setName(p->getName());
						else
							p->setName(sig->getName());
						sig->setForced(true);
						p->setSignature(sig);
						e = Location::global(p->getName(), proc);
					}
				}
			}
		} else if (auto ft = dynamic_cast<FloatType *>(t)) {
			e = new Ternary(opItof, new Const(32), new Const(ft->getSize()), e);
		}
	}

	return e;
}
#endif

/**
 * Process this call for ellipsis parameters.  If found, in a printf/scanf
 * call, truncate the number of parameters if needed, and return true if any
 * signature parameters added.
 *
 * This function has two jobs.  One is to truncate the list of arguments based
 * on the format string.  The second is to add parameter types to the
 * signature.
 *
 * If -Td is used, type analysis will be rerun with these changes.
 */
bool
CallStatement::ellipsisProcessing(Prog *prog)
{
	// if (!getDestProc() || !getDestProc()->getSignature()->hasEllipsis())
	if (!getDestProc() || !signature->hasEllipsis())
		return false;
	// functions like printf almost always have too many args
	const auto &name = getDestProc()->getName();
	int format = -1;
	if (name == "printf" || name == "scanf")
		format = 0;
	else if (name == "sprintf" || name == "fprintf" || name == "sscanf")
		format = 1;
	else if (getNumArguments() && getArgumentExp(getNumArguments() - 1)->isStrConst())
		format = getNumArguments() - 1;
	else return false;
	if (VERBOSE)
		LOG << "ellipsis processing for " << name << "\n";
	Exp *formatExp = getArgumentExp(format);
	// We sometimes see a[m[blah{...}]]
	if (formatExp->isAddrOf()) {
		formatExp = ((Unary *)formatExp)->getSubExp1();
		if (auto re = dynamic_cast<RefExp *>(formatExp))
			formatExp = re->getSubExp1();
		if (formatExp->isMemOf())
			formatExp = ((Location *)formatExp)->getSubExp1();
	}
	const char *formatStr = nullptr;
	if (auto re = dynamic_cast<RefExp *>(formatExp)) {
		// Maybe it's defined to be a Const string
		if (auto def = re->getDef()) {  // Not all nullptr refs get converted to implicits
			if (auto pa = dynamic_cast<PhiAssign *>(def)) {
				// More likely. Example: switch_gcc. Only need ONE candidate format string
				for (const auto &pi : *pa) {
					if (auto as = dynamic_cast<Assign *>(pi.def)) {
						auto rhs = as->getRight();
						if (rhs && rhs->isStrConst()) {
							formatStr = ((Const *)rhs)->getStr();
							break;
						}
					}
				}
			} else if (auto as = dynamic_cast<Assign *>(def)) {
				// This would be unusual; propagation would normally take care of this
				auto rhs = as->getRight();
				if (rhs && rhs->isStrConst())
					formatStr = ((Const *)rhs)->getStr();
			}
		}
	} else if (formatExp->isStrConst()) {
		formatStr = ((Const *)formatExp)->getStr();
	}
	if (!formatStr) return false;
	// actually have to parse it
	// Format string is: % [flags] [width] [.precision] [size] type
	int n = 1;  // Count the format string itself (may also be "format" more arguments)
	char ch;
	// Set a flag if the name of the function is scanf/sscanf/fscanf
	bool isScanf = name == "scanf" || name.compare(1, 5, "scanf") == 0;
	const char *p = formatStr;
	while ((p = strchr(p, '%'))) {
		++p;  // Point past the %
		bool veryLong = false;  // %lld or %L
		do {
			ch = *p++;  // Skip size and precisionA
			switch (ch) {
			case '*':
				// Example: printf("Val: %*.*f\n", width, precision, val);
				++n;  // There is an extra parameter for the width or precision
				// This extra parameter is of type integer, never int* (so pass false as last argument)
				addSigParam(new IntegerType(), false);
				continue;
			case '-':
			case '+':
			case '#':
			case ' ':
				// Flag. Ignore
				continue;
			case '.':
				// Separates width and precision. Ignore.
				continue;
			case 'h':
			case 'l':
				// size of half or long. Argument is usually still one word. Ignore.
				// Exception: %llx
				// TODO: handle architectures where l implies two words
				// TODO: at least h has implications for scanf
				if (*p == 'l') {
					// %llx
					++p;  // Skip second l
					veryLong = true;
				}
				continue;
			case 'L':
				// long. TODO: handle L for long doubles.
				// ++n;  // At least chew up one more parameter so later types are correct
				veryLong = true;
				continue;
			default:
				if ('0' <= ch && ch <= '9') continue;  // width or precision
				break;  // Else must be format type, handled below
			}
			break;
		} while (1);
		if (ch != '%')  // Don't count %%
			++n;
		switch (ch) {
		case 'd':
		case 'i':
			// Signed integer
			addSigParam(new IntegerType(veryLong ? 64 : 32), isScanf);
			break;
		case 'u':
		case 'x':
		case 'X':
		case 'o':
			// Unsigned integer
			addSigParam(new IntegerType(32, -1), isScanf);
			break;
		case 'f':
		case 'g':
		case 'G':
		case 'e':
		case 'E':
			// Various floating point formats
			// Note that for scanf, %f means float, and %lf means double, whereas for printf, both of these mean
			// double
			addSigParam(new FloatType(veryLong ? 128 : (isScanf ? 32 : 64)), isScanf);// Note: may not be 64 bits for some archs
			break;
		case 's':
			// String
			addSigParam(new PointerType(new ArrayType(new CharType)), isScanf);
			break;
		case 'c':
			// Char
			addSigParam(new CharType, isScanf);
			break;
		case '%':
			break;  // Ignore %% (emits 1 percent char)
		default:
			LOG << "Unhandled format character " << ch << " in format string for call " << *this << "\n";
		}
	}
	setNumArguments(format + n);
	signature->killEllipsis();  // So we don't do this again
	return true;
}

/**
 * Make an assign suitable for use as an argument from a callee context
 * expression.
 */
Assign *
CallStatement::makeArgAssign(Type *ty, Exp *e)
{
	Exp *lhs = e->clone();
	localiseComp(lhs);  // Localise the components of lhs (if needed)
	Exp *rhs = localiseExp(e->clone());
	auto as = new Assign(ty, lhs, rhs);
	as->setProc(proc);
	as->setBB(pbb);
	// It may need implicit converting (e.g. sp{-} -> sp{0})
	Cfg *cfg = proc->getCFG();
	if (cfg->implicitsDone()) {
		ImplicitConverter ic(cfg);
		StmtImplicitConverter sm(ic, cfg);
		as->accept(sm);
	}
	return as;
}

/**
 * \note Helper function for the above.
 */
void
CallStatement::addSigParam(Type *ty, bool isScanf)
{
	if (isScanf) ty = new PointerType(ty);
	signature->addParameter(ty);
	Exp *paramExp = signature->getParamExp(signature->getNumParams() - 1);
	if (VERBOSE)
		LOG << "  ellipsisProcessing: adding parameter " << *paramExp << " of type " << ty->getCtype() << "\n";
	if (arguments.size() < (unsigned)signature->getNumParams()) {
		Assign *as = makeArgAssign(ty, paramExp);
		arguments.append(as);
	}
}

/**
 * \brief Remove from returns only.
 *
 * Remove the return (if any) related to loc.  Loc may or may not be
 * subscripted.
 */
void
ReturnStatement::removeReturn(Exp *loc)
{
	if (auto re = dynamic_cast<RefExp *>(loc))
		loc = re->getSubExp1();
	for (auto rr = returns.begin(); rr != returns.end(); ++rr) {
		if (*((Assignment *)*rr)->getLeft() == *loc) {
			returns.erase(rr);
			return;  // Assume only one definition
		}
	}
}

void
ReturnStatement::addReturn(Assignment *a)
{
	returns.append(a);
}

/**
 * Sets the BRANCH_TYPE of this jcond as well as the flag indicating whether
 * or not the floating point condition codes are used.
 *
 * \param cond       The BRANCH_TYPE.
 * \param usesFloat  This conditional jump checks the floating point condition
 *                   codes.
 */
void
BoolAssign::setCondType(BRANCH_TYPE cond, bool usesFloat)
{
	jtCond = cond;
	bFloat = usesFloat;
	setCondExpr(new Terminal(opFlags));
}

/**
 * \brief Return the Exp representing the HL condition.
 */
Exp *
BoolAssign::getCondExpr() const
{
	return pCond;
}

/**
 * \brief Set the Exp representing the HL condition.
 */
void
BoolAssign::setCondExpr(Exp *pss)
{
	;//delete pCond;
	pCond = pss;
}

/**
 * \fn void Statement::fixSuccessor()
 * \brief fixSuccessor (succ(r2) -> r3).
 */
void
Assign::fixSuccessor()
{
	lhs = lhs->fixSuccessor();
	rhs = rhs->fixSuccessor();
}

/**
 * \brief Memory depth.
 */
int
Assign::getMemDepth() const
{
	int d1 = lhs->getMemDepth();
	int d2 = rhs->getMemDepth();
	if (d1 >= d2) return d1;
	return d2;
}

#if 0
bool
Assign::match(const char *pattern, std::map<std::string, Exp *> &bindings)
{
	if (!strstr(pattern, ":="))
		return false;
	char *left = strdup(pattern);
	char *right = strstr(left, ":=");
	*right++ = 0;
	++right;
	while (*right == ' ')
		++right;
	char *endleft = left + strlen(left) - 1;
	while (*endleft == ' ') {
		*endleft = 0;
		--endleft;
	}

	return lhs->match(left, bindings) && rhs->match(right, bindings);
}

void addPhiReferences(StatementSet &, PhiAssign *);

static void
addSimpleCopyReferences(StatementSet &stmts, Assign *def)
{
	if (!(*def->getLeft() == *def->getRight()->getSubExp1()))
		return;
	auto copy = ((RefExp *)def->getRight())->getDef();
	if (!stmts.exists(copy)) {
		stmts.insert(copy);
		if (auto pa = dynamic_cast<PhiAssign *>(copy))
			addPhiReferences(stmts, pa);
		else if (auto as = dynamic_cast<Assign *>(copy))
			if (as->getRight()->isSubscript())
				addSimpleCopyReferences(stmts, as);
	}
}

static void
addPhiReferences(StatementSet &stmts, PhiAssign *def)
{
	for (const auto &it : *def) {
		auto pa = dynamic_cast<PhiAssign *>(it.def);
		auto as = dynamic_cast<Assign *>(it.def);
		if (pa && !stmts.exists(pa)) {
			stmts.insert(pa);
			addPhiReferences(stmts, pa);
		} else if (as && as->getRight()->isSubscript()) {
			stmts.insert(as);
			addSimpleCopyReferences(stmts, as);
		} else
			stmts.insert(it.def);
	}
}
#endif

/**
 * \fn void Statement::genConstraints(LocationSet &cons)
 * \brief Generate constraints (for constraint based type analysis).
 */
void
BranchStatement::genConstraints(LocationSet &cons)
{
	if (!pCond && VERBOSE) {
		LOG << "Warning: BranchStatment " << number << " has no condition expression!\n";
		return;
	}
	Type *opsType;
	if (bFloat)
		opsType = new FloatType(0);
	else
		opsType = new IntegerType(0);
	if (jtCond == BRANCH_JUGE || jtCond == BRANCH_JULE
	 || jtCond == BRANCH_JUG  || jtCond == BRANCH_JUL) {
		assert(!bFloat);
		((IntegerType *)opsType)->bumpSigned(-1);
	} else if (jtCond == BRANCH_JSGE || jtCond == BRANCH_JSLE
	        || jtCond == BRANCH_JSG  || jtCond == BRANCH_JSL) {
		assert(!bFloat);
		((IntegerType *)opsType)->bumpSigned(+1);
	}

	// Constraints leading from the condition
	assert(pCond->getArity() == 2);
	Exp *a = ((Binary *)pCond)->getSubExp1();
	Exp *b = ((Binary *)pCond)->getSubExp2();
	// Generate constraints for a and b separately (if any).  Often only need a size, since we get basic type and
	// signedness from the branch condition (e.g. jump if unsigned less)
	Exp *Ta;
	Exp *Tb;
	if (a->isSizeCast()) {
		opsType->setSize(((Const *)((Binary *)a)->getSubExp1())->getInt());
		Ta = new Unary(opTypeOf, ((Binary *)a)->getSubExp2());
	} else
		Ta = new Unary(opTypeOf, a);
	if (b->isSizeCast()) {
		opsType->setSize(((Const *)((Binary *)b)->getSubExp1())->getInt());
		Tb = new Unary(opTypeOf, ((Binary *)b)->getSubExp2());
	} else
		Tb = new Unary(opTypeOf, b);
	// Constrain that Ta == opsType and Tb == opsType
	Exp *con = new Binary(opEqual, Ta, new TypeVal(opsType));
	cons.insert(con);
	con = new Binary(opEqual, Tb, new TypeVal(opsType));
	cons.insert(con);
}
void
CallStatement::genConstraints(LocationSet &cons)
{
	auto dest = getDestProc();
	if (!dest) return;
	Signature *destSig = dest->getSignature();
	// Generate a constraint for the type of each actual argument to be equal to the type of each formal parameter
	// (hopefully, these are already calculated correctly; if not, we need repeat till no change)
	int p = 0;
	for (const auto &aa : arguments) {
		Exp *arg = ((Assign *)aa)->getRight();
		// Handle a[m[x]]
		if (arg->isAddrOf()) {
			Exp *sub = arg->getSubExp1();
			if (auto re = dynamic_cast<RefExp *>(sub))
				sub = re->getSubExp1();
			if (sub->isMemOf())
				arg = ((Location *)sub)->getSubExp1();
		}
		if (arg->isRegOf() || arg->isMemOf() || arg->isSubscript() || arg->isLocal() || arg->isGlobal()) {
			Exp *con = new Binary(opEqual,
			                      new Unary(opTypeOf, arg->clone()),
			                      new TypeVal(destSig->getParamType(p)->clone()));
			cons.insert(con);
		}
		++p;
	}

	if (dynamic_cast<LibProc *>(dest)) {
		// A library procedure... check for two special cases
		const auto &name = dest->getName();
		// Note: might have to chase back via a phi statement to get a sample
		// string
		const char *str;
		Exp *arg0 = ((Assign *)*arguments.begin())->getRight();
		if ((name == "printf" || name == "scanf") && !!(str = arg0->getAnyStrConst())) {
			// actually have to parse it
			int n = 1;  // Number of %s plus 1 = number of args
			const char *p = str;
			while ((p = strchr(p, '%'))) {
				++p;
				Type *t = nullptr;
				int longness = 0;
				bool sign = true;
				bool cont;
				do {
					cont = false;
					switch (*p) {
					case 'u':
						sign = false;
						cont = true;
						break;
					case 'x':
						sign = false;
						// Fall through
					case 'i':
					case 'd':
						{
							int size = 32;
							// Note: the following only works for 32 bit code or where sizeof (long) == sizeof (int)
							if (longness == 2) size = 64;
							t = new IntegerType(size, sign);
						}
						break;
					case 'f':
					case 'g':
						t = new FloatType(64);
						break;
					case 's':
						t = new PointerType(new CharType());
						break;
					case 'l':
						++longness;
						cont = true;
						break;
					case '.':
						cont = true;
						break;
					case '*':
						assert(0);  // Star format not handled yet
					default:
						if (*p >= '0' && *p <= '9')
							cont = true;
						break;
					}
					++p;
				} while (cont);
				if (t) {
					// scanf takes addresses of these
					if (name == "scanf")
						t = new PointerType(t);
					// Generate a constraint for the parameter
					auto tv = new TypeVal(t);
					auto aa = arguments.begin();
					std::advance(aa, n);
					Exp *argn = ((Assign *)*aa)->getRight();
					Exp *con = argn->genConstraints(tv);
					cons.insert(con);
				}
				++n;
			}
		}
	}
}
void
Assignment::genConstraints(LocationSet &cons)
{
	// Almost every assignment has at least a size from decoding
	// MVE: do/will PhiAssign's have a valid type? Why not?
	if (type)
		cons.insert(new Binary(opEqual,
		                       new Unary(opTypeOf, new RefExp(lhs, this)),
		                       new TypeVal(type)));
}
void
Assign::genConstraints(LocationSet &cons)
{
	Assignment::genConstraints(cons);  // Gen constraint for the LHS
	Exp *con = rhs->genConstraints(new Unary(opTypeOf, new RefExp(lhs->clone(), this)));
	if (con) cons.insert(con);
}
void
PhiAssign::genConstraints(LocationSet &cons)
{
	// Generate a constraints st that all the phi's have to be the same type as
	// result
	Exp *result = new Unary(opTypeOf, new RefExp(lhs, this));
	for (const auto &def : defVec) {
		Exp *conjunct = new Binary(opEqual,
		                           result,
		                           new Unary(opTypeOf, new RefExp(def.e, def.def)));
		cons.insert(conjunct);
	}
}

/**
 * \brief Set the constant subscripts (using a visitor).
 */
int
Statement::setConscripts(int n)
{
	StmtConscriptSetter scs(n, false);
	accept(scs);
	return scs.getLast();
}

/**
 * \brief Clear the constant subscripts (using a visitor).
 */
void
Statement::clearConscripts()
{
	StmtConscriptSetter scs(0, true);
	accept(scs);
}

/**
 * \brief Cast the constant num to be of type ty.  Return true if a change was
 * made.
 */
bool
Statement::castConst(int num, Type *ty)
{
	ExpConstCaster ecc(num, ty);
	StmtModifier scc(ecc);
	accept(scc);
	return ecc.isChanged();
}

/**
 * \brief Strip all size casts.
 */
void
Statement::stripSizes()
{
	SizeStripper ss;
	StmtModifier sm(ss);
	accept(sm);
}

/**
 * \brief Bypass calls for references in this statement.
 *
 * Fix references to the returns of call statements.
 */
void
Statement::bypass()
{
	CallBypasser cb(this);
	StmtPartModifier sm(cb);  // Use the Part modifier so we don't change the top level of LHS of assigns etc
	accept(sm);
	if (cb.isTopChanged())
		simplify();  // E.g. m[esp{20}] := blah -> m[esp{-}-20+4] := blah
}

/**
 * \brief Adds (inserts) all locations (registers or memory etc) used by this
 * statement.
 *
 * Find the locations used by expressions in this Statement.
 * Use the StmtExpVisitor and UsedLocsFinder visitor classes.
 *
 * \param cc  Count Collectors.  Set to true to count the uses in collectors.
 */
void
Statement::addUsedLocs(LocationSet &used, bool cc, bool memOnly)
{
	UsedLocsFinder ulf(used, memOnly);
	UsedLocsVisitor ulv(ulf, cc);
	accept(ulv);
}

/**
 * Special version of the above for finding used locations.  Returns true if
 * defineAll was found.
 */
bool
Statement::addUsedLocals(LocationSet &used)
{
	UsedLocalFinder ulf(used, proc);
	UsedLocsVisitor ulv(ulf, false);
	accept(ulv);
	return ulf.wasAllFound();
}

/**
 * \brief For all expressions in this Statement, replace any/all e with
 * e{def}.
 */
void
Statement::subscriptVar(Exp *e, Statement *def /*, Cfg *cfg */)
{
	ExpSubscripter es(e, def /*, cfg*/);
	StmtSubscripter ss(es);
	accept(ss);
}

/**
 * \brief Find all constants in this Statement.
 */
void
Statement::findConstants(std::list<Const *> &lc)
{
	ConstFinder cf(lc);
	StmtExpVisitor scf(cf);
	accept(scf);
}

/**
 * Convert this PhiAssignment to an ordinary Assignment.  Hopefully, this is
 * the only place that Statements change from one class to another.  All
 * throughout the code, we assume that the addresses of Statement objects do
 * not change, so we need this slight hack to overwrite one object with
 * another.
 */
void
PhiAssign::convertToAssign(Exp *rhs)
{
	// I believe we always want to propagate to these ex-phi's; check!:
	rhs = rhs->propagateAll();
	// Thanks to tamlin for this cleaner way of implementing this hack
	static_assert(sizeof (Assign) <= sizeof (PhiAssign), "");
	auto n = number;  // These items disappear with the destructor below
	auto bb = pbb;
	auto p = proc;
	auto type_ = type;
	auto lhs_ = lhs;
	this->~PhiAssign();                             // Explicitly destroy this, but keep the memory allocated.
	auto a = new(this) Assign(type_, lhs_, rhs);// construct in-place. Note that 'a' == 'this'
	a->setNumber(n);
	a->setProc(p);
	a->setBB(bb);
#if 0
	RTL *rtl = bb->getRTLWithStatement(this);
	if (rtl->getAddress() == 0)
		rtl->setAddress(1);  // Strange things happen to real assignments with address 0
#endif
}

void
PhiAssign::putAt(int i, Statement *def, Exp *e)
{
	assert(e); // should be something surely
	if (i >= (int)defVec.size())
		defVec.resize(i + 1);  // Note: possible to insert uninitialised elements
	defVec[i].def = def;
	defVec[i].e = e;
}

/**
 * \fn bool Statement::isDefinition() const
 * \returns  true if this statement defines anything.
 */
bool
CallStatement::isDefinition() const
{
	LocationSet defs;
	getDefinitions(defs);
	return defs.size() != 0;
}

/**
 * \fn bool Statement::definesLoc(Exp *loc) const
 * \returns  true if this Statement defines loc.
 *
 * Does a ReturnStatement define anything?  Not really, the locations are
 * already defined earlier in the procedure.  However, nothing comes after the
 * return statement, so it doesn't hurt to pretend it does, and this is a
 * place to store the return type(s) for example.
 *
 * FIXME:  Seems it would be cleaner to say that ReturnStatements don't define
 * anything.
 */
bool
Assignment::definesLoc(Exp *loc) const
{
	if (lhs->getOper() == opAt)  // For foo@[lo:hi], match of foo==loc OR whole thing == loc
		if (*((Ternary *)lhs)->getSubExp1() == *loc) return true;
	return *lhs == *loc;
}
bool
CallStatement::definesLoc(Exp *loc) const
{
	for (const auto &def : defines) {
		Exp *lhs = ((Assign *)def)->getLeft();
		if (*lhs == *loc)
			return true;
	}
	return false;
}
bool
ReturnStatement::definesLoc(Exp *loc) const
{
	for (const auto &mod : modifieds) {
		if (mod->definesLoc(loc))
			return true;
	}
	return false;
}

/**
 * \fn void Statement::getDefinitions(LocationSet &def) const
 * \returns  A set of locations defined by this Statement.
 *
 * All the Assignment-derived classes have the same definitions:  the lhs.
 *
 * Classes with no definitions (e.g. GotoStatement and children) don't
 * override this.  FIXME:  ReturnStatement may not want to either, see
 * definesLoc().
 */
void
CallStatement::getDefinitions(LocationSet &defs) const
{
	for (const auto &def : defines)
		defs.insert(((Assignment *)def)->getLeft());
	// Childless calls are supposed to define everything. In practice they don't really define things like %pc, so we
	// need some extra logic in getTypeFor()
	if (isChildless() && !Boomerang::get().assumeABI)
		defs.insert(new Terminal(opDefineAll));
}
void
ReturnStatement::getDefinitions(LocationSet &ls) const
{
	for (const auto &mod : modifieds)
		mod->getDefinitions(ls);
}
void
Assignment::getDefinitions(LocationSet &defs) const
{
	if (lhs->getOper() == opAt)  // foo@[lo:hi] really only defines foo
		defs.insert(((Ternary *)lhs)->getSubExp1());
	else
		defs.insert(lhs);
	// Special case: flag calls define %CF (and others)
	if (lhs->isFlags()) {
		defs.insert(new Terminal(opCF));
		defs.insert(new Terminal(opZF));
	}
}
void
BoolAssign::getDefinitions(LocationSet &defs) const
{
	defs.insert(getLeft());
}

/**
 * \fn Type *Statement::getTypeFor(Exp *e) const
 * \brief Get the type for the definition, if any, for expression e in this
 * statement.
 */
/**
 * \brief Get the type defined by this Statement for this location.
 *
 * Scan the returns for e.  If found, return the type associated with that
 * return.
 */
Type *
CallStatement::getTypeFor(Exp *e) const
{
	// The defines "cache" what the destination proc is defining
	if (auto as = defines.findOnLeft(e))
		return as->getType();
	if (e->isPC())
		// Special case: just return void*
		return new PointerType(new VoidType);
	return new VoidType;
}
Type *
ReturnStatement::getTypeFor(Exp *e) const
{
	for (const auto &mod : modifieds) {
		if (*((Assignment *)mod)->getLeft() == *e)
			return ((Assignment *)mod)->getType();
	}
	return nullptr;
}
/**
 * \brief Get the type for this assignment.  It should define e.
 */
Type *
Assignment::getTypeFor(Exp *e) const
{
	// assert(*lhs == *e);  // No: local vs base expression
	return type;
}

/**
 * \fn void Statement::setTypeFor(Exp *e, Type *ty)
 * \brief Set the type for the definition of e in this Statement.
 */
void
CallStatement::setTypeFor(Exp *e, Type *ty)
{
	if (auto as = defines.findOnLeft(e))
		return as->setType(ty);
	// See if it is in our reaching definitions
	Exp *ref = defCol.findDefFor(e);
	if (auto re = dynamic_cast<RefExp *>(ref))
		if (auto def = re->getDef())
			def->setTypeFor(e, ty);
}
void
ReturnStatement::setTypeFor(Exp *e, Type *ty)
{
	for (const auto &mod : modifieds) {
		if (*((Assignment *)mod)->getLeft() == *e) {
			((Assignment *)mod)->setType(ty);
			break;
		}
	}
	for (const auto &ret : returns) {
		if (*((Assignment *)ret)->getLeft() == *e) {
			((Assignment *)ret)->setType(ty);
			return;
		}
	}
}
/**
 * \brief Set the type for this assignment.  It should define e.
 */
void
Assignment::setTypeFor(Exp *e, Type *ty)
{
	// assert(*lhs == *e);
	Type *oldType = type;
	type = ty;
	if (DEBUG_TA && oldType != ty)
		LOG << "    changed type of " << *this << "  (type was " << oldType->getCtype() << ")\n";
}

bool
lessAssignment::operator ()(const Assignment *x, const Assignment *y) const
{
	auto xx = const_cast<Assignment *>(x);
	auto yy = const_cast<Assignment *>(y);
	return (*xx->getLeft() < *yy->getLeft());  // Compare the LHS expressions
}

bool
lessAssign::operator ()(const Assign *x, const Assign *y) const
{
	auto xx = const_cast<Assign *>(x);
	auto yy = const_cast<Assign *>(y);
	return (*xx->getLeft() < *yy->getLeft());  // Compare the LHS expressions
}

/**
 * \brief Update modifieds from the collector.
 *
 * Update the modifieds, in case the signature and hence ordering and
 * filtering has changed, or the locations in the collector have changed.
 * Does NOT remove preserveds (deferred until updating returns).
 */
void
ReturnStatement::updateModifieds()
{
	Signature *sig = proc->getSignature();
	auto oldMods = StatementList();
	oldMods.swap(modifieds);

	const auto &inedges = pbb->getInEdges();
	if (inedges.size() == 1)
		if (auto call = dynamic_cast<CallStatement *>(inedges[0]->getLastStmt()))
			if (call->getDestProc() && FrontEnd::noReturnCallDest(call->getDestProc()->getName()))
				return;

	// For each location in the collector, make sure that there is an assignment in the old modifieds, which will
	// be filtered and sorted to become the new modifieds
	// Ick... O(N*M) (N existing modifeds, M collected locations)
	for (const auto &def : col) {
		bool found = false;
		Assign *as = (Assign *)def;
		Exp *colLhs = as->getLeft();
		if (proc->filterReturns(colLhs))
			continue;  // Filtered out
		for (const auto &mod : oldMods) {
			Exp *lhs = ((Assign *)mod)->getLeft();
			if (*lhs == *colLhs) {
				found = true;
				break;
			}
		}
		if (!found) {
			auto ias = new ImplicitAssign(as->getType()->clone(), as->getLeft()->clone());
			ias->setProc(proc);  // Comes from the Collector
			ias->setBB(pbb);
			oldMods.append(ias);
		}
	}

	// Mostly the old modifications will be in the correct order, and inserting will be fastest near the start of the
	// new list. So read the old modifications in reverse order
	for (auto it = oldMods.end(); it != oldMods.begin(); ) {
		--it;  // Becuase we are using a forwards iterator backwards
		// Make sure the LHS is still in the collector
		Assign *as = (Assign *)*it;
		Exp *lhs = as->getLeft();
		if (!col.existsOnLeft(lhs))
			continue;  // Not in collector: delete it (don't copy it)
		if (proc->filterReturns(lhs))
			continue;  // Filtered out: delete it

		// Insert as, in order, into the existing set of modifications
		auto nn = modifieds.begin();
		for (; nn != modifieds.end(); ++nn)
			if (sig->returnCompare(*as, *(Assign *)*nn))  // If the new assignment is less than the current one
				break;  // then insert before this position
		modifieds.insert(nn, as);
	}
}

/**
 * \brief Update returns from the modifieds.
 *
 * Update the returns, in case the signature and hence ordering and filtering
 * has changed, or the locations in the modifieds list.
 */
void
ReturnStatement::updateReturns()
{
	Signature *sig = proc->getSignature();
	int sp = sig->getStackRegister();
	auto oldRets = StatementList();
	oldRets.swap(returns);
	// For each location in the modifieds, make sure that there is an assignment in the old returns, which will
	// be filtered and sorted to become the new returns
	// Ick... O(N*M) (N existing returns, M modifieds locations)
	for (const auto &mod : modifieds) {
		bool found = false;
		Exp *loc = ((Assignment *)mod)->getLeft();
		if (proc->filterReturns(loc))
			continue;  // Filtered out
		// Special case for the stack pointer: it has to be a modified (otherwise, the changes will bypass the calls),
		// but it is not wanted as a return
		if (loc->isRegN(sp)) continue;
		for (const auto &ret : oldRets) {
			Exp *lhs = ((Assign *)ret)->getLeft();
			if (*lhs == *loc) {
				found = true;
				break;
			}
		}
		if (!found) {
			Exp *rhs = col.findDefFor(loc);  // Find the definition that reaches the return statement's collector
			auto as = new Assign(loc->clone(), rhs->clone());
			as->setProc(proc);
			as->setBB(pbb);
			oldRets.append(as);
		}
	}

	// Mostly the old returns will be in the correct order, and inserting will be fastest near the start of the
	// new list. So read the old returns in reverse order
	for (auto it = oldRets.end(); it != oldRets.begin(); ) {
		--it;  // Becuase we are using a forwards iterator backwards
		// Make sure the LHS is still in the modifieds
		Assign *as = (Assign *)*it;
		Exp *lhs = as->getLeft();
		if (!modifieds.existsOnLeft(lhs))
			continue;  // Not in modifieds: delete it (don't copy it)
		if (proc->filterReturns(lhs))
			continue;  // Filtered out: delete it
#if 1
		// Preserveds are NOT returns (nothing changes, so what are we returning?)
		// Check if it is a preserved location, e.g. r29 := r29{-}
		auto rhs = dynamic_cast<RefExp *>(as->getRight());
		if (rhs && rhs->isImplicitDef() && *rhs->getSubExp1() == *lhs)
			continue;  // Filter out the preserveds
#endif

		// Insert as, in order, into the existing set of returns
		auto nn = returns.begin();
		for (; nn != returns.end(); ++nn)
			if (sig->returnCompare(*as, *(Assign *)*nn))  // If the new assignment is less than the current one
				break;  // then insert before this position
		returns.insert(nn, as);
	}
}

/**
 * \brief Update the defines based on a callee change.
 *
 * Set the defines to the set of locations modified by the callee, or if no
 * callee, to all variables live at this call.
 */
void
CallStatement::updateDefines()
{
	Signature *sig;
	if (procDest)
		// The signature knows how to order the returns
		sig = procDest->getSignature();
	else
		// Else just use the enclosing proc's signature
		sig = proc->getSignature();

	if (dynamic_cast<LibProc *>(procDest)) {
		sig->setLibraryDefines(&defines);  // Set the locations defined
		return;
	} else if (Boomerang::get().assumeABI) {
		// Risky: just assume the ABI caller save registers are defined
		Signature::setABIdefines(proc->getProg(), &defines);
		return;
	}

	// Move the defines to a temporary list
	auto oldDefines = StatementList();
	oldDefines.swap(defines);

	if (procDest && calleeReturn) {
		StatementList &modifieds = ((UserProc *)procDest)->getModifieds();
		for (const auto &mod : modifieds) {
			Assign *as = (Assign *)mod;
			Exp *loc = as->getLeft();
			if (proc->filterReturns(loc))
				continue;
			Type *ty = as->getType();
			if (!oldDefines.existsOnLeft(loc))
				oldDefines.append(new ImplicitAssign(ty, loc));
		}
	} else {
		// Ensure that everything in the UseCollector has an entry in oldDefines
		for (const auto &use : useCol) {
			Exp *loc = use;
			if (proc->filterReturns(loc))
				continue;  // Filtered out
			if (!oldDefines.existsOnLeft(loc)) {
				auto as = new ImplicitAssign(loc->clone());
				as->setProc(proc);
				as->setBB(pbb);
				oldDefines.append(as);
			}
		}
	}

	for (auto it = oldDefines.end(); it != oldDefines.begin(); ) {
		--it;  // Becuase we are using a forwards iterator backwards
		// Make sure the LHS is still in the return or collector
		Assign *as = (Assign *)*it;
		Exp *lhs = as->getLeft();
		if (calleeReturn) {
			if (!calleeReturn->definesLoc(lhs))
				continue;  // Not in callee returns
		} else {
			if (!useCol.exists(lhs))
				continue;  // Not in collector: delete it (don't copy it)
		}
		if (proc->filterReturns(lhs))
			continue;  // Filtered out: delete it

		// Insert as, in order, into the existing set of definitions
		auto nn = defines.begin();
		for (; nn != defines.end(); ++nn)
			if (sig->returnCompare(*as, *(Assign *)*nn))  // If the new assignment is less than the current one
				break;  // then insert before this position
		defines.insert(nn, as);
	}
}

/**
 * A helper class for updateArguments.  It just dishes out a new argument from
 * one of the three sources:  The signature, the callee parameters, or the
 * defCollector in the call.
 */
class ArgSourceProvider {
	enum Src { SRC_LIB, SRC_CALLEE, SRC_COL };
	Src src;
	CallStatement *call;
	int i, n;                    // For SRC_LIB
	Signature *callSig;
	StatementList::iterator pp;  // For SRC_CALLEE
	StatementList *calleeParams;
	DefCollector::iterator cc;   // For SRC_COL
	DefCollector *defCol;
public:
	ArgSourceProvider(CallStatement *);
	Exp *nextArgLoc();
	Type *curType(Exp *);
	bool exists(Exp *);
	Exp *localise(Exp *);
};

ArgSourceProvider::ArgSourceProvider(CallStatement *call) :
	call(call)
{
	auto procDest = call->getDestProc();
	if (dynamic_cast<LibProc *>(procDest)) {
		src = SRC_LIB;
		callSig = call->getSignature();
		n = callSig->getNumParams();
		i = 0;
	} else if (call->getCalleeReturn()) {
		src = SRC_CALLEE;
		calleeParams = &((UserProc *)procDest)->getParameters();
		pp = calleeParams->begin();
	} else {
		Signature *destSig = nullptr;
		if (procDest)
			destSig = procDest->getSignature();
		if (destSig && destSig->isForced()) {
			src = SRC_LIB;
			callSig = destSig;
			n = callSig->getNumParams();
			i = 0;
		} else {
			src = SRC_COL;
			defCol = call->getDefCollector();
			cc = defCol->begin();
		}
	}
}

/**
 * \brief Get the next location (not subscripted).
 */
Exp *
ArgSourceProvider::nextArgLoc()
{
	Exp *s;
	bool allZero;
	switch (src) {
	case SRC_LIB:
		if (i == n) return nullptr;
		s = callSig->getParamExp(i++)->clone();
		s->removeSubscripts(allZero);  // e.g. m[sp{-} + 4] -> m[sp + 4]
		call->localiseComp(s);
		return s;
	case SRC_CALLEE:
		if (pp == calleeParams->end()) return nullptr;
		s = ((Assignment *)*pp++)->getLeft()->clone();
		s->removeSubscripts(allZero);
		call->localiseComp(s);  // Localise the components. Has the effect of translating into the contect of this caller
		return s;
	case SRC_COL:
		if (cc == defCol->end()) return nullptr;
		// Give the location, i.e. the left hand side of the assignment
		return ((Assign *)*cc++)->getLeft();
	}
	return nullptr;  // Suppress warning
}

/**
 * \brief Localise to this call if necessary.
 */
Exp *
ArgSourceProvider::localise(Exp *e)
{
	if (src == SRC_COL) {
		// Provide the RHS of the current assignment
		Exp *ret = ((Assign *)*--cc)->getRight();
		++cc;
		return ret;
	}
	// Else just use the call to localise
	return call->localiseExp(e);
}

/**
 * \brief Get the current location's type.
 */
Type *
ArgSourceProvider::curType(Exp *e)
{
	switch (src) {
	case SRC_LIB:
		return callSig->getParamType(i - 1);
	case SRC_CALLEE:
		{
			Type *ty = ((Assignment *)*--pp)->getType();
			++pp;
			return ty;
		}
	case SRC_COL:
		{
			// Mostly, there won't be a type here, I would think...
			Type *ty = (*--cc)->getType();
			++cc;
			return ty;
		}
	}
	return nullptr;  // Suppress warning
}

/**
 * \brief True if the given location (not subscripted) exists as a source.
 */
bool
ArgSourceProvider::exists(Exp *e)
{
	bool allZero;
	switch (src) {
	case SRC_LIB:
		if (callSig->hasEllipsis())
			// FIXME: for now, just don't check
			return true;
		for (i = 0; i < n; ++i) {
			Exp *sigParam = callSig->getParamExp(i)->clone();
			sigParam->removeSubscripts(allZero);
			call->localiseComp(sigParam);
			if (*sigParam == *e)
				return true;
		}
		return false;
	case SRC_CALLEE:
		for (const auto &pp : *calleeParams) {
			Exp *par = ((Assignment *)pp)->getLeft()->clone();
			par->removeSubscripts(allZero);
			call->localiseComp(par);
			if (*par == *e)
				return true;
		}
		return false;
	case SRC_COL:
		return defCol->existsOnLeft(e);
	}
	return false;  // Suppress warning
}

/**
 * \brief Update the arguments based on a callee change.
 */
void
CallStatement::updateArguments()
{
	/*
		If this is a library call, source = signature
		else if there is a callee return, source = callee parameters
		else
			if a forced callee signature, source = signature
			else source is def collector in this call.
		move arguments to oldArguments
		for each arg lhs in source
			if exists in oldArguments, leave alone
			else if not filtered append assignment lhs=lhs to oldarguments
		for each argument as in oldArguments in reverse order
			lhs = as->getLeft
			if (lhs does not exist in source) continue
			if filterParams(lhs) continue
			insert as into arguments, considering sig->argumentCompare
	*/
	// Note that if propagations are limited, arguments and collected reaching definitions can be in terms of phi
	// statements that have since been translated to assignments. So propagate through them now
	// FIXME: reconsider! There are problems (e.g. with test/pentium/fromSSA2, test/pentium/fbranch) if you propagate
	// to the expressions in the arguments (e.g. m[esp{phi1}-20]) but don't propagate into ordinary statements that
	// define the actual argument. For example, you might have m[esp{-}-56] in the call, but the actual definition of
	// the printf argument is still m[esp{phi1} -20] = "%d".
	if (EXPERIMENTAL) {
		bool convert;
		proc->propagateStatements(convert, 88);
	}
	auto oldArguments = StatementList();
	oldArguments.swap(arguments);
	if (EXPERIMENTAL) {
		// I don't really know why this is needed, but I was seeing r28 := ((((((r28{-}-4)-4)-4)-8)-4)-4)-4:
		for (const auto &def : defCol)
			def->simplify();
	}

	Signature *sig = proc->getSignature();
	// Ensure everything in the callee's signature (if this is a library call), or the callee parameters (if available),
	// or the def collector if not,  exists in oldArguments
	ArgSourceProvider asp(this);
	Exp *loc;
	while (!!(loc = asp.nextArgLoc())) {
		if (proc->filterParams(loc))
			continue;
		if (!oldArguments.existsOnLeft(loc)) {
			// Check if the location is renamable. If not, localising won't work, since it relies on definitions
			// collected in the call, and you just get m[...]{-} even if there are definitions.
			Exp *rhs;
			if (proc->canRename(loc))
				rhs = asp.localise(loc->clone());
			else
				rhs = loc->clone();
			Type *ty = asp.curType(loc);
			auto as = new Assign(ty, loc->clone(), rhs);
			as->setNumber(number);  // Give the assign the same statement number as the call (for now)
			as->setParent(this);
			as->setProc(proc);
			as->setBB(pbb);
			oldArguments.append(as);
		}
	}

	for (auto it = oldArguments.end(); it != oldArguments.begin(); ) {
		--it;  // Becuase we are using a forwards iterator backwards
		// Make sure the LHS is still in the callee signature / callee parameters / use collector
		Assign *as = (Assign *)*it;
		Exp *lhs = as->getLeft();
		if (!asp.exists(lhs)) continue;
		if (proc->filterParams(lhs))
			continue;  // Filtered out: delete it

		// Insert as, in order, into the existing set of definitions
		auto nn = arguments.begin();
		for (; nn != arguments.end(); ++nn)
			if (sig->argumentCompare(*as, *(Assign *)*nn))  // If the new assignment is less than the current one
				break;  // then insert before this position
		arguments.insert(nn, as);
	}
}

/**
 * \brief Calculate defines(this) isect live(this).
 *
 * Calculate results(this) = defines(this) isect live(this).
 *
 * \note Could use a LocationList for this, but then there is nowhere to store
 * the types (for DFA based TA).  So the RHS is just ignored.
 */
StatementList *
CallStatement::calcResults()
{
	auto ret = new StatementList;
	if (procDest) {
		if (dynamic_cast<LibProc *>(procDest)) {
			auto sig = procDest->getSignature();
			int n = sig->getNumReturns();
			for (int i = 1; i < n; ++i) {  // Ignore first (stack pointer) return
				Exp *sigReturn = sig->getReturnExp(i);
#if SYMS_IN_BACK_END
				// But we have translated out of SSA form, so some registers have had to have been replaced with locals
				// So wrap the return register in a ref to this and check the locals
				auto wrappedRet = new RefExp(sigReturn, this);
				const char *locName = proc->findLocal(wrappedRet);  // E.g. r24{16}
				if (locName)
					sigReturn = Location::local(locName, proc);  // Replace e.g. r24 with local19
#endif
				if (useCol.exists(sigReturn)) {
					auto as = new ImplicitAssign(getTypeFor(sigReturn), sigReturn);
					ret->append(as);
				}
			}
		} else {
			Exp *rsp = Location::regOf(proc->getSignature()->getStackRegister(proc->getProg()));
			for (const auto &def : defines) {
				Exp *lhs = ((Assign *)def)->getLeft();
				// The stack pointer is allowed as a define, so remove it here as a special case non result
				if (*lhs == *rsp) continue;
				if (useCol.exists(lhs))
					ret->append(def);
			}
		}
	} else {
		// For a call with no destination at this late stage, use everything live at the call except for the stack
		// pointer register. Needs to be sorted
		auto sig = proc->getSignature();
		int sp = sig->getStackRegister();
		for (const auto &use : useCol) {  // Iterates through reaching definitions
			Exp *loc = use;
			if (proc->filterReturns(loc)) continue;        // Ignore filtered locations
			if (loc->isRegN(sp)) continue;                 // Ignore the stack pointer
			auto as = new ImplicitAssign(loc);  // Create an implicit assignment
			auto nn = ret->begin();
			for (; nn != ret->end(); ++nn)  // Iterates through new results
				if (sig->returnCompare(*as, *(Assignment *)*nn))  // If the new assignment is less than the current one,
					break;  // then insert before this position
			ret->insert(nn, as);
		}
	}
	return ret;
}

void
CallStatement::removeDefine(Exp *e)
{
	for (auto ss = defines.begin(); ss != defines.end(); ++ss) {
		Assign *as = ((Assign *)*ss);
		if (*as->getLeft() == *e) {
			defines.erase(ss);
			return;
		}
	}
	LOG << "WARNING: could not remove define " << *e << " from call " << *this << "\n";
}

bool
CallStatement::isChildless() const
{
	if (!procDest) return true;
	auto up = dynamic_cast<UserProc *>(procDest);
	if (!up) return false;
	// Early in the decompile process, recursive calls are treated as childless, so they use and define all
	if (up->isEarlyRecursive())
		return true;
	return !calleeReturn;
}

/**
 * Do the call bypass logic e.g. r28{20} -> r28{17} + 4 (where 20 is this
 * CallStatement).
 *
 * \param[out] ch  Set if changed (bypassed).
 */
Exp *
CallStatement::bypassRef(RefExp *r, bool &ch)
{
	Exp *base = r->getSubExp1();
	Exp *proven;
	ch = false;
	if (dynamic_cast<LibProc *>(procDest)) {
		auto sig = procDest->getSignature();
		proven = sig->getProven(base);
		if (!proven) {  // Not (known to be) preserved
			if (sig->findReturn(base) != -1)
				return r;  // Definately defined, it's the return
			// Otherwise, not all that sure. Assume that library calls pass things like local variables
		}
	} else {
		// Was using the defines to decide if something is preserved, but consider sp+4 for stack based machines
		// Have to use the proven information for the callee (if any)
		if (!procDest)
			return r;  // Childless callees transmit nothing
		//if (procDest->isLocal(base))  // ICK! Need to prove locals and parameters through calls...
		// FIXME: temporary HACK! Ignores alias issues.
		auto up = dynamic_cast<UserProc *>(procDest);
		if (up && up->isLocalOrParamPattern(base)) {
			Exp *ret = localiseExp(base->clone());  // Assume that it is proved as preserved
			ch = true;
			if (VERBOSE)
				LOG << *base << " allowed to bypass call statement " << number << " ignoring aliasing; result " << *ret << "\n";
			return ret;
		}

		proven = procDest->getProven(base);  // e.g. r28+4
	}
	if (!proven)
		return r;  // Can't bypass, since nothing proven
	Exp *to = localiseExp(base);  // e.g. r28{17}
	assert(to);
	proven = proven->clone();  // Don't modify the expressions in destProc->proven!
	proven = proven->searchReplaceAll(base, to, ch);  // e.g. r28{17} + 4
	if (ch && VERBOSE)
		LOG << "bypassRef() replacing " << *r << " with " << *proven << "\n";
	return proven;
}

/**
 * \brief Remove from modifieds AND from returns.
 */
void
ReturnStatement::removeModified(Exp *loc)
{
	modifieds.removeDefOf(loc);
	returns.removeDefOf(loc);
}

/**
 * \brief For testing.
 */
void
CallStatement::addDefine(ImplicitAssign *as)
{
	defines.append(as);
}

void
CallStatement::eliminateDuplicateArgs()
{
	LocationSet ls;
	for (auto it = arguments.begin(); it != arguments.end(); ) {
		Exp *lhs = ((Assignment *)*it)->getLeft();
		if (ls.exists(lhs)) {
			// This is a duplicate
			it = arguments.erase(it);
			continue;
		}
		ls.insert(lhs);
		++it;
	}
}

/**
 * \brief Generate a list of references for the parameters.
 */
void
PhiAssign::enumerateParams(std::list<Exp *> &le)
{
	for (const auto &def : defVec) {
		if (!def.e) continue;
		auto r = new RefExp(def.e, def.def);
		le.push_back(r);
	}
}

/**
 * \brief Map registers and temporaries to local variables.
 */
void
Statement::mapRegistersToLocals()
{
	ExpRegMapper erm(proc);
	StmtRegMapper srm(erm);
	accept(srm);
}

/**
 * Insert casts where needed, since fromSSA will erase type information.
 */
void
Statement::insertCasts()
{
	// First we postvisit expressions using a StmtModifier and an ExpCastInserter
	ExpCastInserter eci(proc);
	StmtModifier sm(eci, true);  // True to ignore collectors
	accept(sm);
	// Now handle the LHS of assigns that happen to be m[...], using a StmtCastInserter
	StmtCastInserter sci;
	accept(sci);
}

/**
 * The last part of the fromSSA logic:  Replace subscripted locations with
 * suitable local variables.
 */
void
Statement::replaceSubscriptsWithLocals()
{
	ExpSsaXformer esx(proc);
	StmtSsaXformer ssx(esx, proc);
	accept(ssx);
}

/**
 * \brief Map expressions to locals.
 */
void
Statement::dfaMapLocals()
{
	DfaLocalMapper dlm(proc);
	StmtModifier sm(dlm, true);  // True to ignore def collector in return statement
	accept(sm);
	if (VERBOSE && dlm.change)
		LOG << "statement mapped with new local(s): " << number << "\n";
}

/**
 * \fn void Statement::setNumber(int num)
 * Overridden for calls (and maybe later returns)
 */
void
CallStatement::setNumber(int num)
{
	number = num;
	// Also number any existing arguments. Important for library procedures, since these have arguments set by the front
	// end based in their signature
	for (const auto &arg : arguments)
		arg->setNumber(num);
}
