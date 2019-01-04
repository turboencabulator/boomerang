/**
 * \file
 * \brief Provides the implementation for the various visitor and modifier
 *        classes.
 *
 * \authors
 * Copyright (C) 2004-2006, Mike Van Emmerik and Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "visitor.h"

#include "boomerang.h"  // For EXPERIMENTAL
#include "exp.h"
#include "log.h"
#include "proc.h"
#include "prog.h"
#include "signature.h"
#include "statement.h"

// FixProcVisitor class

bool
FixProcVisitor::visit(Location *l, bool &recurse)
{
	l->setProc(proc);  // Set the proc, but only for Locations
	return true;
}

// GetProcVisitor class

bool
GetProcVisitor::visit(Location *l, bool &recurse)
{
	proc = l->getProc();
	return !proc;  // Continue recursion only if failed so far
}

// SetConscripts class

bool
SetConscripts::visit(Const *c)
{
	if (!bInLocalGlobal) {
		if (bClear)
			c->setConscript(0);
		else
			c->setConscript(++curConscript);
	}
	bInLocalGlobal = false;
	return true;  // Continue recursion
}

bool
SetConscripts::visit(Location *l, bool &recurse)
{
	OPER op = l->getOper();
	if (op == opLocal || op == opGlobal || op == opRegOf || op == opParam)
		bInLocalGlobal = true;
	return true;  // Continue recursion
}

bool
SetConscripts::visit(Binary *b, bool &recurse)
{
	OPER op = b->getOper();
	if (op == opSize)
		bInLocalGlobal = true;
	return true;  // Continue recursion
}


bool
StmtConscriptSetter::visit(Assign *stmt)
{
	SetConscripts sc(curConscript, bClear);
	stmt->getLeft()->accept(sc);
	stmt->getRight()->accept(sc);
	curConscript = sc.getLast();
	return true;
}
bool
StmtConscriptSetter::visit(PhiAssign *stmt)
{
	SetConscripts sc(curConscript, bClear);
	stmt->getLeft()->accept(sc);
	curConscript = sc.getLast();
	return true;
}
bool
StmtConscriptSetter::visit(ImplicitAssign *stmt)
{
	SetConscripts sc(curConscript, bClear);
	stmt->getLeft()->accept(sc);
	curConscript = sc.getLast();
	return true;
}

bool
StmtConscriptSetter::visit(CallStatement *stmt)
{
	SetConscripts sc(curConscript, bClear);
	StatementList &args = stmt->getArguments();
	for (const auto &arg : args)
		arg->accept(*this);
	curConscript = sc.getLast();
	return true;
}

bool
StmtConscriptSetter::visit(CaseStatement *stmt)
{
	SetConscripts sc(curConscript, bClear);
	if (auto si = stmt->getSwitchInfo()) {
		si->pSwitchVar->accept(sc);
		curConscript = sc.getLast();
	}
	return true;
}

bool
StmtConscriptSetter::visit(ReturnStatement *stmt)
{
	SetConscripts sc(curConscript, bClear);
	for (const auto &rr : *stmt)
		rr->accept(*this);
	curConscript = sc.getLast();
	return true;
}

bool
StmtConscriptSetter::visit(BoolAssign *stmt)
{
	SetConscripts sc(curConscript, bClear);
	stmt->getCondExpr()->accept(sc);
	stmt->getLeft()->accept(sc);
	curConscript = sc.getLast();
	return true;
}

bool
StmtConscriptSetter::visit(BranchStatement *stmt)
{
	SetConscripts sc(curConscript, bClear);
	stmt->getCondExpr()->accept(sc);
	curConscript = sc.getLast();
	return true;
}

bool
StmtConscriptSetter::visit(ImpRefStatement *stmt)
{
	SetConscripts sc(curConscript, bClear);
	stmt->getAddressExp()->accept(sc);
	curConscript = sc.getLast();
	return true;
}

void
PhiStripper::visit(PhiAssign *s, bool &recurse)
{
	del = true;
}

Exp *
CallBypasser::postVisit(RefExp *r)
{
	// If child was modified, simplify now
	Exp *ret = r;
	if (!(unchanged & mask)) ret = r->simplify();
	mask >>= 1;
	// Note: r (the pointer) will always == ret (also the pointer) here, so the below is safe and avoids a cast
	Statement *def = r->getDef();
	CallStatement *call = (CallStatement *)def;
	if (call && call->isCall()) {
		bool ch;
		ret = call->bypassRef((RefExp *)ret, ch);
		if (ch) {
			unchanged &= ~mask;
			mod = true;
			// Now have to recurse to do any further bypassing that may be required
			// E.g. bypass the two recursive calls in fibo?? FIXME: check!
			CallBypasser cb(enclosingStmt);
			return ret->accept(cb);
		}
	}

	// Else just leave as is (perhaps simplified)
	return ret;
}

Exp *
CallBypasser::postVisit(Location *e)
{
	// Hack to preserve a[m[x]]. Can likely go when ad hoc TA goes.
	bool isAddrOfMem = e->isAddrOf() && e->getSubExp1()->isMemOf();
	if (isAddrOfMem) return e;
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	return ret;
}

Exp *
SimpExpModifier::postVisit(Location *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	return ret;
}
Exp *
SimpExpModifier::postVisit(RefExp *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	return ret;
}
Exp *
SimpExpModifier::postVisit(Unary *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	return ret;
}
Exp *
SimpExpModifier::postVisit(Binary *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplifyArith()->simplify();
	mask >>= 1;
	return ret;
}
Exp *
SimpExpModifier::postVisit(Ternary *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	return ret;
}
Exp *
SimpExpModifier::postVisit(TypedExp *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	return ret;
}
Exp *
SimpExpModifier::postVisit(FlagDef *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	return ret;
}
Exp *
SimpExpModifier::postVisit(Const *e)
{
	mask >>= 1;
	return e;
}
Exp *
SimpExpModifier::postVisit(TypeVal *e)
{
	mask >>= 1;
	return e;
}
Exp *
SimpExpModifier::postVisit(Terminal *e)
{
	mask >>= 1;
	return e;
}

// Add used locations finder
bool
UsedLocsFinder::visit(Location *e, bool &recurse)
{
	if (!memOnly)
		used->insert(e);  // All locations visited are used
	if (e->isMemOf()) {
		// Example: m[r28{10} - 4]  we use r28{10}
		Exp *child = e->getSubExp1();
		// Care! Need to turn off the memOnly flag for work inside the m[...], otherwise everything will get ignored
		bool wasMemOnly = memOnly;
		memOnly = false;
		child->accept(*this);
		memOnly = wasMemOnly;
		recurse = false;  // Already looked inside child
	}
	return true;  // Continue looking for other locations
}

bool
UsedLocsFinder::visit(Terminal *e)
{
	if (memOnly)
		return true;  // Only interested in m[...]
	switch (e->getOper()) {
	case opPC:
	case opFlags:
	case opFflags:
	case opDefineAll:
	// Fall through
	// The carry flag can be used in some SPARC idioms, etc
	case opDF: case opCF: case opZF: case opNF: case opOF:  // also these
		used->insert(e);
	default:
		break;
	}
	return true;  // Always continue recursion
}

bool
UsedLocsFinder::visit(RefExp *e, bool &recurse)
{
	if (memOnly)
		return true;       // Don't count this reference
	used->insert(e);  // This location is used
	// However, e's subexpression is NOT used ...
	recurse = false;
	// ... unless that is a m[x], array[x] or .x, in which case x (not m[x]/array[x]/refd.x) is used
	Exp *refd = e->getSubExp1();
	if (refd->isMemOf()) {
		Exp *x = ((Location *)refd)->getSubExp1();
		x->accept(*this);
	} else if (refd->isArrayIndex()) {
		Exp *x1 = ((Binary *)refd)->getSubExp1();
		x1->accept(*this);
		Exp *x2 = ((Binary *)refd)->getSubExp2();
		x2->accept(*this);
	} else if (refd->isMemberOf()) {
		Exp *x = ((Binary *)refd)->getSubExp1();
		x->accept(*this);
	}
	return true;
}

bool
UsedLocalFinder::visit(Location *e, bool &recurse)
{
#if 0
	const char *sym = proc->lookupSym(e);
	if (sym)
		recurse = false;  // Don't look inside this local or parameter
	if (proc->findLocal(e))
#else
	if (e->isLocal())
#endif
		used->insert(e);  // Found a local
	return true;  // Continue looking for other locations
}

bool
UsedLocalFinder::visit(TypedExp *e, bool &recurse)
{
	Type *ty = e->getType();
	// Assumption: (cast)exp where cast is of pointer type means that exp is the address of a local
	if (ty->resolvesToPointer()) {
		Exp *sub = e->getSubExp1();
		Location *mof = Location::memOf(sub);
		if (proc->findLocal(mof, ty)) {
			used->insert(mof);
			recurse = false;
		}
	}
	return true;
}

bool
UsedLocalFinder::visit(Terminal *e)
{
	if (e->getOper() == opDefineAll)
		all = true;
	if (proc->findFirstSymbol(e))
		used->insert(e);
	return true;  // Always continue recursion
}

bool
UsedLocsVisitor::visit(Assign *s, bool &recurse)
{
	Exp *lhs = s->getLeft();
	Exp *rhs = s->getRight();
	if (rhs) rhs->accept(ev);
	// Special logic for the LHS. Note: PPC can have r[tmp + 30] on LHS
	if (lhs->isMemOf() || lhs->isRegOf()) {
		Exp *child = ((Location *)lhs)->getSubExp1();  // m[xxx] uses xxx
		// Care! Don't want the memOnly flag when inside a m[...]. Otherwise, nothing will be found
		// Also beware that ev may be a UsedLocalFinder now
		if (auto ulf = dynamic_cast<UsedLocsFinder *>(&ev)) {
			bool wasMemOnly = ulf->isMemOnly();
			ulf->setMemOnly(false);
			child->accept(ev);
			ulf->setMemOnly(wasMemOnly);
		}
	} else if (lhs->isArrayIndex() || lhs->isMemberOf()) {
		Exp *subExp1 = ((Binary *)lhs)->getSubExp1();  // array(base, index) and member(base, offset)?? use base and index
		subExp1->accept(ev);
		Exp *subExp2 = ((Binary *)lhs)->getSubExp2();
		subExp2->accept(ev);
	} else if (lhs->getOper() == opAt) {  // foo@[lo:hi] uses foo, lo, and hi
		Exp *subExp1 = ((Ternary *)lhs)->getSubExp1();
		subExp1->accept(ev);
		Exp *subExp2 = ((Ternary *)lhs)->getSubExp2();
		subExp2->accept(ev);
		Exp *subExp3 = ((Ternary *)lhs)->getSubExp3();
		subExp3->accept(ev);
	}
	recurse = false;  // Don't do the usual accept logic
	return true;      // Continue the recursion
}
bool
UsedLocsVisitor::visit(PhiAssign *s, bool &recurse)
{
	Exp *lhs = s->getLeft();
	// Special logic for the LHS
	if (lhs->isMemOf()) {
		Exp *child = ((Location *)lhs)->getSubExp1();
		if (auto ulf = dynamic_cast<UsedLocsFinder *>(&ev)) {
			bool wasMemOnly = ulf->isMemOnly();
			ulf->setMemOnly(false);
			child->accept(ev);
			ulf->setMemOnly(wasMemOnly);
		}
	} else if (lhs->isArrayIndex() || lhs->isMemberOf()) {
		Exp *subExp1 = ((Binary *)lhs)->getSubExp1();
		subExp1->accept(ev);
		Exp *subExp2 = ((Binary *)lhs)->getSubExp2();
		subExp2->accept(ev);
	}
	for (const auto &uu : *s) {
		// Note: don't make the RefExp based on lhs, since it is possible that the lhs was renamed in fromSSA()
		// Use the actual expression in the PhiAssign
		// Also note that it's possible for uu.e to be nullptr. Suppose variable a can be assigned to along in-edges
		// 0, 1, and 3; inserting the phi parameter at index 3 will cause a null entry at 2
		if (uu.e) {
			auto temp = new RefExp(uu.e, uu.def);
			temp->accept(ev);
		}
	}

	recurse = false;  // Don't do the usual accept logic
	return true;      // Continue the recursion
}
bool
UsedLocsVisitor::visit(ImplicitAssign *s, bool &recurse)
{
	Exp *lhs = s->getLeft();
	// Special logic for the LHS
	if (lhs->isMemOf()) {
		Exp *child = ((Location *)lhs)->getSubExp1();
		if (auto ulf = dynamic_cast<UsedLocsFinder *>(&ev)) {
			bool wasMemOnly = ulf->isMemOnly();
			ulf->setMemOnly(false);
			child->accept(ev);
			ulf->setMemOnly(wasMemOnly);
		}
	} else if (lhs->isArrayIndex() || lhs->isMemberOf()) {
		Exp *subExp1 = ((Binary *)lhs)->getSubExp1();
		subExp1->accept(ev);
		Exp *subExp2 = ((Binary *)lhs)->getSubExp2();
		subExp2->accept(ev);
	}
	recurse = false;  // Don't do the usual accept logic
	return true;      // Continue the recursion
}

bool
UsedLocsVisitor::visit(CallStatement *s, bool &recurse)
{
	if (auto pDest = s->getDest())
		pDest->accept(ev);
	StatementList &arguments = s->getArguments();
	for (const auto &arg : arguments) {
		// Don't want to ever collect anything from the lhs
		((Assign *)arg)->getRight()->accept(ev);
	}
	if (countCol) {
		DefCollector *col = s->getDefCollector();
		for (const auto &dd : *col)
			dd->accept(*this);
	}
	recurse = false;  // Don't do the normal accept logic
	return true;      // Continue the recursion
}

bool
UsedLocsVisitor::visit(ReturnStatement *s, bool &recurse)
{
	// For the final pass, only consider the first return
	for (const auto &rr : *s)
		rr->accept(*this);
	// Also consider the reaching definitions to be uses, so when they are the only non-empty component of this
	// ReturnStatement, they can get propagated to.
	if (countCol) {  // But we need to ignore these "uses" unless propagating
		DefCollector *col = s->getCollector();
		for (const auto &dd : *col)
			dd->accept(*this);
	}

	// Insert a phantom use of "everything" here, so that we can find out if any childless calls define something that
	// may end up being returned
	// FIXME: Not here! Causes locals to never get removed. Find out where this belongs, if anywhere:
	//((UsedLocsFinder *)ev)->getLocSet()->insert(new Terminal(opDefineAll));

	recurse = false;  // Don't do the normal accept logic
	return true;      // Continue the recursion
}

bool
UsedLocsVisitor::visit(BoolAssign *s, bool &recurse)
{
	if (auto pCond = s->getCondExpr())
		pCond->accept(ev);  // Condition is used
	Exp *lhs = s->getLeft();
	if (lhs && lhs->isMemOf()) {  // If dest is of form m[x]...
		Exp *x = ((Location *)lhs)->getSubExp1();
		if (auto ulf = dynamic_cast<UsedLocsFinder *>(&ev)) {
			bool wasMemOnly = ulf->isMemOnly();
			ulf->setMemOnly(false);
			x->accept(ev);
			ulf->setMemOnly(wasMemOnly);
		}
	} else if (lhs->isArrayIndex() || lhs->isMemberOf()) {
		Exp *subExp1 = ((Binary *)lhs)->getSubExp1();
		subExp1->accept(ev);
		Exp *subExp2 = ((Binary *)lhs)->getSubExp2();
		subExp2->accept(ev);
	}
	recurse = false;  // Don't do the normal accept logic
	return true;      // Continue the recursion
}

//
// Expression subscripter
//
Exp *
ExpSubscripter::preVisit(Location *e, bool &recurse)
{
	if (*e == *search) {
		recurse = e->isMemOf();     // Don't double subscript unless m[...]
		return new RefExp(e, def);  // Was replaced by postVisit below
	}
	return e;
}

Exp *
ExpSubscripter::preVisit(Binary *e, bool &recurse)
{
	// array[index] is like m[addrexp]: requires a subscript
	if (e->isArrayIndex() && *e == *search)
		return new RefExp(e, def);  // Was replaced by postVisit below
	return e;
}

Exp *
ExpSubscripter::preVisit(Terminal *e)
{
	if (*e == *search)
		return new RefExp(e, def);
	return e;
}

Exp *
ExpSubscripter::preVisit(RefExp *e, bool &recurse)
{
	recurse = false;  // Don't look inside... not sure about this
	return e;
}

// The Statement subscripter class
void
StmtSubscripter::visit(Assign *s, bool &recurse)
{
	Exp *rhs = s->getRight();
	s->setRight(rhs->accept(mod));
	// Don't subscript the LHS of an assign, ever
	Exp *lhs = s->getLeft();
	if (lhs->isMemOf() || lhs->isRegOf()) {
		((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(mod));
	}
	recurse = false;
}
void
StmtSubscripter::visit(PhiAssign *s, bool &recurse)
{
	Exp *lhs = s->getLeft();
	if (lhs->isMemOf()) {
		((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(mod));
	}
	recurse = false;
}
void
StmtSubscripter::visit(ImplicitAssign *s, bool &recurse)
{
	Exp *lhs = s->getLeft();
	if (lhs->isMemOf()) {
		((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(mod));
	}
	recurse = false;
}
void
StmtSubscripter::visit(BoolAssign *s, bool &recurse)
{
	Exp *lhs = s->getLeft();
	if (lhs->isMemOf()) {
		((Location *)lhs)->setSubExp1(((Location *)lhs)->getSubExp1()->accept(mod));
	}
	Exp *rhs = s->getCondExpr();
	s->setCondExpr(rhs->accept(mod));
	recurse = false;
}
void
StmtSubscripter::visit(CallStatement *s, bool &recurse)
{
	if (auto pDest = s->getDest())
		s->setDest(pDest->accept(mod));
	// Subscript the ordinary arguments
	StatementList &arguments = s->getArguments();
	for (const auto &arg : arguments)
		arg->accept(*this);
	// Returns are like the LHS of an assignment; don't subscript them directly (only if m[x], and then only subscript
	// the x's)
	recurse = false;  // Don't do the usual accept logic
}


// Size stripper
Exp *
SizeStripper::preVisit(Binary *b, bool &recurse)
{
	if (b->isSizeCast())
		// Could be a size cast of a size cast
		return b->getSubExp2()->stripSizes();
	return b;
}

Exp *
ExpConstCaster::preVisit(Const *c)
{
	if (c->getConscript() == num) {
		changed = true;
		return new TypedExp(ty, c);
	}
	return c;
}


// This is the code (apart from definitions) to find all constants in a Statement
bool
ConstFinder::visit(Const *e)
{
	lc.push_back(e);
	return true;
}
bool
ConstFinder::visit(Location *e, bool &recurse)
{
	if (!e->isMemOf())  // We DO want to see constants in memofs
		recurse = false;  // Don't consider register numbers, global names, etc
	return true;
}

// This is in the POST visit function, because it's important to process any child expressions first.
// Otherwise, for m[r28{0} - 12]{0}, you could be adding an implicit assignment with a nullptr definition for r28.
Exp *
ImplicitConverter::postVisit(RefExp *e)
{
	if (!e->getDef())
		e->setDef(cfg->findImplicitAssign(e->getSubExp1()));
	return e;
}


void
StmtImplicitConverter::visit(PhiAssign *s, bool &recurse)
{
	// The LHS could be a m[x] where x has a null subscript; must do first
	s->setLeft(s->getLeft()->accept(mod));
	for (auto &uu : *s) {
		if (!uu.e) continue;
		if (!uu.def)
			uu.def = cfg->findImplicitAssign(uu.e);
	}
	recurse = false;  // Already done LHS
}


// Localiser. Subscript a location with the definitions that reach the call, or with {-} if none
Exp *
Localiser::preVisit(RefExp *e, bool &recurse)
{
	recurse = false;  // Don't recurse into already subscripted variables
	mask <<= 1;
	return e;
}

Exp *
Localiser::postVisit(Location *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	if (auto r = call->findDefFor(ret)) {
		ret = r->clone();
		if (0 && EXPERIMENTAL) {  // FIXME: check if sometimes needed
			// The trouble with the below is that you can propagate to say a call statement's argument expression and
			// not to the assignment of the actual argument. Examples: test/pentium/fromssa2, fbranch
			bool ch;
			ret = ret->propagateAllRpt(ch);  // Propagate into this repeatedly, in case propagation is limited
		}
		ret = ret->bypass();
		unchanged &= ~mask;
		mod = true;
	} else
		ret = new RefExp(ret, nullptr);  // No definition reaches, so subscript with {-}
	return ret;
}

// Want to be able to localise a few terminals, in particular <all>
Exp *
Localiser::postVisit(Terminal *e)
{
	Exp *ret = e;
	if (!(unchanged & mask)) ret = e->simplify();
	mask >>= 1;
	if (auto r = call->findDefFor(ret)) {
		ret = r->clone()->bypass();
		unchanged &= ~mask;
		mod = true;
	} else
		ret = new RefExp(ret, nullptr);  // No definition reaches, so subscript with {-}
	return ret;
}

bool
ComplexityFinder::visit(Location *e, bool &recurse)
{
	if (proc && proc->findFirstSymbol(e)) {
		// This is mapped to a local. Count it as zero, not about 3 (m[r28+4] -> memof, regof, plus)
		recurse = false;
		return true;
	}
	if (e->isMemOf() || e->isArrayIndex())
		++count;  // Count the more complex unaries
	return true;
}
bool ComplexityFinder::visit(Unary *e,   bool &recurse) { ++count; return true; }
bool ComplexityFinder::visit(Binary *e,  bool &recurse) { ++count; return true; }
bool ComplexityFinder::visit(Ternary *e, bool &recurse) { ++count; return true; }

bool
MemDepthFinder::visit(Location *e, bool &recurse)
{
	if (e->isMemOf())
		++depth;
	return true;
}

// Ugh! This is still a separate propagation mechanism from Statement::propagateTo().
Exp *
ExpPropagator::postVisit(RefExp *e)
{
	// No need to call e->canRename() here, because if e's base expression is not suitable for renaming, it will never
	// have been renamed, and we never would get here
	if (!Statement::canPropagateToExp(e))  // Check of the definition statement is suitable for propagating
		return e;
	Statement *def = e->getDef();
	Exp *res = e;
	if (def && def->isAssign()) {
		Exp *lhs = ((Assign *)def)->getLeft();
		Exp *rhs = ((Assign *)def)->getRight();
		bool ch;
		res = e->searchReplaceAll(new RefExp(lhs, def), rhs->clone(), ch);
		if (ch) {
			change = true;       // Record this change
			unchanged &= ~mask;  // Been changed now (so simplify parent)
			if (res->isSubscript())
				res = postVisit((RefExp *)res);  // Recursively propagate more if possible
		}
	}
	return res;
}

// Return true if e is a primitive expression; basically, an expression you can propagate to without causing
// memory expression problems. See Mike's thesis for details
// Algorithm: if find any unsubscripted location, not primitive
//   Implicit definitions are primitive (but keep searching for non primitives)
//   References to the results of calls are considered primitive... but only if bypassed?
//   Other references considered non primitive
// Start with result=true, must find primitivity in all components
bool
PrimitiveTester::visit(Location *e, bool &recurse)
{
	// We reached a bare (unsubscripted) location. This is certainly not primitive
	recurse = false;
	result = false;
	return false;  // No need to continue searching
}

bool
PrimitiveTester::visit(RefExp *e, bool &recurse)
{
	Statement *def = e->getDef();
	// If defined by a call, e had better not be a memory location (crude approximation for now)
	if (!def || def->getNumber() == 0 || (def->isCall() && !e->getSubExp1()->isMemOf())) {
		// Implicit definitions are always primitive
		// The results of calls are always primitive
		recurse = false;  // Don't recurse into the reference
		return true;      // Result remains true
	}

	// For now, all references to other definitions will be considered non primitive. I think I'll have to extend this!
	result = false;
	recurse = false;  // Regareless of outcome, don't recurse into the reference
	return true;
}

bool
ExpHasMemofTester::visit(Location *e, bool &recurse)
{
	if (e->isMemOf()) {
		recurse = false;  // Don't recurse children (not needed surely)
		result = true;    // Found a memof
		return false;     // Don't continue searching the expression
	}
	return true;
}

bool
TempToLocalMapper::visit(Location *e, bool &recurse)
{
	if (e->isTemp()) {
		// We have a temp subexpression; get its name
		const char *tempName = ((Const *)e->getSubExp1())->getStr();
		Type *ty = Type::getTempType(tempName);  // Types for temps strictly depend on the name
		// This call will do the mapping from the temp to a new local:
		proc->getSymbolExp(e, ty, true);
	}
	recurse = false;  // No need to examine the string
	return true;
}

ExpRegMapper::ExpRegMapper(UserProc *p) :
	proc(p)
{
	prog = proc->getProg();
}

// The idea here is to map the default of a register to a symbol with the type of that first use. If the register is
// not involved in any conflicts, it will use this name by default
bool
ExpRegMapper::visit(RefExp *e, bool &recurse)
{
	Exp *base = e->getSubExp1();
	if (base->isRegOf() || proc->isLocalOrParamPattern(base))  // Don't convert if e.g. a global
		proc->checkLocalFor(e);
	recurse = false;  // Don't examine the r[] inside
	return true;
}

bool
StmtRegMapper::common(Assignment *stmt, bool &recurse)
{
	// In case lhs is a reg or m[reg] such that reg is otherwise unused
	Exp *lhs = stmt->getLeft();
	auto re = new RefExp(lhs, stmt);
	re->accept((ExpRegMapper &)ev);
	return true;
}

bool StmtRegMapper::visit(        Assign *stmt, bool &recurse) { return common(stmt, recurse); }
bool StmtRegMapper::visit(     PhiAssign *stmt, bool &recurse) { return common(stmt, recurse); }
bool StmtRegMapper::visit(ImplicitAssign *stmt, bool &recurse) { return common(stmt, recurse); }
bool StmtRegMapper::visit(    BoolAssign *stmt, bool &recurse) { return common(stmt, recurse); }

// Constant global converter. Example: m[m[r24{16} + m[0x8048d60]{-}]{-}]{-} -> m[m[r24{16} + 32]{-}]{-}
// Allows some complex variations to be matched to standard indirect call forms
Exp *
ConstGlobalConverter::preVisit(RefExp *e, bool &recurse)
{
	Exp *base, *addr, *idx, *glo;
	if (e->isImplicitDef()) {
		if ((base = e->getSubExp1(), base->isMemOf())
		 && (addr = ((Location *)base)->getSubExp1(), addr->isIntConst())) {
			// We have a m[K]{-}
			int K = ((Const *)addr)->getInt();
			int value = prog->readNative4(K);
			recurse = false;
			return new Const(value);
		} else if (base->isGlobal()) {
			// We have a glo{-}
			const char *gname = ((Const *)(base->getSubExp1()))->getStr();
			ADDRESS gloValue = prog->getGlobalAddr(gname);
			int value = prog->readNative4(gloValue);
			recurse = false;
			return new Const(value);
		} else if (base->isArrayIndex()
		        && (idx = ((Binary *)base)->getSubExp2(), idx->isIntConst())
		        && (glo = ((Binary *)base)->getSubExp1(), glo->isGlobal())) {
			// We have a glo[K]{-}
			int K = ((Const *)idx)->getInt();
			const char *gname = ((Const *)(glo->getSubExp1()))->getStr();
			ADDRESS gloValue = prog->getGlobalAddr(gname);
			Type *gloType = prog->getGlobal(gname)->getType();
			assert(gloType->isArray());
			Type *componentType = gloType->asArray()->getBaseType();
			int value = prog->readNative4(gloValue + K * (componentType->getSize() / 8));
			recurse = false;
			return new Const(value);
		}
	}
	return e;
}

bool
ExpDestCounter::visit(RefExp *e, bool &recurse)
{
	if (Statement::canPropagateToExp(e))
		++destCounts[e];
	return true;       // Continue visiting the rest of Exp *e
}

bool
FlagsFinder::visit(Binary *e, bool &recurse)
{
	if (e->isFlagCall()) {
		found = true;
		return false;  // Don't continue searching
	}
	return true;
}

// Search for bare memofs (not subscripted) in the expression
bool
BadMemofFinder::visit(Location *e, bool &recurse)
{
	if (e->isMemOf()) {
		found = true;  // A bare memof
		return false;
	}
	return true;  // Continue searching
}

bool
BadMemofFinder::visit(RefExp *e, bool &recurse)
{
	Exp *base = e->getSubExp1();
	if (base->isMemOf()) {
		// Beware: it may be possible to have a bad memof inside a subscripted one
		Exp *addr = ((Location *)base)->getSubExp1();
		addr->accept(*this);
		if (found)
			return false;  // Don't continue searching
#if NEW  // FIXME: not ready for this until have incremental propagation
		const char *sym = proc->lookupSym(e);
		if (!sym) {
			found = true;     // Found a memof that is not a symbol
			recurse = false;  // Don't look inside the refexp
			return false;
		}
#endif
	}
	recurse = false;  // Don't look inside the refexp
	return true;      // It has a symbol; noting bad foound yet but continue searching
}

// CastInserters. More cases to be implemented.

// Check the type of the address expression of memof to make sure it is compatible with the given memofType.
// memof may be changed internally to include a TypedExp, which will emit as a cast
void
ExpCastInserter::checkMemofType(Exp *memof, Type *memofType)
{
	Exp *addr = ((Unary *)memof)->getSubExp1();
	if (addr->isSubscript()) {
		Exp *addrBase = ((RefExp *)addr)->getSubExp1();
		Type *actType = ((RefExp *)addr)->getDef()->getTypeFor(addrBase);
		Type *expectedType = new PointerType(memofType);
		if (!actType->isCompatibleWith(expectedType)) {
			((Unary *)memof)->setSubExp1(new TypedExp(expectedType, addrBase));
		}
	}
}

Exp *
ExpCastInserter::postVisit(RefExp *e)
{
	Exp *base = e->getSubExp1();
	if (base->isMemOf()) {
		// Check to see if the address expression needs type annotation
		Statement *def = e->getDef();
		Type *memofType = def->getTypeFor(base);
		checkMemofType(base, memofType);
	}
	return e;
}

static Exp *
checkSignedness(Exp *e, int reqSignedness)
{
	Type *ty = e->ascendType();
	int currSignedness = 0;
	bool isInt = ty->resolvesToInteger();
	if (isInt) {
		currSignedness = ty->asInteger()->getSignedness();
		currSignedness = (currSignedness >= 0) ? 1 : -1;
	}
	//if (!isInt || currSignedness != reqSignedness) { // }
	// Don't want to cast e.g. floats to integer
	if (isInt && currSignedness != reqSignedness) {
		IntegerType *newtype;
		if (!isInt)
			newtype = new IntegerType(STD_SIZE, reqSignedness);
		else
			newtype = new IntegerType(((IntegerType *)ty)->getSize(), reqSignedness);  // Transfer size
		newtype->setSigned(reqSignedness);
		return new TypedExp(newtype, e);
	}
	return e;
}

Exp *
ExpCastInserter::postVisit(Binary *e)
{
	OPER op = e->getOper();
	switch (op) {
	// This case needed for e.g. test/pentium/switch_gcc:
	case opLessUns:
	case opGtrUns:
	case opLessEqUns:
	case opGtrEqUns:
	case opShiftR:
		e->setSubExp1(checkSignedness(e->getSubExp1(), -1));
		if (op != opShiftR)  // The shift amount (second operand) is sign agnostic
			e->setSubExp2(checkSignedness(e->getSubExp2(), -1));
		break;
	// This case needed for e.g. test/sparc/minmax2, if %g1 is declared as unsigned int
	case opLess:
	case opGtr:
	case opLessEq:
	case opGtrEq:
	case opShiftRA:
		e->setSubExp1(checkSignedness(e->getSubExp1(), +1));
		if (op != opShiftRA)
			e->setSubExp2(checkSignedness(e->getSubExp2(), +1));
		break;
	default:
		break;
	}
	return e;
}

Exp *
ExpCastInserter::postVisit(Const *e)
{
	if (e->isIntConst()) {
		bool naturallySigned = e->getInt() < 0;
		Type *ty = e->getType();
		if (naturallySigned && ty->isInteger() && !ty->asInteger()->isSigned()) {
			return new TypedExp(new IntegerType(ty->asInteger()->getSize(), -1), e);
		}
	}
	return e;
}

bool StmtCastInserter::visit(        Assign *s) { return common(s); }
bool StmtCastInserter::visit(     PhiAssign *s) { return common(s); }
bool StmtCastInserter::visit(ImplicitAssign *s) { return common(s); }
bool StmtCastInserter::visit(    BoolAssign *s) { return common(s); }
bool
StmtCastInserter::common(Assignment *s)
{
	Exp *lhs = s->getLeft();
	if (lhs->isMemOf()) {
		Type *memofType = s->getType();
		ExpCastInserter::checkMemofType(lhs, memofType);
	}
	return true;
}

Exp *
ExpSsaXformer::postVisit(RefExp *e)
{
	if (auto sym = proc->lookupSymFromRefAny(e))
		return Location::local(sym, proc);
	// We should not get here: all locations should be replaced with Locals or Parameters
	//LOG << "ERROR! Could not find local or parameter for " << e << " !!\n";
	return e->getSubExp1();  // At least strip off the subscript
}

// Common code for the left hand side of assignments
void
StmtSsaXformer::commonLhs(Assignment *as)
{
	Exp *lhs = as->getLeft();
	lhs = lhs->accept((ExpSsaXformer &)mod);  // In case the LHS has say m[r28{0}+8] -> m[esp+8]
	auto re = new RefExp(lhs, as);
	if (auto sym = proc->lookupSymFromRefAny(re))
		as->setLeft(Location::local(sym, proc));
}

// FIXME:  None of these StmtSsaXformer methods specified whether it should
//         recurse or not.  Default is now to recurse.

void
StmtSsaXformer::visit(BoolAssign *s, bool &recurse)
{
	commonLhs(s);
	Exp *pCond = s->getCondExpr();
	pCond = pCond->accept((ExpSsaXformer &)mod);
	s->setCondExpr(pCond);
}

void
StmtSsaXformer::visit(Assign *s, bool &recurse)
{
	commonLhs(s);
	Exp *rhs = s->getRight();
	rhs = rhs->accept((ExpSsaXformer &)mod);
	s->setRight(rhs);
}

void
StmtSsaXformer::visit(ImplicitAssign *s, bool &recurse)
{
	commonLhs(s);
}

void
StmtSsaXformer::visit(PhiAssign *s, bool &recurse)
{
	commonLhs(s);
	UserProc *proc = ((ExpSsaXformer &)mod).getProc();
	for (auto &uu : *s) {
		if (!uu.e) continue;
		auto r = new RefExp(uu.e, uu.def);
		if (auto sym = proc->lookupSymFromRefAny(r))
			uu.e = Location::local(sym, proc);  // Some may be parameters, but hopefully it won't matter
	}
}

void
StmtSsaXformer::visit(CallStatement *s, bool &recurse)
{
	if (auto pDest = s->getDest()) {
		pDest = pDest->accept((ExpSsaXformer &)mod);
		s->setDest(pDest);
	}
	StatementList &arguments = s->getArguments();
	for (const auto &arg : arguments)
		arg->accept(*this);
	// Note that defines have statements (assignments) within a statement (this call). The fromSSA logic, which needs
	// to subscript definitions on the left with the statement pointer, won't work if we just call the assignment's
	// fromSSA() function
	StatementList &defines = s->getDefines();
	for (const auto &def : defines) {
		auto as = (Assignment *)def;
		// FIXME: use of fromSSAleft is deprecated
		Exp *e = as->getLeft()->fromSSAleft(((ExpSsaXformer &)mod).getProc(), s);
		// FIXME: this looks like a HACK that can go:
		auto procDest = s->getDestProc();
		if (procDest && dynamic_cast<LibProc *>(procDest) && e->isLocal()) {
			UserProc *proc = s->getProc();  // Enclosing proc
			const char *name = ((Const *)e->getSubExp1())->getStr();
			Type *lty = proc->getLocalType(name);
			Type *ty = as->getType();
			if (ty && lty && *ty != *lty) {
				LOG << "local " << *e << " has type " << lty->getCtype()
				    << " that doesn't agree with type of define " << ty->getCtype() << " of a library, why?\n";
				proc->setLocalType(name, ty);
			}
		}
		as->setLeft(e);
	}
	// Don't think we'll need this anyway:
	// defCol.fromSSAform(ig);

	// However, need modifications of the use collector; needed when say eax is renamed to local5, otherwise
	// local5 is removed from the results of the call
	s->useColFromSsaForm(s);
}

// Map expressions to locals, using the (so far DFA based) type analysis information
// Basically, descend types, and when you get to m[...] compare with the local high level pattern;
// when at a sum or difference, check for the address of locals high level pattern that is a pointer

// Map expressions to locals, some with names like param3
DfaLocalMapper::DfaLocalMapper(UserProc *proc) :
	proc(proc)
{
	sig = proc->getSignature();
	prog = proc->getProg();
}

// Common processing for the two main cases (visiting a Location or a Binary)
bool
DfaLocalMapper::processExp(Exp *e)
{
	if (proc->isLocalOrParamPattern(e)) {  // Check if this is an appropriate pattern for local variables
		if (sig->isStackLocal(prog, e)) {
			change = true;  // We've made a mapping
			// We have probably not even run TA yet, so doing a full descendtype here would be silly
			// Note also that void is compatible with all types, so the symbol effectively covers all types
			proc->getSymbolExp(e, new VoidType(), true);
#if 0
		} else {
			std::ostringstream ost;
			ost << "tparam" << proc->nextParamNum();
			proc->mapSymbolTo(e, Location::param(ost.str(), proc));
#endif
		}
		return false;  // set recurse false: Don't dig inside m[x] to make m[a[m[x]]] !
	}
	return true;
}

Exp *
DfaLocalMapper::preVisit(Location *e, bool &recurse)
{
	if (e->isMemOf() && !proc->findFirstSymbol(e)) {  // Need the 2nd test to ensure change set correctly
		recurse = processExp(e);
	}
	return e;
}

Exp *
DfaLocalMapper::preVisit(Binary *e, bool &recurse)
{
#if 1
	// Check for sp -/+ K
	Exp *memOf_e = Location::memOf(e);
	if (proc->findFirstSymbol(memOf_e)) {
		recurse = false;  // Already done; don't recurse
		return e;
	} else {
		recurse = processExp(memOf_e);  // Process m[this]
		if (!recurse)  // If made a change this visit,
			return new Unary(opAddrOf, memOf_e);  // change to a[m[this]]
	}
#endif
	return e;
}

Exp *
DfaLocalMapper::preVisit(TypedExp *e, bool &recurse)
{
	// Assume it's already been done correctly, so don't recurse into this
	recurse = false;
	return e;
}
