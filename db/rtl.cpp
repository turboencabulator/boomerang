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
#include "statement.h"
#include "types.h"
#include "visitor.h"

#include <iomanip>      // For std::setw
#include <sstream>

#include <cassert>

/**
 * \param addr  The native address of the instruction.
 */
RTL::RTL(ADDRESS addr) :
	nativeAddr(addr)
{
}

RTL::RTL(ADDRESS addr, Statement *stmt) :
	nativeAddr(addr)
{
	stmtList.push_back(stmt);
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
	for (const auto &stmt : other.stmtList)
		stmtList.push_back(stmt->clone());
}

/**
 * \brief Assignment copy.  Set this RTL to a deep copy of "other".
 *
 * \param other  RTL to copy.
 *
 * \returns A reference to this object.
 */
RTL &
RTL::operator =(const RTL &other)
{
	if (this != &other) {
		stmtList.clear();
		for (const auto &stmt : other.stmtList)
			stmtList.push_back(stmt->clone());

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
	return new RTL(*this);
}

/**
 * \brief Accept a visitor to this RTL.
 *
 * Visit this RTL, and all its Statements.
 */
bool
RTL::accept(StmtVisitor &v)
{
	// Might want to do something at the RTL level:
	if (!v.visit(this))
		return false;
	for (const auto &stmt : stmtList)
		if (!stmt->accept(v))
			return false;
	return true;
}

/**
 * \brief Add s to end of RTL.
 *
 * Append the given Statement to the end of this RTL.
 *
 * \note stmt is NOT copied. This is different to how UQBT was!
 *
 * \param s  Statement to append.
 */
void
RTL::appendStmt(Statement *s)
{
	stmtList.push_back(s);
}

/**
 * \brief Transfers Statements from a list to the end of this RTL.
 *
 * \param l  List of Statements to move.
 */
void
RTL::splice(std::list<Statement *> &l)
{
	stmtList.splice(stmtList.end(), l);
}

/**
 * \brief Transfers Statements from other RTL to the end of this RTL.
 *
 * \param r  RTL whose Statements we are to move.
 */
void
RTL::splice(RTL &r)
{
	splice(r.stmtList);
}

/**
 * \brief Insert s before position it.
 *
 * Insert the given Statement before iterator it.
 *
 * \note No copy of stmt is made. This is different to UQBT.
 *
 * \param it  Position to insert before.
 * \param s   Statement to insert.
 */
RTL::iterator
RTL::insertStmt(const_iterator it, Statement *s)
{
	return stmtList.insert(it, s);
}

/**
 * \brief Delete statement at position it.
 */
RTL::iterator
RTL::deleteStmt(const_iterator it)
{
	return stmtList.erase(it);
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
	auto fill = os.fill('0');
	os << std::hex << std::setw(8) << nativeAddr << std::dec;
	os.fill(fill);
	if (html)
		os << "</td>";

	// Print the statements
	// First line has 8 extra chars as above
	bool bFirst = true;
	for (const auto &stmt : stmtList) {
		if (html) {
			if (!bFirst) os << "<tr><td></td>";
		} else {
			if (bFirst) os << " ";
			else        os << std::setw(9) << " ";
		}
		if (stmt) stmt->print(os, html);
		// Note: we only put newlines where needed. So none at the end of
		// Statement::print; one here to separate from other statements
		if (html)
			os << "</tr>";
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
 * \param r   Ptr to RTL to print to the stream.
 *
 * \returns Copy of os (for concatenation).
 */
std::ostream &
operator <<(std::ostream &os, const RTL *r)
{
	if (!r)
		return os << "NULL ";
	return os << *r;
}
std::ostream &
operator <<(std::ostream &os, const RTL &r)
{
	r.print(os);
	return os;
}

/**
 * \brief Replace all instances of "search" with "replace".
 *
 * \param[in] search   Ptr to an expression to search for.
 * \param[in] replace  Ptr to the expression with which to replace it.
 */
bool
RTL::searchAndReplace(Exp *search, Exp *replace)
{
	bool change = false;
	for (const auto &stmt : stmtList)
		change |= stmt->searchAndReplace(search, replace);
	return change;
}

/**
 * \brief Find all instances of the search expression.
 *
 * Searches for all instances of "search" and adds them to "result" in reverse
 * nesting order.  The search is optionally type sensitive.
 *
 * \param[in] search   A location to search for.
 * \param[out] result  A list which will have any matching expressions
 *                     appended to it.
 *
 * \returns true if there were any matches.
 */
bool
RTL::searchAll(Exp *search, std::list<Exp *> &result)
{
	bool found = false;
	for (const auto &stmt : stmtList) {
		Exp *res;
		if (stmt->search(search, res)) {
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
 * \brief True if flags are affected.
 *
 * \returns true if this RTL affects the condition codes.
 */
bool
RTL::areFlagsAffected() const
{
	for (const auto &stmt : stmtList)
		// If it is a flag call, then the CCs are affected
		if (stmt->isFlagAssgn())
			return true;
	return false;
}

/**
 * \brief Code generation.
 */
void
RTL::generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) const
{
	for (const auto &stmt : stmtList) {
		stmt->generateCode(hll, pbb, indLevel);
	}
}

/**
 * \brief Simplify all the uses/defs in this RTL.
 */
void
RTL::simplify()
{
	for (auto it = stmtList.begin(); it != stmtList.end(); /*++it*/) {
		const auto &stmt = *it;
		stmt->simplify();
		if (auto branch = dynamic_cast<BranchStatement *>(stmt)) {
			if (auto cond = branch->getCondExpr()) {
				if (cond->isFalse()) {
					if (VERBOSE)
						LOG << "removing branch with false condition at 0x" << std::hex << getAddress() << std::dec << " " << *branch << "\n";
					it = stmtList.erase(it);
					continue;
				} else if (cond->isTrue()) {
					if (VERBOSE)
						LOG << "replacing branch with true condition with goto at 0x" << std::hex << getAddress() << std::dec << " " << *branch << "\n";
					*it = new GotoStatement(branch->getFixedDest());
				}
			}
		} else if (auto assign = dynamic_cast<Assign *>(stmt)) {
			if (auto guard = assign->getGuard()) {
				if (guard->isFalse()) {
					// This assignment statement can be deleted
					if (VERBOSE)
						LOG << "removing assignment with false guard at 0x" << std::hex << getAddress() << std::dec << " " << *assign << "\n";
					it = stmtList.erase(it);
					continue;
				}
			}
		}
		++it;
	}
}

/**
 * \brief True if this RTL ends in a GotoStatement.
 */
bool
RTL::isGoto() const
{
	if (stmtList.empty()) return false;
	return stmtList.back()->getKind() == STMT_GOTO;
}

/**
 * \brief Is this RTL a branch instruction?
 */
bool
RTL::isBranch() const
{
	if (stmtList.empty()) return false;
	return stmtList.back()->getKind() == STMT_BRANCH;
}

/**
 * \brief Is this RTL a call instruction?
 */
bool
RTL::isCall() const
{
	if (stmtList.empty()) return false;
	return stmtList.back()->getKind() == STMT_CALL;
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
	accept(ssc);
	return ssc.getLast();
}
