/**
 * \file
 * \brief Implementation of the classes that describe a low-level RTL
 *        (register transfer list).
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2000-2001, Sun Microsystems, Inc
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

#include "rtl.h"

#include "boomerang.h"
#include "exp.h"
#include "log.h"
#include "statement.h"
#include "type.h"
#include "types.h"
#include "visitor.h"

#include <iomanip>      // For std::setw, std::setfill
#include <sstream>

#include <cassert>

RTL::RTL()
{
}

/**
 * \param instNativeAddr  The native address of the instruction.
 * \param listExp         Ptr to existing list of Exps.
 */
RTL::RTL(ADDRESS instNativeAddr, std::list<Statement *> *listStmt /*= nullptr*/) :
	nativeAddr(instNativeAddr)
{
	if (listStmt)
		stmtList = *listStmt;
}

/**
 * \brief Copy constructor.  Makes deep copy of "other".
 *
 * A deep clone is made of the given object so that the lists of Exps do not
 * share memory.
 *
 * \param other  RTL to copy from.
 */
RTL::RTL(const RTL &other) :
	nativeAddr(other.nativeAddr)
{
	for (auto it = other.stmtList.cbegin(); it != other.stmtList.cend(); ++it) {
		stmtList.push_back((*it)->clone());
	}
}

RTL::~RTL()
{
}

/**
 * \brief Assignment copy.  Set this RTL to a deep copy of "other".
 *
 * \param other  RTL to copy.
 *
 * \returns A reference to this object.
 */
RTL &
RTL::operator =(RTL &other)
{
	if (this != &other) {
		// Do a deep copy always
		for (auto it = other.stmtList.begin(); it != other.stmtList.end(); ++it)
			stmtList.push_back((*it)->clone());

		nativeAddr = other.nativeAddr;
	}
	return *this;
}

/**
 * \brief Return a deep copy, including a deep copy of the list of Statements.
 *
 * Deep copy clone; deleting the clone will not affect this RTL object.
 *
 * \returns Pointer to a new RTL that is a clone of this one.
 */
RTL *
RTL::clone() const
{
	std::list<Statement *> le;
	deepCopyList(le);
	return new RTL(nativeAddr, &le);
}

/**
 * \brief Accept a visitor to this RTL.
 *
 * Visit this RTL, and all its Statements.
 */
bool
RTL::accept(StmtVisitor *visitor)
{
	// Might want to do something at the RTL level:
	if (!visitor->visit(this)) return false;
	for (auto it = stmtList.begin(); it != stmtList.end(); ++it) {
		if (!(*it)->accept(visitor)) return false;
	}
	return true;
}

/**
 * \brief Make a deep copy of the list of Exp*
 *
 * Make a copy of this RTL's list of Exp* to the given list.
 *
 * \param dest  Ref to empty list to copy to.
 */
void
RTL::deepCopyList(std::list<Statement *> &dest) const
{
	for (auto it = stmtList.begin(); it != stmtList.end(); ++it) {
		dest.push_back((*it)->clone());
	}
}

/**
 * \brief Add s to end of RTL.
 *
 * Append the given Statement at the end of this RTL.
 *
 * \note Exception: Leaves any flag call at the end (so may push exp to second
 * last position, instead of last).
 *
 * \note stmt is NOT copied. This is different to how UQBT was!
 *
 * \param s  Pointer to Statement to append.
 */
void
RTL::appendStmt(Statement *s)
{
	if (!stmtList.empty()) {
		if (stmtList.back()->isFlagAssgn()) {
			auto it = stmtList.end();
			stmtList.insert(--it, s);
			return;
		}
	}
	stmtList.push_back(s);
}

/**
 * \brief Add s to start of RTL.
 *
 * Prepend the given Statement at the start of this RTL.
 *
 * \note No clone of the statement is made. This is different to how UQBT was.
 *
 * \param s  Ptr to Statement to prepend.
 */
void
RTL::prependStmt(Statement *s)
{
	stmtList.push_front(s);
}

/**
 * \brief Append list of exps to end.
 *
 * Append a given list of Statements to this RTL.
 *
 * \note A copy of the Statements in le are appended.
 *
 * \param le  List of Exps to insert.
 */
void
RTL::appendListStmt(std::list<Statement *> &le)
{
	for (auto it = le.begin(); it != le.end(); ++it) {
		stmtList.insert(stmtList.end(), (*it)->clone());
	}
}

/**
 * \brief Append Statements from other RTL to end.
 *
 * Append the Statemens of another RTL to this object.
 *
 * \note A copy of the Statements in r are appended.
 *
 * \param r  Reference to RTL whose Exps we are to insert.
 */
void
RTL::appendRTL(RTL &r)
{
	appendListStmt(r.stmtList);
}

/**
 * \brief Insert s before expression at position i.
 *
 * Insert the given Statement before index i.
 *
 * \note No copy of stmt is made. This is different to UQBT.
 *
 * \param s  Pointer to the Statement to insert.
 * \param i  Position to insert before (0 = first).
 */
void
RTL::insertStmt(Statement *s, unsigned i)
{
	// Check that position i is not out of bounds
	assert(i < stmtList.size() || stmtList.empty());

	// Find the position
	auto pp = stmtList.begin();
	for (; i > 0; --i, ++pp);

	// Do the insertion
	stmtList.insert(pp, s);
}

/**
 * \brief Insert s before iterator it.
 */
void
RTL::insertStmt(Statement *s, iterator it)
{
	stmtList.insert(it, s);
}

/**
 * \brief Change stmt at position i.
 *
 * Replace the ith Statement with the given one.
 *
 * \param s  Pointer to the new Exp.
 * \param i  Index of Exp position (0 = first).
 */
void
RTL::updateStmt(Statement *s, unsigned i)
{
	// Check that position i is not out of bounds
	assert(i < stmtList.size());

	// Find the position
	auto pp = stmtList.begin();
	for (; i > 0; --i, ++pp);

	// Note that sometimes we might update even when we don't know if it's
	// needed, e.g. after a searchReplace.
	// In that case, don't update, and especially don't delete the existing
	// statement (because it's also the one we are updating!)
	if (*pp != s) {
		// Do the update
		;//delete *pp;
		*pp = s;
	}
}

/**
 * \brief Delete expression at position i.
 */
void
RTL::deleteStmt(unsigned i)
{
	// check that position i is not out of bounds
	assert(i < stmtList.size());

	auto pp = stmtList.begin();
	std::advance(pp, i);
	stmtList.erase(pp);
}

/**
 * \brief Delete the last statement.
 */
void
RTL::deleteLastStmt()
{
	assert(!stmtList.empty());
	stmtList.pop_back();
}

/**
 * \brief Replace the last Statement.
 */
void
RTL::replaceLastStmt(Statement *repl)
{
	assert(!stmtList.empty());
	Statement *&last = stmtList.back();
	last = repl;
}

/**
 * \brief Return the number of Stmts in RTL.
 *
 * Get the number of Statements in this RTL
 *
 * \returns Integer number of Statements.
 */
int
RTL::getNumStmt() const
{
	return stmtList.size();
}

/**
 * \brief Return the i'th element in RTL.
 *
 * Provides indexing on a list.  Changed from operator [] so that we keep in
 * mind it is linear in its execution time.
 *
 * \param i  The index of the element we want (0 = first).
 *
 * \returns The element at the given index
 *          or nullptr if the index is out of bounds.
 */
Statement *
RTL::elementAt(unsigned i) const
{
	auto it = stmtList.begin();
	for (; i > 0 && it != stmtList.end(); --i, ++it);
	if (it == stmtList.end()) {
		return nullptr;
	}
	return *it;
}

/**
 * \brief Print RTL to a stream.
 *
 * Prints this object to a stream in text form.
 *
 * \param os  Stream to output to (often cout or cerr).
 */
void
RTL::print(std::ostream &os /*= cout*/, bool html /*=false*/) const
{
	if (html)
		os << "<tr><td>";
	// print out the instruction address of this RTL
	os << std::hex << std::setfill('0') << std::setw(8) << nativeAddr;
	os << std::dec << std::setfill(' ');  // Ugh - why is this needed?
	if (html)
		os << "</td>";

	// Print the statements
	// First line has 8 extra chars as above
	bool bFirst = true;
	for (auto ss = stmtList.begin(); ss != stmtList.end(); ++ss) {
		Statement *stmt = *ss;
		if (html) {
			if (!bFirst) os << "<tr><td></td>";
			os << "<td width=\"50\" align=\"center\">";
		} else {
			if (bFirst) os << " ";
			else        os << std::setw(9) << " ";
		}
		if (stmt) stmt->print(os, html);
		// Note: we only put newlines where needed. So none at the end of
		// Statement::print; one here to separate from other statements
		if (html)
			os << "</td></tr>";
		os << "\n";
		bFirst = false;
	}
	if (stmtList.empty()) os << std::endl;  // New line for NOP
}

/**
 * \brief Print to a string (mainly for debugging).
 */
std::string
RTL::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

/**
 * \brief Output operator for RTL*
 *
 * Just makes it easier to use e.g. std::cerr << myRTLptr.
 *
 * \param os  Output stream to send to.
 * \param p   Ptr to RTL to print to the stream.
 *
 * \returns Copy of os (for concatenation).
 */
std::ostream &
operator <<(std::ostream &os, RTL *r)
{
	if (!r) {
		os << "NULL ";
		return os;
	}
	r->print(os);
	return os;
}

/**
 * \brief Set the RTL's source address.
 *
 * Set the nativeAddr field.
 *
 * \param addr  Native address.
 */
void
RTL::updateAddress(ADDRESS addr)
{
	nativeAddr = addr;
}

/**
 * \brief Replace all instances of "search" with "replace".
 *
 * \param search   Ptr to an expression to search for.
 * \param replace  Ptr to the expression with which to replace it.
 */
bool
RTL::searchAndReplace(Exp *search, Exp *replace)
{
	bool ch = false;
	for (auto it = stmtList.begin(); it != stmtList.end(); ++it)
		ch |= (*it)->searchAndReplace(search, replace);
	return ch;
}

/**
 * \brief Find all instances of the search expression.
 *
 * Searches for all instances of "search" and adds them to "result" in reverse
 * nesting order.  The search is optionally type sensitive.
 *
 * \param search  A location to search for.
 * \param result  A list which will have any matching exprs appended to it.
 *
 * \returns true if there were any matches.
 */
bool
RTL::searchAll(Exp *search, std::list<Exp *> &result)
{
	bool found = false;
	for (auto it = stmtList.begin(); it != stmtList.end(); ++it) {
		Statement *e = *it;
		Exp *res;
		if (e->search(search, res)) {
			found = true;
			result.push_back(res);
		}
	}
	return found;
}

/**
 * \brief Clear the list of Exps.
 *
 * Remove all statements from this RTL.
 */
void
RTL::clear()
{
	stmtList.clear();
}

/**
 * \brief Insert an assignment into this RTL.
 *
 * Prepends or appends an assignment to the front or back of this RTL.
 *
 * \note Is this really used?  What about types?
 *
 * \pre Assumes that pLhs and pRhs are "new" Exp's that are not part of other
 * Exps.  (Otherwise, there will be problems when deleting this Exp.)
 *
 * \pre If size == -1, assumes there is already at least one assignment in
 * this RTL.
 *
 * \param pLhs  Ptr to Exp to place on LHS.
 * \param pRhs  Ptr to Exp to place on RHS.
 * \param prep  true if prepend (else append).
 * \param type  Type of the transfer, or nullptr.
 */
void
RTL::insertAssign(Exp *pLhs, Exp *pRhs, bool prep, Type *type /*= nullptr */)
{
	// Generate the assignment expression
	Assign *asgn = new Assign(type, pLhs, pRhs);
	if (prep)
		prependStmt(asgn);
	else
		appendStmt(asgn);
}

/**
 * \brief Insert an assignment into this RTL, after temps have been defined.
 *
 * Inserts an assignment at or near the top of this RTL, after any assignments
 * to temporaries.  If the last assignment is to a temp, the insertion is done
 * before that last assignment.
 *
 * \pre Assumes that pLhs and pRhs are "new" Exp's that are not part of other
 * Exps.  (Otherwise, there will be problems when deleting this Exp.)
 *
 * \pre If type == nullptr, assumes there is already at least one assignment
 * in this RTL (?)
 *
 * \note Hopefully this is only a temporary measure.
 *
 * \param pLhs  Ptr to Exp to place on LHS.
 * \param pRhs  Ptr to Exp to place on RHS.
 * \param type  Type of the transfer, or nullptr.
 */
void
RTL::insertAfterTemps(Exp *pLhs, Exp *pRhs, Type *type /* nullptr */)
{
	// First skip all assignments with temps on LHS
	auto it = stmtList.begin();
	for (; it != stmtList.end(); ++it) {
		Statement *s = *it;
		if (!s->isAssign())
			break;
		Exp *LHS = ((Assign *)s)->getLeft();
		if (LHS->isTemp())
			break;
	}

	// Now check if the next Stmt is an assignment
	if ((it == stmtList.end()) || !(*it)->isAssign()) {
		// There isn't an assignment following. Use the previous Exp to insert
		// before
		if (it != stmtList.begin())
			--it;
	}

	if (!type)
		type = getType();

	// Generate the assignment expression
	Assign *asgn = new Assign(type, pLhs, pRhs);

	// Insert before "it"
	stmtList.insert(it, asgn);
}

/**
 * \brief Return type of first Assign.
 *
 * Get the "type" for this RTL. Just gets the type of the first assignment
 * Exp.
 *
 * \note The type of the first assign may not be the type that you want!
 *
 * \returns A pointer to the type.
 */
Type *
RTL::getType() const
{
	for (auto it = stmtList.begin(); it != stmtList.end(); ++it) {
		Statement *e = *it;
		if (e->isAssign())
			return ((Assign *)e)->getType();
	}
	return new IntegerType();  // Default to 32 bit integer if no assignments
}

/**
 * \brief True if flags are affected.
 *
 * Return true if this RTL affects the condition codes.
 *
 * \note Assumes that if there is a flag call Exp, then it is the last.
 *
 * \returns Boolean as above.
 */
bool
RTL::areFlagsAffected() const
{
	if (stmtList.empty()) return false;
	Statement *last = stmtList.back();
	// If it is a flag call, then the CCs are affected
	return last->isFlagAssgn();
}

/**
 * \brief Code generation.
 */
void
RTL::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) const
{
	for (auto it = stmtList.begin(); it != stmtList.end(); ++it) {
		(*it)->generateCode(hll, pbb, indLevel);
	}
}

/**
 * \brief Simplify all the uses/defs in this RTL.
 */
void
RTL::simplify()
{
	for (auto it = stmtList.begin(); it != stmtList.end(); /*++it*/) {
		Statement *s = *it;
		s->simplify();
		if (s->isBranch()) {
			Exp *cond = ((BranchStatement *)s)->getCondExpr();
			if (cond && cond->isIntConst()) {
				if (((Const *)cond)->getInt() == 0) {
					if (VERBOSE)
						LOG << "removing branch with false condition at " << getAddress()  << " " << *it << "\n";
					it = stmtList.erase(it);
					continue;
				} else {
					if (VERBOSE)
						LOG << "replacing branch with true condition with goto at " << getAddress() << " " << *it << "\n";
					*it = new GotoStatement(((BranchStatement *)s)->getFixedDest());
				}
			}
		} else if (s->isAssign()) {
			Exp *guard = ((Assign *)s)->getGuard();
			if (guard && (guard->isFalse() || (guard->isIntConst() && ((Const *)guard)->getInt() == 0))) {
				// This assignment statement can be deleted
				if (VERBOSE)
					LOG << "removing assignment with false guard at " << getAddress() << " " << *it << "\n";
				it = stmtList.erase(it);
				continue;
			}
		}
		++it;
	}
}

/**
 * \brief Is this RTL a compare instruction?  If so, the passed register and
 * compared value (a semantic string) are set.
 *
 * Return true if this is an unmodified compare instruction of a register with
 * an operand.
 *
 * \note Will also match a subtract if the flags are set.
 * \note expOperand, if set, is not cloned.
 * \note Assumes that the first subtract on the RHS is the actual compare
 * operation.
 *
 * \param iReg        Ref to integer to set with the register index.
 * \param expOperand  Ref to ptr to expression of operand.
 *
 * \returns true if found.
 */
bool
RTL::isCompare(int &iReg, Exp *&expOperand) const
{
	// Expect to see a subtract, then a setting of the flags
	// Dest of subtract should be a register (could be the always zero register)
	if (getNumStmt() < 2) return false;
	// Could be first some assignments to temporaries
	// But the actual compare could also be an assignment to a temporary
	// So we search for the first RHS with an opMinus, that has a LHS to
	// a register (whether a temporary or a machine register)
	int i = 0;
	Exp *rhs;
	Statement *cur;
	do {
		cur = elementAt(i);
		if (cur->getKind() != STMT_ASSIGN) return false;
		rhs = ((Assign *)cur)->getRight();
		++i;
	} while (rhs->getOper() != opMinus && i < getNumStmt());
	if (rhs->getOper() != opMinus) return false;
	// We have a subtract assigning to a register.
	// Check if there is a subflags last
	Statement *last = elementAt(getNumStmt() - 1);
	if (!last->isFlagAssgn()) return false;
	Exp *sub = ((Binary *)rhs)->getSubExp1();
	// Should be a compare of a register and something (probably a constant)
	if (!sub->isRegOf()) return false;
	// Set the register and operand expression, and return true
	iReg = ((Const *)((Unary *)sub)->getSubExp1())->getInt();
	expOperand = ((Binary *)rhs)->getSubExp2();
	return true;
}

/**
 * \brief True if this RTL ends in a GotoStatement.
 */
bool
RTL::isGoto() const
{
	if (stmtList.empty()) return false;
	Statement *last = stmtList.back();
	return last->getKind() == STMT_GOTO;
}

/**
 * \brief Is this RTL a branch instruction?
 */
bool
RTL::isBranch() const
{
	if (stmtList.empty()) return false;
	Statement *last = stmtList.back();
	return last->getKind() == STMT_BRANCH;
}

/**
 * \brief Is this RTL a call instruction?
 */
bool
RTL::isCall() const
{
	if (stmtList.empty()) return false;
	Statement *last = stmtList.back();
	return last->getKind() == STMT_CALL;
}

/**
 * \brief Get the "special" (High Level) Statement this RTL (else nullptr).
 *
 * Use this slow function when you can't be sure that the HL Statement is
 * last.
 */
Statement *
RTL::getHlStmt() const
{
	for (auto rit = stmtList.rbegin(); rit != stmtList.rend(); ++rit) {
		if ((*rit)->getKind() != STMT_ASSIGN)
			return *rit;
	}
	return nullptr;
}

/**
 * \brief Set or clear all the "constant subscripts" (conscripts) in this RTL.
 */
int
RTL::setConscripts(int n, bool bClear)
{
	StmtConscriptSetter ssc(n, bClear);
	accept(&ssc);
	return ssc.getLast();
}
