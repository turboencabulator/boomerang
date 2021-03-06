/**
 * \file
 * \brief Implementation of the BasicBlock class.
 *
 * \authors
 * Copyright (C) 1997-2001, The University of Queensland
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

#include "basicblock.h"

#include "boomerang.h"
#include "cfg.h"
#include "exp.h"
#include "hllcode.h"
#include "proc.h"
#include "prog.h"
#include "rtl.h"
#include "statement.h"
#include "type.h"
#include "types.h"
#include "visitor.h"

#ifdef GARBAGE_COLLECTOR
#include <gc/gc.h>
#endif

#include <algorithm>
#include <sstream>

#include <cassert>

/**
 * \brief Destructor.
 */
BasicBlock::~BasicBlock()
{
	// Delete the RTLs
	if (m_pRtls) {
		for (const auto &rtl : *m_pRtls) {
			delete rtl;
		}
	}

	// and delete the list
	delete m_pRtls;
}

/**
 * \brief Private constructor.  Called by Cfg::newBB.
 * \param pRtls
 * \param bbType
 */
BasicBlock::BasicBlock(std::list<RTL *> *pRtls, BBTYPE bbType) :
	m_nodeType(bbType)
{
	assert(pRtls);
	assert(bbType != INCOMPLETE);
	setRTLs(pRtls);
}

bool
BasicBlock::isCaseOption() const
{
	if (caseHead)
		for (const auto &succ : caseHead->m_OutEdges)
			if (succ == this)
				return true;
	return false;
}

/**
 * \brief Return whether this BB has been traversed or not.
 *
 * \returns  True if traversed.
 */
bool
BasicBlock::isTraversed() const
{
	return m_iTraversed;
}

/**
 * \brief Sets the traversed flag.
 *
 * \param[in] bTraversed  true to set this BB to traversed.
 */
void
BasicBlock::setTraversed(bool bTraversed)
{
	m_iTraversed = bTraversed;
}

/**
 * Sets the RTLs for this BB. This is the only place that the RTLs for a block
 * must be set as we need to add the back link for a call instruction to its
 * enclosing BB.
 *
 * \param rtls  A list of RTLs.
 */
void
BasicBlock::setRTLs(std::list<RTL *> *rtls)
{
	// should we delete old ones here?  breaks some things - trent
	m_pRtls = rtls;

	// Used to set the link between the last instruction (a call) and this BB if this is a call BB
}

/**
 * BBs are complete after being populated with RTLs and a type.
 *
 * \returns  true if this basic block is complete.
 */
bool
BasicBlock::isComplete() const
{
	return m_nodeType != INCOMPLETE;
}

/**
 * \brief Return the type of the basic block.
 *
 * \returns  The type of the basic block.
 */
BBTYPE
BasicBlock::getType() const
{
	return m_nodeType;
}

/**
 * \brief Set the type of the basic block.
 *
 * Update the type.  Used for example where a COMPJUMP type is updated to an
 * NWAY when a switch idiom is discovered.
 *
 * \param[in] bbType  The new type.
 */
void
BasicBlock::updateType(BBTYPE bbType)
{
	assert(m_pRtls);
	assert(bbType != INCOMPLETE);
	m_nodeType = bbType;
}

/**
 * \brief Print to a string (for debugging).
 *
 * \returns  The string.
 */
std::string
BasicBlock::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

/**
 * \brief Print the BB. For -R and for debugging.
 *
 * Display the whole BB to the given stream.  Used for "-R" option, and handy
 * for debugging.
 *
 * \param os  Stream to output to.
 */
void
BasicBlock::print(std::ostream &os, bool html) const
{
	if (html)
		os << "<br>";
	switch (m_nodeType) {
	case INCOMPLETE: os << "Incomplete";    break;
	case INVALID:    os << "Invalid";       break;
	case FALL:       os << "Fall";          break;
	case ONEWAY:     os << "Oneway";        break;
	case TWOWAY:     os << "Twoway";        break;
	case NWAY:       os << "Nway";          break;
	case COMPJUMP:   os << "Computed jump"; break;
	case COMPCALL:   os << "Computed call"; break;
	case CALL:       os << "Call";          break;
	case RET:        os << "Ret";           break;
	}
	os << " BB:\n";
	os << "in edges:" << std::hex;
	for (const auto &pred : m_InEdges)
		os << " " << pred->getHiAddr();
	os << std::dec << "\n";
	os << "out edges:" << std::hex;
	for (const auto &succ : m_OutEdges)
		os << " " << succ->getLowAddr();
	os << std::dec << "\n";
	if (m_pRtls) {  // Can be zero if e.g. INVALID
		if (html)
			os << "<table>\n";
		for (const auto &rtl : *m_pRtls)
			rtl->print(os, html);
		if (html)
			os << "</table>\n";
	}
}

#if 0 // Cruft?
bool
BasicBlock::isBackEdge(const BasicBlock *pred) const
{
	return this == pred || (m_DFTfirst < pred->m_DFTfirst && m_DFTlast > pred->m_DFTlast);
}
#endif

/**
 * \brief Get the address associated with the BB.  Note that this is not
 * always the same as getting the address of the first RTL (e.g. if the first
 * RTL is a delay instruction of a DCTI instruction; then the address of this
 * RTL will be 0).
 *
 * Get the lowest real address associated with this BB.  Note that although
 * this is usually the address of the first RTL, it is not always so.  For
 * example, if the BB contains just a delayed branch, and the delay
 * instruction for the branch does not affect the branch, so the delay
 * instruction is copied in front of the branch instruction.  Its address will
 * be UpdateAddress()d to 0, since it is "not really there", so the low
 * address for this BB will be the address of the branch.
 *
 * \returns  The lowest real address associated with this BB.
 */
ADDRESS
BasicBlock::getLowAddr() const
{
	if (!m_pRtls || m_pRtls->empty()) return 0;
	ADDRESS a = m_pRtls->front()->getAddress();
	if (a == 0 && m_pRtls->size() > 1) {
		auto it = m_pRtls->begin();
		ADDRESS add2 = (*++it)->getAddress();
		// This is a bit of a hack for 286 programs, whose main actually starts at offset 0. A better solution would be
		// to change orphan BBs' addresses to NO_ADDRESS, but I suspect that this will cause many problems. MVE
		if (add2 < 0x10)
			// Assume that 0 is the real address
			return 0;
		return add2;
	}
	return a;
}

/**
 * Get the highest address associated with this BB.  This is always the
 * address associated with the last RTL.
 */
ADDRESS
BasicBlock::getHiAddr() const
{
	assert(m_pRtls);
	return m_pRtls->back()->getAddress();
}

/**
 * Get pointer to the list of RTLs.
 */
std::list<RTL *> *
BasicBlock::getRTLs() const
{
	return m_pRtls;
}

RTL *
BasicBlock::getRTLWithStatement(Statement *stmt) const
{
	if (m_pRtls)
		for (const auto &rtl : *m_pRtls)
			for (const auto &s : rtl->getList())
				if (s == stmt)
					return rtl;
	return nullptr;
}

/**
 * \brief Get the set of in edges.
 *
 * Get a constant reference to the vector of in edges.
 *
 * \returns  A constant reference to the vector of in edges.
 */
const std::vector<BasicBlock *> &
BasicBlock::getInEdges() const
{
	return m_InEdges;
}

/**
 * \brief Get the set of out edges.
 *
 * Get a constant reference to the vector of out edges.
 *
 * \returns  A constant reference to the vector of out edges.
 */
const std::vector<BasicBlock *> &
BasicBlock::getOutEdges() const
{
	return m_OutEdges;
}

/**
 * \brief Change an existing out-edge to a new value.
 *
 * Change the given out-edge (0 is first) to the given successor.
 * Also updates the in-edges of the old and new successors.
 *
 * \param i     Index (0 based) of out-edge to change.
 * \param succ  BB that will be the new successor.
 */
void
BasicBlock::setOutEdge(int i, BasicBlock *succ)
{
	assert(i < (int)m_OutEdges.size());
	m_OutEdges[i]->deleteInEdge(this);
	m_OutEdges[i] = succ;
	succ->addInEdge(this);
}

/**
 * \brief Get the n-th out edge, or null if it does not exist.
 *
 * Returns the i-th out edge of this BB; counting starts at 0.
 *
 * \param i  Index (0 based) of the desired out edge.
 * \returns  The i-th out edge; null if there is no such out edge.
 */
BasicBlock *
BasicBlock::getOutEdge(unsigned int i) const
{
	if (i < m_OutEdges.size())
		return m_OutEdges[i];
	return nullptr;
}

/**
 * Given an address, returns the outedge which corresponds to that address or
 * null if there was no such outedge.
 *
 * \param addr  The address.
 */
BasicBlock *
BasicBlock::getCorrectOutEdge(ADDRESS addr) const
{
	for (const auto &succ : m_OutEdges)
		if (succ->getLowAddr() == addr)
			return succ;
	return nullptr;
}

/**
 * Change all in-edges into this BB to instead point to a successor BB.
 * Usually called when this BB is being combined with its successor and will
 * soon be deleted.
 */
void
BasicBlock::bypass(BasicBlock *succ)
{
	for (const auto &pred : m_InEdges) {
		auto it = std::find(pred->m_OutEdges.begin(), pred->m_OutEdges.end(), this);
		assert(it != pred->m_OutEdges.end());
		*it = succ;
		succ->addInEdge(pred);
	}
	m_InEdges.clear();
}

/**
 * \brief Add an in-edge.
 *
 * Add the given in-edge.  Needed for example when duplicating BBs.
 *
 * \param pred  BB that will be a new predecessor.
 */
void
BasicBlock::addInEdge(BasicBlock *pred)
{
	m_InEdges.push_back(pred);
}

/**
 * \brief Delete an in-edge.
 *
 * Delete the in-edge from the given BB.  Needed for example when duplicating
 * BBs.
 */
void
BasicBlock::deleteInEdge(BasicBlock *pred)
{
	auto it = std::find(m_InEdges.begin(), m_InEdges.end(), pred);
	if (it != m_InEdges.end())
		m_InEdges.erase(it);
}

/**
 * Add a new out-edge to this BB.
 * Also add a corresponding in-edge to the successor BB.
 */
void
BasicBlock::addEdge(BasicBlock *succ)
{
	m_OutEdges.push_back(succ);
	succ->addInEdge(this);
}

/**
 * Remove an out-edge from this BB.
 * Also remove a corresponding in-edge from the successor BB.
 */
void
BasicBlock::deleteEdge(BasicBlock *succ)
{
	succ->deleteInEdge(this);
	auto it = std::find(m_OutEdges.begin(), m_OutEdges.end(), succ);
	if (it != m_OutEdges.end())
		m_OutEdges.erase(it);
}

#if 0 // Cruft?
/**
 * Traverse this node and recurse on its children in a depth first manner.
 * Records the times at which this node was first visited and last visited.
 *
 * \param first  The number of nodes that have been visited.
 * \param last   The number of nodes that have been visited for the last time
 *               during this traversal.
 * \returns      The number of nodes (including this one) that were traversed
 *               from this node.
 */
unsigned
BasicBlock::DFTOrder(int &first, int &last)
{
	m_DFTfirst = ++first;

	unsigned numTraversed = 1;
	m_iTraversed = true;

	for (const auto &succ : m_OutEdges) {
		if (!succ->m_iTraversed)
			numTraversed += succ->DFTOrder(first, last);
	}

	m_DFTlast = ++last;

	return numTraversed;
}

/**
 * Traverse this node and recurse on its parents in a reverse depth first
 * manner.  Records the times at which this node was first visited and last
 * visited.
 *
 * \param first  The number of nodes that have been visited.
 * \param last   The number of nodes that have been visited for the last time
 *               during this traversal.
 * \returns      The number of nodes (including this one) that were traversed
 *               from this node.
 */
unsigned
BasicBlock::RevDFTOrder(int &first, int &last)
{
	m_DFTrevfirst = ++first;

	unsigned numTraversed = 1;
	m_iTraversed = true;

	for (const auto &pred : m_InEdges) {
		if (!pred->m_iTraversed)
			numTraversed += pred->RevDFTOrder(first, last);
	}

	m_DFTrevlast = ++last;

	return numTraversed;
}
#endif

/**
 * Static comparison function that returns true if the first BB has an address
 * less than the second BB.
 *
 * \param bb1  First BB.
 * \param bb2  Last BB.
 * \returns    bb1.address < bb2.address
 */
bool
BasicBlock::lessAddress(BasicBlock *bb1, BasicBlock *bb2)
{
	return bb1->getLowAddr() < bb2->getLowAddr();
}

#if 0 // Cruft?
/**
 * Static comparison function that returns true if the first BB has DFT first
 * order less than the second BB.
 *
 * \param bb1  First BB.
 * \param bb2  Last BB.
 * \returns    bb1.first_DFS < bb2.first_DFS
 */
bool
BasicBlock::lessFirstDFT(BasicBlock *bb1, BasicBlock *bb2)
{
	return bb1->m_DFTfirst < bb2->m_DFTfirst;
}

/**
 * Static comparison function that returns true if the first BB has DFT last
 * order less than the second BB.
 *
 * \param bb1  First BB.
 * \param bb2  Last BB.
 * \returns    bb1.last_DFS < bb2.last_DFS
 */
bool
BasicBlock::lessLastDFT(BasicBlock *bb1, BasicBlock *bb2)
{
	return bb1->m_DFTlast < bb2->m_DFTlast;
}
#endif

/**
 * Get the destination of the call, if this is a CALL BB with a fixed dest.
 * Otherwise, return NO_ADDRESS.
 *
 * \returns  Native destination of the call, or NO_ADDRESS.
 */
ADDRESS
BasicBlock::getCallDest() const
{
	if (m_nodeType == CALL && !m_pRtls->empty()) {
		const auto &stmts = m_pRtls->back()->getList();
		for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
			if (auto call = dynamic_cast<CallStatement *>(*srit))
				return call->getFixedDest();
		}
	}
	return NO_ADDRESS;
}

Proc *
BasicBlock::getCallDestProc() const
{
	if (m_nodeType == CALL && !m_pRtls->empty()) {
		const auto &stmts = m_pRtls->back()->getList();
		for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
			if (auto call = dynamic_cast<CallStatement *>(*srit))
				return call->getDestProc();
		}
	}
	return nullptr;
}

/*
 * \brief Get first statement in a BB.
 */
Statement *
BasicBlock::getFirstStmt(rtlit &rit, stlit &sit)
{
	if (m_pRtls) {
		for (rit = m_pRtls->begin(); rit != m_pRtls->end(); ++rit) {
			RTL *rtl = *rit;
			sit = rtl->getList().begin();
			if (sit != rtl->getList().end())
				return *sit;
		}
	}
	return nullptr;
}

/*
 * \brief Get next statement in a BB.
 */
Statement *
BasicBlock::getNextStmt(rtlit &rit, stlit &sit)
{
	if (++sit != (*rit)->getList().end())
		return *sit;  // End of current RTL not reached, so return next
	// Else, find next non-empty RTL & return its first statement
	do {
		if (++rit == m_pRtls->end())
			return nullptr;  // End of all RTLs reached, return null Statement
	} while ((*rit)->getList().empty());  // Ignore all RTLs with no statements
	sit = (*rit)->getList().begin();  // Point to 1st statement at start of next RTL
	return *sit;                      // Return first statement
}

Statement *
BasicBlock::getPrevStmt(rtlrit &rrit, stlrit &srit)
{
	if (++srit != (*rrit)->getList().rend())
		return *srit;  // Beginning of current RTL not reached, so return next
	// Else, find prev non-empty RTL & return its last statement
	do {
		if (++rrit == m_pRtls->rend())
			return nullptr;  // End of all RTLs reached, return null Statement
	} while ((*rrit)->getList().empty());  // Ignore all RTLs with no statements
	srit = (*rrit)->getList().rbegin();  // Point to last statement at end of prev RTL
	return *srit;                       // Return last statement
}

Statement *
BasicBlock::getLastStmt(rtlrit &rrit, stlrit &srit)
{
	if (m_pRtls) {
		for (rrit = m_pRtls->rbegin(); rrit != m_pRtls->rend(); ++rrit) {
			RTL *rtl = *rrit;
			srit = rtl->getList().rbegin();
			if (srit != rtl->getList().rend())
				return *srit;
		}
	}
	return nullptr;
}

/**
 * For those of us that don't want the iterators.
 */
Statement *
BasicBlock::getFirstStmt()
{
	rtlit rit;
	stlit sit;
	return getFirstStmt(rit, sit);
}

/**
 * For those of us that don't want the iterators.
 */
Statement *
BasicBlock::getLastStmt()
{
	rtlrit rrit;
	stlrit srit;
	return getLastStmt(rrit, srit);
}

void
BasicBlock::getStatements(StatementList &stmts)
{
	if (m_pRtls) {
		for (const auto &rtl : *m_pRtls) {
			for (const auto &stmt : rtl->getList()) {
				if (!stmt->getBB())
					stmt->setBB(this);
				stmts.append(stmt);
			}
		}
	}
}

/*
 * Structuring and code generation.
 *
 * This code is whole heartly based on AST by Doug Simon. Portions may be
 * copyright to him and are available under a BSD style license.
 *
 * Adapted for Boomerang by Trent Waddington, 20 June 2002.
 */

/**
 * \brief Get the condition.
 */
Exp *
BasicBlock::getCond() const throw (LastStatementNotABranchError)
{
	// the condition will be in the last rtl
	assert(m_pRtls);
	const auto &last = m_pRtls->back();
	// it should contain a BranchStatement
	if (auto bs = dynamic_cast<BranchStatement *>(last->getHlStmt()))
		return bs->getCondExpr();
	if (VERBOSE)
		LOG << "throwing LastStatementNotABranchError\n";
	throw LastStatementNotABranchError(last->getHlStmt());
}

/**
 * \brief Get the destination expression, if any.
 */
Exp *
BasicBlock::getDest() const throw (LastStatementNotAGotoError)
{
	// The destianation will be in the last rtl
	assert(m_pRtls);
	const auto &lastRtl = m_pRtls->back();
	// It should contain a GotoStatement or derived class
	Statement *lastStmt = lastRtl->getHlStmt();
	if (auto cs = dynamic_cast<CaseStatement *>(lastStmt)) {
		// Get the expression from the switch info
		if (auto si = cs->getSwitchInfo())
			return si->pSwitchVar;
	} else if (auto gs = dynamic_cast<GotoStatement *>(lastStmt)) {
		return gs->getDest();
	}
	if (VERBOSE)
		LOG << "throwing LastStatementNotAGotoError\n";
	throw LastStatementNotAGotoError(lastStmt);
}

/**
 * \brief Set the condition.
 */
void
BasicBlock::setCond(Exp *e) throw (LastStatementNotABranchError)
{
	// the condition will be in the last rtl
	assert(m_pRtls);
	// it should contain a BranchStatement
	const auto &stmts = m_pRtls->back()->getList();
	assert(!stmts.empty());
	for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
		if (auto bs = dynamic_cast<BranchStatement *>(*srit)) {
			bs->setCondExpr(e);
			return;
		}
	}
	throw LastStatementNotABranchError(nullptr);
}

/**
 * \brief Check if there is a jump if equals relation.
 */
bool
BasicBlock::isJmpZ(BasicBlock *dest) const
{
	// The condition will be in the last rtl
	assert(m_pRtls);
	// it should contain a BranchStatement
	const auto &stmts = m_pRtls->back()->getList();
	assert(!stmts.empty());
	for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
		if (auto bs = dynamic_cast<BranchStatement *>(*srit)) {
			BRANCH_TYPE jt = bs->getCond();
			if (jt == BRANCH_JE) {
				const auto &trueEdge = m_OutEdges[0];
				return dest == trueEdge;
			} else if (jt == BRANCH_JNE) {
				const auto &falseEdge = m_OutEdges[1];
				return dest == falseEdge;
			} else {
				return false;
			}
		}
	}
	assert(0);
	return false;
}

/**
 * \brief Get the loop body.
 */
BasicBlock *
BasicBlock::getLoopBody() const
{
	assert(m_structType == PRETESTLOOP || m_structType == POSTTESTLOOP || m_structType == ENDLESSLOOP);
	assert(m_OutEdges.size() == 2);
	if (m_OutEdges[0] != m_loopFollow)
		return m_OutEdges[0];
	return m_OutEdges[1];
}

/**
 * \brief Establish if this BB is an ancestor of another BB.
 */
bool
BasicBlock::isAncestorOf(const BasicBlock *other) const
{
	return ((loopStamps[0] < other->loopStamps[0]
	      && loopStamps[1] > other->loopStamps[1])
	     || (revLoopStamps[0] < other->revLoopStamps[0]
	      && revLoopStamps[1] > other->revLoopStamps[1]));
#if 0
	return (m_DFTfirst < other->m_DFTfirst
	     && m_DFTlast  > other->m_DFTlast)
	    || (m_DFTrevlast  < other->m_DFTrevlast
	     && m_DFTrevfirst > other->m_DFTrevfirst);
#endif
}

/**
 * \brief Simplify all the expressions in this BB.
 */
void
BasicBlock::simplify()
{
	if (m_pRtls)
		for (const auto &rtl : *m_pRtls)
			rtl->simplify();
	if (m_nodeType == TWOWAY) {
		if (!m_pRtls || m_pRtls->empty()) {
			m_nodeType = FALL;
		} else {
			const auto &rtl = m_pRtls->back();
			if (rtl->isGoto()) {
				m_nodeType = ONEWAY;
			} else if (!rtl->isBranch()) {
				m_nodeType = FALL;
			}
		}
		if (m_nodeType == FALL) {
			// set out edges to be the second one
			if (VERBOSE)
				LOG << "turning TWOWAY into FALL: "
				    << "0x" << std::hex << m_OutEdges[0]->getLowAddr() << std::dec << " "
				    << "0x" << std::hex << m_OutEdges[1]->getLowAddr() << std::dec << "\n";
			auto redundant = m_OutEdges[0];
			m_OutEdges[0] = m_OutEdges[1];
			m_OutEdges.resize(1);
			if (VERBOSE)
				LOG << "redundant edge to 0x" << std::hex << redundant->getLowAddr() << std::dec << " inedges: ";
			auto rinedges = std::vector<BasicBlock *>();
			rinedges.swap(redundant->m_InEdges);
			for (const auto &pred : rinedges) {
				if (VERBOSE)
					LOG << "0x" << std::hex << pred->getLowAddr() << std::dec << " ";
				if (pred != this)
					redundant->m_InEdges.push_back(pred);
				else if (VERBOSE)
					LOG << "(ignored) ";
			}
			if (VERBOSE)
				LOG << "\n   after: 0x" << std::hex << m_OutEdges[0]->getLowAddr() << std::dec << "\n";
		} else if (m_nodeType == ONEWAY) {
			// set out edges to be the first one
			if (VERBOSE)
				LOG << "turning TWOWAY into ONEWAY: "
				    << "0x" << std::hex << m_OutEdges[0]->getLowAddr() << std::dec << " "
				    << "0x" << std::hex << m_OutEdges[1]->getLowAddr() << std::dec << "\n";
			auto redundant = m_OutEdges[1];
			m_OutEdges.resize(1);
			if (VERBOSE)
				LOG << "redundant edge to 0x" << std::hex << redundant->getLowAddr() << std::dec << " inedges: ";
			auto rinedges = std::vector<BasicBlock *>();
			rinedges.swap(redundant->m_InEdges);
			for (const auto &pred : rinedges) {
				if (VERBOSE)
					LOG << "0x" << std::hex << pred->getLowAddr() << std::dec << " ";
				if (pred != this)
					redundant->m_InEdges.push_back(pred);
				else if (VERBOSE)
					LOG << "(ignored) ";
			}
			if (VERBOSE)
				LOG << "\n   after: 0x" << std::hex << m_OutEdges[0]->getLowAddr() << std::dec << "\n";
		}
	}
}

/**
 * \brief Establish if this BB has a back edge to the given destination.
 */
bool
BasicBlock::hasBackEdgeTo(const BasicBlock *dest) const
{
	//assert(HasEdgeTo(dest) || dest == this);
	return dest == this || dest->isAncestorOf(this);
}

/**
 * \returns  true if every parent (i.e. forward in edge source) of this node
 * has had its code generated.
 */
bool
BasicBlock::allParentsGenerated() const
{
	for (const auto &pred : m_InEdges)
		if (!pred->hasBackEdgeTo(this)
		 &&  pred->traversed != DFS_CODEGEN)
			return false;
	return true;
}

/**
 * Emits a goto statement (at the correct indentation level) with the
 * destination label for dest.  Also places the label just before the
 * destination code if it isn't already there.  If the goto is to the return
 * block, it would be nice to emit a 'return' instead (but would have to
 * duplicate the other code in that return BB).  Also, 'continue' and 'break'
 * statements are used instead if possible.
 */
void
BasicBlock::emitGotoAndLabel(HLLCode *hll, int indLevel, BasicBlock *dest)
{
	if (loopHead && (loopHead == dest || loopHead->loopFollow == dest)) {
		if (loopHead == dest)
			hll->AddContinue(indLevel);
		else
			hll->AddBreak(indLevel);
	} else {
		hll->AddGoto(indLevel, dest->ord);

		dest->hllLabel = true;
	}
}

/**
 * Generates code for each non CTI (except procedure calls) statement within
 * the block.
 */
void
BasicBlock::WriteBB(HLLCode *hll, int indLevel)
{
	if (DEBUG_GEN)
		LOG << "Generating code for BB at 0x" << std::hex << getLowAddr() << std::dec << "\n";

	// Allocate space for a label to be generated for this node and add this to the generated code. The actual label can
	// then be generated now or back patched later
	hll->AddLabel(indLevel, ord);

	if (m_pRtls) {
		for (const auto &rtl : *m_pRtls) {
			if (DEBUG_GEN)
				LOG << "0x" << std::hex << rtl->getAddress() << std::dec << "\t";
			rtl->generateCode(hll, this, indLevel);
		}
		if (DEBUG_GEN)
			LOG << "\n";
	}

	// save the indentation level that this node was written at
	indentLevel = indLevel;
}

void
BasicBlock::generateCode(HLLCode *hll, int indLevel, BasicBlock *latch, std::list<BasicBlock *> &followSet, std::list<BasicBlock *> &gotoSet, UserProc *proc)
{
	// If this is the follow for the most nested enclosing conditional, then don't generate anything. Otherwise if it is
	// in the follow set generate a goto to the follow
	BasicBlock *enclFollow = followSet.empty() ? nullptr : followSet.back();

	if (isIn(gotoSet, this) && !isLatchNode()
	 && ((latch && latch->loopHead && this == latch->loopHead->loopFollow)
	  || !allParentsGenerated())) {
		emitGotoAndLabel(hll, indLevel, this);
		return;
	} else if (isIn(followSet, this)) {
		if (this != enclFollow)
			emitGotoAndLabel(hll, indLevel, this);
		return;
	}

	// Has this node already been generated?
	if (traversed == DFS_CODEGEN) {
		// this should only occur for a loop over a single block
		// FIXME: is this true? Perl_list (0x8068028) in the SPEC 2000 perlbmk seems to have a case with sType = Cond,
		// lType == PreTested, and latchNode == nullptr
		//assert(sType == Loop && lType == PostTested && latchNode == this);
		return;
	} else
		traversed = DFS_CODEGEN;

	// if this is a latchNode and the current indentation level is the same as the first node in the loop, then this
	// write out its body and return otherwise generate a goto
	if (isLatchNode()) {
		if (latch && latch->loopHead
		 && indLevel == latch->loopHead->indentLevel + (latch->loopHead->lType == PreTested ? 1 : 0)) {
			WriteBB(hll, indLevel);
			return;
		} else {
			// unset its traversed flag
			traversed = UNTRAVERSED;

			emitGotoAndLabel(hll, indLevel, this);
			return;
		}
	}

	switch (sType) {
	case Loop:
	case LoopCond:
		// add the follow of the loop (if it exists) to the follow set
		if (loopFollow)
			followSet.push_back(loopFollow);

		if (lType == PreTested) {
			assert(latchNode->m_OutEdges.size() == 1);

			// write the body of the block (excluding the predicate)
			WriteBB(hll, indLevel);

			// write the 'while' predicate
			Exp *cond = getCond();
			if (m_OutEdges[BTHEN] == loopFollow) {
				cond = new Unary(opLNot, cond);
				cond = cond->simplify();
			}
			hll->AddPretestedLoopHeader(indLevel, cond);

			// write the code for the body of the loop
			BasicBlock *loopBody = (m_OutEdges[BELSE] == loopFollow) ? m_OutEdges[BTHEN] : m_OutEdges[BELSE];
			loopBody->generateCode(hll, indLevel + 1, latchNode, followSet, gotoSet, proc);

			// if code has not been generated for the latch node, generate it now
			if (latchNode->traversed != DFS_CODEGEN) {
				latchNode->traversed = DFS_CODEGEN;
				latchNode->WriteBB(hll, indLevel + 1);
			}

			// rewrite the body of the block (excluding the predicate) at the next nesting level after making sure
			// another label won't be generated
			hllLabel = false;
			WriteBB(hll, indLevel + 1);

			// write the loop tail
			hll->AddPretestedLoopEnd(indLevel);
		} else {
			// write the loop header
			if (lType == Endless)
				hll->AddEndlessLoopHeader(indLevel);
			else
				hll->AddPosttestedLoopHeader(indLevel);

			// if this is also a conditional header, then generate code for the conditional. Otherwise generate code
			// for the loop body.
			if (sType == LoopCond) {
				// set the necessary flags so that generateCode can successfully be called again on this node
				sType = Cond;
				traversed = UNTRAVERSED;
				generateCode(hll, indLevel + 1, latchNode, followSet, gotoSet, proc);
			} else {
				WriteBB(hll, indLevel + 1);

				// write the code for the body of the loop
				m_OutEdges[0]->generateCode(hll, indLevel + 1, latchNode, followSet, gotoSet, proc);
			}

			if (lType == PostTested) {
				// if code has not been generated for the latch node, generate it now
				if (latchNode->traversed != DFS_CODEGEN) {
					latchNode->traversed = DFS_CODEGEN;
					latchNode->WriteBB(hll, indLevel + 1);
				}

				//hll->AddPosttestedLoopEnd(indLevel, getCond());
				// MVE: the above seems to fail when there is a call in the middle of the loop (so loop is 2 BBs)
				// Just a wild stab:
				hll->AddPosttestedLoopEnd(indLevel, latchNode->getCond());
			} else {
				assert(lType == Endless);

				// if code has not been generated for the latch node, generate it now
				if (latchNode->traversed != DFS_CODEGEN) {
					latchNode->traversed = DFS_CODEGEN;
					latchNode->WriteBB(hll, indLevel + 1);
				}

				// write the closing bracket for an endless loop
				hll->AddEndlessLoopEnd(indLevel);
			}
		}

		// write the code for the follow of the loop (if it exists)
		if (loopFollow) {
			// remove the follow from the follow set
			followSet.pop_back();

			if (loopFollow->traversed != DFS_CODEGEN)
				loopFollow->generateCode(hll, indLevel, latch, followSet, gotoSet, proc);
			else
				emitGotoAndLabel(hll, indLevel, loopFollow);
		}
		break;

	case Cond:
		{
			// reset this back to LoopCond if it was originally of this type
			if (latchNode)
				sType = LoopCond;

			// for 2 way conditional headers that are effectively jumps into
			// or out of a loop or case body, we will need a new follow node
			BasicBlock *tmpCondFollow = nullptr;

			// keep track of how many nodes were added to the goto set so that
			// the correct number are removed
			int gotoTotal = 0;

			// add the follow to the follow set if this is a case header
			if (cType == Case)
				followSet.push_back(condFollow);
			else if (cType != Case && condFollow) {
				// For a structured two conditional header, its follow is
				// added to the follow set
				//myLoopHead = (sType == LoopCond ? this : loopHead);

				if (usType == Structured)
					followSet.push_back(condFollow);

				// Otherwise, for a jump into/outof a loop body, the follow is added to the goto set.
				// The temporary follow is set for any unstructured conditional header branch that is within the
				// same loop and case.
				else {
					if (usType == JumpInOutLoop) {
						// define the loop header to be compared against
						BasicBlock *myLoopHead = (sType == LoopCond ? this : loopHead);
						gotoSet.push_back(condFollow);
						++gotoTotal;

						// also add the current latch node, and the loop header of the follow if they exist
						if (latch) {
							gotoSet.push_back(latch);
							++gotoTotal;
						}

						if (condFollow->loopHead && condFollow->loopHead != myLoopHead) {
							gotoSet.push_back(condFollow->loopHead);
							++gotoTotal;
						}
					}

					if (cType == IfThen)
						tmpCondFollow = m_OutEdges[BELSE];
					else
						tmpCondFollow = m_OutEdges[BTHEN];

					// for a jump into a case, the temp follow is added to the follow set
					if (usType == JumpIntoCase)
						followSet.push_back(tmpCondFollow);
				}
			}

			// write the body of the block (excluding the predicate)
			WriteBB(hll, indLevel);

			// write the conditional header
			SWITCH_INFO *psi = nullptr;  // Init to nullptr to suppress a warning
			if (cType == Case) {
				// The CaseStatement will be in the last RTL this BB
				RTL *last = m_pRtls->back();
				CaseStatement *cs = (CaseStatement *)last->getHlStmt();
				psi = cs->getSwitchInfo();
				// Write the switch header (i.e. "switch(var) {")
				hll->AddCaseCondHeader(indLevel, psi->pSwitchVar);
			} else {
				Exp *cond = getCond();
				if (!cond)
					cond = new Const(0xfeedface);  // hack, but better than a crash
				if (cType == IfElse) {
					cond = new Unary(opLNot, cond->clone());
					cond = cond->simplify();
				}
				if (cType == IfThenElse)
					hll->AddIfElseCondHeader(indLevel, cond);
				else
					hll->AddIfCondHeader(indLevel, cond);
			}

			// write code for the body of the conditional
			if (cType != Case) {
				BasicBlock *succ = m_OutEdges[cType == IfElse ? BELSE : BTHEN];

				// emit a goto statement if the first clause has already been
				// generated or it is the follow of this node's enclosing loop
				if (succ->traversed == DFS_CODEGEN || (loopHead && succ == loopHead->loopFollow))
					emitGotoAndLabel(hll, indLevel + 1, succ);
				else
					succ->generateCode(hll, indLevel + 1, latch, followSet, gotoSet, proc);

				// generate the else clause if necessary
				if (cType == IfThenElse) {
					// generate the 'else' keyword and matching brackets
					hll->AddIfElseCondOption(indLevel);

					succ = m_OutEdges[BELSE];

					// emit a goto statement if the second clause has already
					// been generated
					if (succ->traversed == DFS_CODEGEN)
						emitGotoAndLabel(hll, indLevel + 1, succ);
					else
						succ->generateCode(hll, indLevel + 1, latch, followSet, gotoSet, proc);

					// generate the closing bracket
					hll->AddIfElseCondEnd(indLevel);
				} else {
					// generate the closing bracket
					hll->AddIfCondEnd(indLevel);
				}
			} else { // case header
				// TODO: linearly emitting each branch of the switch does not result
				//       in optimal fall-through.
				// generate code for each out branch
				for (unsigned int i = 0; i < m_OutEdges.size(); ++i) {
					// emit a case label
					// FIXME: Not valid for all switch types
					Const caseVal(0);
					if (psi->chForm == 'F')  // "Fortran" style?
						caseVal.setInt(psi->dests[i]);  // Yes, use the table value itself
					else
						caseVal.setInt((int)(psi->iLower + i));
					hll->AddCaseCondOption(indLevel, &caseVal);

					// generate code for the current outedge
					BasicBlock *succ = m_OutEdges[i];
					//assert(succ->caseHead == this || succ == condFollow || HasBackEdgeTo(succ));
					if (succ->traversed == DFS_CODEGEN)
						emitGotoAndLabel(hll, indLevel + 1, succ);
					else
						succ->generateCode(hll, indLevel + 1, latch, followSet, gotoSet, proc);
				}
				// generate the closing bracket
				hll->AddCaseCondEnd(indLevel);
			}


			// do all the follow stuff if this conditional had one
			if (condFollow) {
				// remove the original follow from the follow set if it was
				// added by this header
				if (usType == Structured || usType == JumpIntoCase) {
					assert(gotoTotal == 0);
					followSet.pop_back();
				} else // remove all the nodes added to the goto set
					for (int i = 0; i < gotoTotal; ++i)
						gotoSet.pop_back();

				// do the code generation (or goto emitting) for the new conditional follow if it exists, otherwise do
				// it for the original follow
				if (!tmpCondFollow)
					tmpCondFollow = condFollow;

				if (tmpCondFollow->traversed == DFS_CODEGEN)
					emitGotoAndLabel(hll, indLevel, tmpCondFollow);
				else
					tmpCondFollow->generateCode(hll, indLevel, latch, followSet, gotoSet, proc);
			}
		}
		break;
	case Seq:
		{
			// generate code for the body of this block
			WriteBB(hll, indLevel);

			// return if this is the 'return' block (i.e. has no out edges) after emmitting a 'return' statement
			if (getType() == RET) {
				// This should be emited now, like a normal statement
				//hll->AddReturnStatement(indLevel, getReturnVal());
				return;
			}

			// return if this doesn't have any out edges (emit a warning)
			if (m_OutEdges.empty()) {
				std::cerr << "WARNING: no out edge for this BB in " << proc->getName() << ":\n";
				this->print(std::cerr);
				std::cerr << "\n";
				if (m_nodeType == COMPJUMP) {
					assert(!m_pRtls->empty());
					const auto &stmts = m_pRtls->back()->getList();
					assert(!stmts.empty());
					auto gs = (GotoStatement *)stmts.back();
					std::ostringstream ost;
					ost << "goto " << *gs->getDest();
					hll->AddLineComment(ost.str());
				}
				return;
			}

			auto succ = m_OutEdges[0];
			if (m_OutEdges.size() != 1) {
				BasicBlock *other = m_OutEdges[1];
				LOG << "found seq with more than one outedge!\n";
				if (getDest()->isIntConst()
				 && ((Const *)getDest())->getInt() == (int)succ->getLowAddr()) {
					other = succ;
					succ = m_OutEdges[1];
					LOG << "taken branch is first out edge\n";
				}

				try {
					hll->AddIfCondHeader(indLevel, getCond());
					if (other->traversed == DFS_CODEGEN)
						emitGotoAndLabel(hll, indLevel + 1, other);
					else
						other->generateCode(hll, indLevel + 1, latch, followSet, gotoSet, proc);
					hll->AddIfCondEnd(indLevel);
				} catch (LastStatementNotABranchError &) {
					LOG << "last statement is not a cond, don't know what to do with this.\n";
				}
			}

			// generate code for its successor if it hasn't already been visited and is in the same loop/case and is not
			// the latch for the current most enclosing loop.  The only exception for generating it when it is not in
			// the same loop is when it is only reached from this node
			if (succ->traversed == DFS_CODEGEN
			 || ((succ->loopHead != loopHead) && (!succ->allParentsGenerated() || isIn(followSet, succ)))
			 || (latch && latch->loopHead && latch->loopHead->loopFollow == succ)
			 || !(caseHead == succ->caseHead || (caseHead && succ == caseHead->condFollow)))
				emitGotoAndLabel(hll, indLevel, succ);
			else {
				if (caseHead && succ == caseHead->condFollow) {
					// generate the 'break' statement
					hll->AddCaseCondOptionEnd(indLevel);
				} else if (!caseHead || caseHead != succ->caseHead || !succ->isCaseOption())
					succ->generateCode(hll, indLevel, latch, followSet, gotoSet, proc);
			}
		}
		break;
	default:
		std::cerr << "unhandled sType " << (int)sType << "\n";
	}
}

/**
 * Get the destination proc.
 * \note This must be a call BB!
 */
Proc *
BasicBlock::getDestProc() const
{
	// The last Statement of the last RTL should be a CallStatement
	auto call = dynamic_cast<CallStatement *>(m_pRtls->back()->getHlStmt());
	assert(call);
	Proc *proc = call->getDestProc();
	if (!proc) {
		std::cerr << "Indirect calls not handled yet\n";
		assert(0);
	}
	return proc;
}

void
BasicBlock::setLoopStamps(int &time, std::vector<BasicBlock *> &order)
{
	// timestamp the current node with the current time and set its traversed
	// flag
	traversed = DFS_LNUM;
	loopStamps[0] = ++time;

	// recurse on unvisited children and set inedges for all children
	for (const auto &succ : m_OutEdges) {
		// set the in edge from this child to its parent (the current node)
		// (not done here, might be a problem)
		// outEdges[i]->inEdges.Add(this);

		// recurse on this child if it hasn't already been visited
		if (succ->traversed != DFS_LNUM)
			succ->setLoopStamps(time, order);
	}

	// set the the second loopStamp value
	loopStamps[1] = ++time;

	// add this node to the ordering structure as well as recording its position within the ordering
	ord = order.size();
	order.push_back(this);
}

void
BasicBlock::setRevLoopStamps(int &time)
{
	// timestamp the current node with the current time and set its traversed flag
	traversed = DFS_RNUM;
	revLoopStamps[0] = ++time;

	// recurse on the unvisited children in reverse order
	for (auto rit = m_OutEdges.rbegin(); rit != m_OutEdges.rend(); ++rit) {
		const auto &succ = *rit;
		// recurse on this child if it hasn't already been visited
		if (succ->traversed != DFS_RNUM)
			succ->setRevLoopStamps(time);
	}

	// set the the second loopStamp value
	revLoopStamps[1] = ++time;
}

void
BasicBlock::setRevOrder(std::vector<BasicBlock *> &order)
{
	// Set this node as having been traversed during the post domimator DFS ordering traversal
	traversed = DFS_PDOM;

	// recurse on unvisited children
	for (const auto &pred : m_InEdges)
		if (pred->traversed != DFS_PDOM)
			pred->setRevOrder(order);

	// add this node to the ordering structure and record the post dom. order of this node as its index within this
	// ordering structure
	revOrd = order.size();
	order.push_back(this);
}

void
BasicBlock::setCaseHead(BasicBlock *head, BasicBlock *follow)
{
	assert(!caseHead);

	traversed = DFS_CASE;

	// don't tag this node if it is the case header under investigation
	if (this != head)
		caseHead = head;

	// if this is a nested case header, then it's member nodes will already have been tagged so skip straight to its
	// follow
	if (getType() == NWAY && this != head) {
		if (condFollow
		 && condFollow->traversed != DFS_CASE
		 && condFollow != follow)
			condFollow->setCaseHead(head, follow);
	} else {
		// traverse each child of this node that:
		//   i) isn't on a back-edge,
		//  ii) hasn't already been traversed in a case tagging traversal and,
		// iii) isn't the follow node.
		for (const auto &succ : m_OutEdges)
			if (!hasBackEdgeTo(succ)
			 && succ->traversed != DFS_CASE
			 && succ != follow)
				succ->setCaseHead(head, follow);
	}
}

void
BasicBlock::setStructType(structType s)
{
	// if this is a conditional header, determine exactly which type of conditional header it is (i.e. switch, if-then,
	// if-then-else etc.)
	if (s == Cond) {
		if (getType() == NWAY)
			cType = Case;
		else if (m_OutEdges[BELSE] == condFollow)
			cType = IfThen;
		else if (m_OutEdges[BTHEN] == condFollow)
			cType = IfElse;
		else
			cType = IfThenElse;
	}

	sType = s;
}

void
BasicBlock::setUnstructType(unstructType us)
{
	assert((sType == Cond || sType == LoopCond) && cType != Case);
	usType = us;
}

unstructType
BasicBlock::getUnstructType() const
{
	assert((sType == Cond || sType == LoopCond) && cType != Case);
	return usType;
}

void
BasicBlock::setLoopType(loopType l)
{
	assert(sType == Loop || sType == LoopCond);
	lType = l;

	// set the structured class (back to) just Loop if the loop type is PreTested OR it's PostTested and is a single
	// block loop
	if (lType == PreTested || (lType == PostTested && this == latchNode))
		sType = Loop;
}

loopType
BasicBlock::getLoopType() const
{
	assert(sType == Loop || sType == LoopCond);
	return lType;
}

void
BasicBlock::setCondType(condType c)
{
	assert(sType == Cond || sType == LoopCond);
	cType = c;
}

condType
BasicBlock::getCondType() const
{
	assert(sType == Cond || sType == LoopCond);
	return cType;
}

bool
BasicBlock::inLoop(BasicBlock *header, BasicBlock *latch) const
{
	assert(header->latchNode == latch);
	assert(header == latch
	    || ((header->loopStamps[0] > latch->loopStamps[0] && latch->loopStamps[1] > header->loopStamps[1])
	     || (header->loopStamps[0] < latch->loopStamps[0] && latch->loopStamps[1] < header->loopStamps[1])));
	// this node is in the loop if it is the latch node OR
	// this node is within the header and the latch is within this when using the forward loop stamps OR
	// this node is within the header and the latch is within this when using the reverse loop stamps
	return this == latch
	    || (header->loopStamps[0] < loopStamps[0] && loopStamps[1] < header->loopStamps[1]
	     && loopStamps[0] < latch->loopStamps[0] && latch->loopStamps[1] < loopStamps[1])
	    || (header->revLoopStamps[0] < revLoopStamps[0] && revLoopStamps[1] < header->revLoopStamps[1]
	     && revLoopStamps[0] < latch->revLoopStamps[0] && latch->revLoopStamps[1] < revLoopStamps[1]);
}

/**
 * \brief For prepending phi functions.
 *
 * Prepend an assignment (usually a PhiAssign or ImplicitAssign).
 *
 * \param proc  The enclosing Proc.
 */
void
BasicBlock::prependStmt(Statement *s, UserProc *proc)
{
	// Check the first RTL (if any)
	s->setBB(this);
	s->setProc(proc);
	if (!m_pRtls->empty()) {
		RTL *rtl = m_pRtls->front();
		if (rtl->getAddress() == 0) {
			// Append to this RTL
			rtl->appendStmt(s);
			return;
		}
	}
	// Otherwise, prepend a new RTL
	auto rtl = new RTL(0, s);
	m_pRtls->push_front(rtl);
}

/**
 * Check for overlap of liveness between the currently live locations
 * (liveLocs) and the set of locations in ls.  Also check for type conflicts
 * if DFA_TYPE_ANALYSIS.  This is a helper function that is not directly
 * declared in the BasicBlock class.
 */
static void
checkForOverlap(LocationSet &liveLocs, LocationSet &ls, ConnectionGraph &ig, UserProc *proc)
{
	// For each location to be considered
	for (const auto &u : ls) {
		auto r = dynamic_cast<RefExp *>(u);
		if (!r) continue;  // Only interested in subscripted vars
		// Interference if we can find a live variable which differs only in the reference
		Exp *dr;
		if (liveLocs.findDifferentRef(r, dr)) {
			// We have an interference between r and dr. Record it
			ig.connect(r, dr);
			if (VERBOSE || DEBUG_LIVENESS)
				LOG << "interference of " << *dr << " with " << *r << "\n";
		}
		// Add the uses one at a time. Note: don't use makeUnion, because then we don't discover interferences
		// from the same statement, e.g.  blah := r24{2} + r24{3}
		liveLocs.insert(r);
	}
}

bool
BasicBlock::calcLiveness(ConnectionGraph &ig, UserProc *myProc)
{
	// Start with the liveness at the bottom of the BB
	LocationSet liveLocs, phiLocs;
	getLiveOut(liveLocs, phiLocs);
	// Do the livensses that result from phi statements at successors first.
	// FIXME: document why this is necessary
	checkForOverlap(liveLocs, phiLocs, ig, myProc);
	// For each RTL in this BB
	if (m_pRtls)  // this can be nullptr
		for (auto rrit = m_pRtls->crbegin(); rrit != m_pRtls->crend(); ++rrit) {
			const auto &stmts = (*rrit)->getList();
			// For each statement this RTL
			for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
				Statement *s = *srit;
				LocationSet defs;
				s->getDefinitions(defs);
				// The definitions don't have refs yet
				defs.addSubscript(s /* , myProc->getCFG() */);
#if 0
				// I used to think it necessary to consider definitions as a special case. However, I now believe that
				// this was either an error of implementation (e.g. it didn't seem to correctly consider the livenesses
				// causesd by phis) or something to do with renaming but not propagating certain memory locations.
				// The idea is now to clearly divide locations into those that can be renamed and propagated, and those
				// which are not renamed or propagated. (Check this.)

				// Also consider it an interference if we define a location that is the same base variable. This can happen
				// when there is a definition that is unused but for whatever reason not eliminated
				// This check is done at the "bottom" of the statement, i.e. before we add s's uses and remove s's
				// definitions to liveLocs
				// Note that phi assignments don't count
				if (!dynamic_cast<PhiAssign *>(s))
					checkForOverlap(liveLocs, defs, ig, myProc, false);
#endif
				// Definitions kill uses. Now we are moving to the "top" of statement s
				liveLocs.makeDiff(defs);
				// Phi functions are a special case. The operands of phi functions are uses, but they don't interfere
				// with each other (since they come via different BBs). However, we don't want to put these uses into
				// liveLocs, because then the livenesses will flow to all predecessors. Only the appropriate livenesses
				// from the appropriate phi parameter should flow to the predecessor. This is done in getLiveOut()
				if (dynamic_cast<PhiAssign *>(s)) continue;
				// Check for livenesses that overlap
				LocationSet uses;
				s->addUsedLocs(uses);
				checkForOverlap(liveLocs, uses, ig, myProc);
				if (DEBUG_LIVENESS)
					LOG << " ## liveness: at top of " << *s << ", liveLocs is " << liveLocs << "\n";
			}
		}
	// liveIn is what we calculated last time
	if (!(liveLocs == liveIn)) {
		liveIn = liveLocs;
		return true;  // A change
	} else
		// No change
		return false;
}

/**
 * Locations that are live at the end of this BB are the union of the
 * locations that are live at the start of its successors.
 *
 * liveout gets all the livenesses, and phiLocs gets a subset of these, which
 * are due to phi statements at the top of successors.
 */
void
BasicBlock::getLiveOut(LocationSet &liveout, LocationSet &phiLocs)
{
	liveout.clear();
	for (const auto &succ : m_OutEdges) {
		// First add the non-phi liveness
		liveout.makeUnion(succ->liveIn);
		int j = succ->whichPred(this);
		// The first RTL will have the phi functions, if any
		if (!succ->m_pRtls || succ->m_pRtls->empty())
			continue;
		const auto &stmts = succ->m_pRtls->front()->getList();
		for (const auto &stmt : stmts) {
			// Only interested in phi assignments. Note that it is possible that some phi assignments have been
			// converted to ordinary assignments.
			if (auto pa = dynamic_cast<PhiAssign *>(stmt)) {
				// Get the jth operand to the phi function; it has a use from BB *this
				auto def = pa->getAt(j).def;
				auto r = new RefExp(pa->getLeft()->clone(), def);
				liveout.insert(r);
				phiLocs.insert(r);
				if (DEBUG_LIVENESS)
					LOG << " ## Liveness: adding " << *r << " due to ref to phi " << *pa << " in BB at 0x" << std::hex << getLowAddr() << std::dec << "\n";
			}
		}
	}
}

/**
 * \brief Get the index of my in-edges is BB pred.
 *
 * Basically the "whichPred" function as per Briggs, Cooper, et al (and
 * presumably "Cryton, Ferante, Rosen, Wegman, and Zadek").  Return -1 if not
 * found.
 */
int
BasicBlock::whichPred(BasicBlock *pred) const
{
	int n = m_InEdges.size();
	for (int i = 0; i < n; ++i) {
		if (m_InEdges[i] == pred)
			return i;
	}
	assert(0);
	return -1;
}

//  //  //  //  //  //  //  //  //  //  //  //  //
//                                              //
//       Indirect jump and call analyses        //
//                                              //
//  //  //  //  //  //  //  //  //  //  //  //  //

/*
 *  Switch statements are generally one of the following three
 *  forms, when reduced to high level representation, where var is the
 *  switch variable, numl and numu are the lower and upper bounds of that
 *  variable that are handled by the switch, and T is the address of the
 *  jump table. Out represents a label after the switch statement.
 *  Each has this pattern to establish the upper bound:
 *      jcond (var > numu) out
 *  Most have a subtract (or add) to establish the lower bound; some (e.g.
 *  epc) have a compare and branch if lower to establish the lower bound;
 *  if this is the case, the table address needs to be altered
 * Here are the 4 types discovered so far; size of an address is 4:
 *  A) jmp m[<expr> * 4 + T]
 *  O) jmp m[<expr> * 4 + T] + T
 *  H) jmp m[(((<expr> & mask) * 8) + T) + 4]
 *  R) jmp %pc + m[%pc + ((<expr> * 4) + k)]        // O1
 *  r) jmp %pc + m[%pc + ((<expr> * 4) - k)] - k    // O2
 *  where for forms A, O, R, r <expr> is one of
 *      r[v]                            // numl == 0
 *      r[v] - numl                     // usual case, switch var is int
 *      ((r[v] - numl) << 24) >>A 24)   // switch var is char in lower byte
 *      etc
 * or in form H, <expr> is something like
 *      ((r[v] - numl) >> 4) + (r[v] - numl)
 *      ((r[v] - numl) >> 8) + ((r[v] - numl) >> 2) + (r[v] - numl)
 *      etc.
 *  Forms A and H have a table of pointers to code handling the switch
 *  values; forms O and r have a table of offsets from the start of the
 *  table itself (i.e. all values are the same as with form A, except
 *  that T is subtracted from each entry.) Form R has offsets relative
 *  to the CALL statement that defines "%pc".
 *  Form H tables are actually (<value>, <address>) pairs, and there are
 *  potentially more items in the table than the range of the switch var.
 */

// Switch High Level patterns

/**
 * With array processing, we get a new form, call it form 'a' (don't confuse
 * with form 'A'):
 *
 * Pattern: \<base\>{}[\<index\>]{}
 *
 * where \<index\> could be \<var\> - \<Kmin\>
 */
static Exp *forma = new RefExp(
    new Binary(opArrayIndex,
        new RefExp(
            new Terminal(opWild),
            (Statement *)-1),
        new Terminal(opWild)),
    (Statement *)-1);

/**
 * Pattern: m[ \<expr\> * 4 + T ]
 */
static Exp *formA = Location::memOf(
    new Binary(opPlus,
        new Binary(opMult,
            new Terminal(opWild),
            new Const(4)),
        new Terminal(opWildIntConst)));

/**
 * Pattern: m[ \<expr\> * 4 + T ] + T
 */
static Exp *formO = new Binary(opPlus,
    Location::memOf(
        new Binary(opPlus,
            new Binary(opMult,
                new Terminal(opWild),
                new Const(4)),
            new Terminal(opWildIntConst))),
    new Terminal(opWildIntConst));

/**
 * Pattern: \%pc + m[\%pc + (\<expr\> * 4) + k]
 *
 * where k is a small constant, typically 28 or 20
 */
static Exp *formR = new Binary(opPlus,
    new Terminal(opPC),
    Location::memOf(new Binary(opPlus,
        new Terminal(opPC),
        new Binary(opPlus,
            new Binary(opMult,
                new Terminal(opWild),
                new Const(4)),
            new Terminal(opWildIntConst)))));

/**
 * Pattern: \%pc + m[\%pc + ((\<expr\> * 4) - k)] - k
 *
 * where k is a smallish constant, e.g. 288 (/usr/bin/vi 2.6, 0c4233c).
 */
static Exp *formr = new Binary(opMinus,
    new Binary(opPlus,
        new Terminal(opPC),
        Location::memOf(new Binary(opPlus,
            new Terminal(opPC),
            new Binary(opMinus,
                new Binary(opMult,
                    new Terminal(opWild),
                    new Const(4)),
                new Terminal(opWildIntConst))))),
    new Terminal(opWildIntConst));

static Exp *hlForms[] = { forma, formA, formO, formR, formr };
static char chForms[] = {   'a',   'A',   'O',   'R',   'r' };

#ifdef GARBAGE_COLLECTOR
void
init_basicblock()
{
	Exp **gc_pointers = (Exp **)GC_MALLOC_UNCOLLECTABLE(5 * sizeof *gc_pointers);
	gc_pointers[0] = forma;
	gc_pointers[1] = formA;
	gc_pointers[2] = formO;
	gc_pointers[3] = formR;
	gc_pointers[4] = formr;
}
#endif

/**
 * Vcall high level patterns
 *
 * Pattern 0: global\<wild\>[0]
 */
static Binary *vfc_funcptr = new Binary(opArrayIndex,
    new Location(opGlobal,
        new Terminal(opWildStrConst), nullptr),
    new Const(0));

/**
 * Pattern 1: m[ m[ \<expr\> + K1 ] + K2 ]
 *
 * K1 is vtable offset, K2 is virtual function offset (could come from m[A2],
 * if A2 is in read-only memory).
 */
static Location *vfc_both = Location::memOf(
    new Binary(opPlus,
        Location::memOf(
            new Binary(opPlus,
                new Terminal(opWild),
                new Terminal(opWildIntConst))),
        new Terminal(opWildIntConst)));

/**
 * Pattern 2: m[ m[ \<expr\> ] + K2 ]
 */
static Location *vfc_vto = Location::memOf(
    new Binary(opPlus,
        new Terminal(opWildMemOf),
        new Terminal(opWildIntConst)));

/**
 * Pattern 3: m[ m[ \<expr\> + K1 ] ]
 */
static Location *vfc_vfo = Location::memOf(
    Location::memOf(
        new Binary(opPlus,
            new Terminal(opWild),
            new Terminal(opWildIntConst))));

/**
 * Pattern 4: m[ m[ \<expr\> ] ]
 */
static Location *vfc_none = Location::memOf(
    new Terminal(opWildMemOf));

static Exp *hlVfc[] = { vfc_funcptr, vfc_both, vfc_vto, vfc_vfo, vfc_none };

/**
 * Extract information from a switch's destination Exp.  Supports the switch
 * forms in hlForms/chForms; select the form beforehand using si.chForm.
 * Populates si.uTable and si.iOffset.  Returns the table index Exp.
 */
static Exp *
findSwParams(Exp *e, SWITCH_INFO &si)
{
	Exp *expr;
	ADDRESS T;
	si.iOffset = 0;
	switch (si.chForm) {
	case 'a':  // Pattern: <base>{}[<index>]{}
		{
			e = ((RefExp *)e)->getSubExp1();
			Exp *base = ((Binary *)e)->getSubExp1();
			if (auto re = dynamic_cast<RefExp *>(base))
				base = re->getSubExp1();
			Exp *con = ((Location *)base)->getSubExp1();
			const char *gloName = ((Const *)con)->getStr();
			UserProc *p = ((Location *)base)->getProc();
			Prog *prog = p->getProg();
			T = prog->getGlobalAddr(gloName);
			expr = ((Binary *)e)->getSubExp2();
		}
		break;
	case 'O':  // Pattern: m[<expr> * 4 + T ] + T
		// Assume T's are equal, discard 2nd T
		e = ((Binary *)e)->getSubExp1();
		// Fall through to 'A'
	case 'A':  // Pattern: m[<expr> * 4 + T ]
		{
			if (auto re = dynamic_cast<RefExp *>(e)) e = re->getSubExp1();
			// b will be (<expr> * 4) + T
			Binary *b = (Binary *)((Location *)e)->getSubExp1();
			Const *TT = (Const *)b->getSubExp2();
			T = (ADDRESS)TT->getInt();
			b = (Binary *)b->getSubExp1();  // b is now <expr> * 4
			expr = b->getSubExp1();
		}
		break;
	case 'R':  // Pattern: %pc + m[%pc + (<expr> * 4) + k]
		{
			// Was: uTable = <RTL's native address> + 8 (possibly SPARC-specific)
			T = 0;  // ?
			// l = m[%pc  + (<expr> * 4) + k]:
			Exp *l = ((Binary *)e)->getSubExp2();
			if (auto re = dynamic_cast<RefExp *>(l)) l = re->getSubExp1();
			// b = %pc + (<expr> * 4) + k:
			Binary *b = (Binary *)((Location *)l)->getSubExp1();
			// b = (<expr> * 4) + k:
			b = (Binary *)b->getSubExp2();
			Const *k = (Const *)b->getSubExp2();
			si.iOffset = k->getInt();
			// b = <expr> * 4:
			b = (Binary *)b->getSubExp1();
			// expr = <expr>:
			expr = b->getSubExp1();
		}
		break;
	case 'r':  // Pattern: %pc + m[%pc + ((<expr> * 4) - k)] - k
		// TODO: Check associativity, was %pc + (m[] - k), now (%pc + m[]) - k
		{
			// Was: uTable = uCopyPC - k, where both k's are equal
			// and uCopyPC is address of an RTL with %pc on the RHS of an assignment
			T = 0;  // ?
			// b = %pc + m[%pc + ((<expr> * 4) - k)]:
			Binary *b = (Binary *)((Binary *)e)->getSubExp1();
			// l = m[%pc + ((<expr> * 4) - k)]:
			Exp *l = b->getSubExp2();
			if (auto re = dynamic_cast<RefExp *>(l)) l = re->getSubExp1();
			// b = %pc + ((<expr> * 4) - k)
			b = (Binary *)((Location *)l)->getSubExp1();
			// b = ((<expr> * 4) - k):
			b = (Binary *)b->getSubExp2();
			// b = <expr> * 4:
			b = (Binary *)b->getSubExp1();
			// expr = <expr>
			expr = b->getSubExp1();
		}
		break;
	default:
		expr = nullptr;
		T = NO_ADDRESS;
		break;
	}
	si.uTable = T;
	return expr;
}

/**
 * Find the number of cases for this switch statement.  Assumes that there is
 * a compare and branch around the indirect branch.  Note: fails
 * test/sparc/switchAnd_cc because of the and instruction, and the compare
 * that is outside is not the compare for the upper bound.  Note that you CAN
 * have an and and still a test for an upper bound. So this needs tightening.
 *
 * TMN: It also needs to check for and handle the double indirect case; where
 * there is one array (of e.g. ubyte) that is indexed by the actual switch
 * value, then the value from that array is used as an index into the array of
 * code pointers.
 */
int
BasicBlock::findNumCases()
{
	for (const auto &pred : m_InEdges) {
		if (pred->m_nodeType != TWOWAY)  // look for a two-way BB
			continue;  // Ignore all others
		assert(!pred->m_pRtls->empty());
		const auto &stmts = pred->m_pRtls->back()->getList();
		assert(!stmts.empty());
		auto lastStmt = (BranchStatement *)stmts.back();
		Exp *pCond = lastStmt->getCondExpr();
		if (pCond->getArity() != 2) continue;
		Exp *rhs = ((Binary *)pCond)->getSubExp2();
		if (!rhs->isIntConst()) continue;
		int k = ((Const *)rhs)->getInt();
		OPER op = pCond->getOper();
		if (op == opGtr || op == opGtrUns)
			return k + 1;
		if (op == opGtrEq || op == opGtrEqUns)
			return k;
		if (op == opLess || op == opLessUns)
			return k;
		if (op == opLessEq || op == opLessEqUns)
			return k + 1;
	}
	LOG << "Could not find number of cases for n-way at address 0x" << std::hex << getLowAddr() << std::dec << "\n";
	return 3;   // Bald faced guess if all else fails
}

/**
 * Find all the possible constant values that the location defined by s could
 * be assigned with.
 */
static void
findConstantValues(Statement *s, std::vector<ADDRESS> &dests)
{
	if (auto pa = dynamic_cast<PhiAssign *>(s)) {
		// For each definition, recurse
		for (const auto &def : *pa)
			findConstantValues(def.def, dests);
		return;
	}
	if (auto as = dynamic_cast<Assign *>(s)) {
		Exp *rhs = as->getRight();
		if (rhs->isIntConst())
			dests.push_back(((Const *)rhs)->getInt());
		return;
	}
}

/**
 * \brief Find indirect jumps and calls.
 *
 * Find any BBs of type COMPJUMP or COMPCALL.  If found, analyse, and if
 * possible decode extra code and return true.
 */
bool
BasicBlock::decodeIndirectJmp(UserProc *proc)
{
#define CHECK_REAL_PHI_LOOPS 0
#if CHECK_REAL_PHI_LOOPS
	rtlit rit;
	stlit sit;
	for (Statement *s = getFirstStmt(rit, sit); s; s = getNextStmt(rit, sit)) {
		if (!dynamic_cast<PhiAssign *>(s)) continue;
		Statement *originalPhi = s;
		StatementSet workSet, seenSet;
		workSet.insert(s);
		seenSet.insert(s);
		do {
			PhiAssign *pi = (PhiAssign *)*workSet.begin();
			workSet.remove(pi);
			for (const auto &def : *pi) {
				if (!dynamic_cast<PhiAssign *>(def.def)) continue;
				if (seenSet.exists(def.def)) {
					std::cerr << "Real phi loop involving statements " << originalPhi->getNumber() << " and " << pi->getNumber() << "\n";
					break;
				} else {
					workSet.insert(def.def);
					seenSet.insert(def.def);
				}
			}
		} while (!workSet.empty());
	}
#endif

	if (m_nodeType == COMPJUMP) {
		assert(!m_pRtls->empty());
		if (DEBUG_SWITCH)
			LOG << "decodeIndirectJmp: " << *m_pRtls->back();
		const auto &stmts = m_pRtls->back()->getList();
		assert(!stmts.empty());
		auto lastStmt = (CaseStatement *)stmts.back();
		// Note: some programs might not have the case expression propagated to, because of the -l switch (?)
		// We used to use ordinary propagation here to get the memory expression, but now it refuses to propagate memofs
		// because of the alias safety issue. Eventually, we should use an alias-safe incremental propagation, but for
		// now we'll assume no alias problems and force the propagation
		bool convert;
		lastStmt->propagateTo(convert, nullptr, nullptr, true /* force */);
		Exp *e = lastStmt->getDest();
		char form = 0;
		int n = sizeof hlForms / sizeof *hlForms;
		for (int i = 0; i < n; ++i) {
			if (*e *= *hlForms[i]) {  // *= compare ignores subscripts
				form = chForms[i];
				if (DEBUG_SWITCH)
					LOG << "indirect jump matches form " << form << "\n";
				break;
			}
		}
		if (form) {
			auto swi = new SWITCH_INFO;
			swi->chForm = form;
			if (auto expr = findSwParams(e, *swi)) {
				swi->iNumTable = findNumCases();
#if 1
				// TMN: Added actual control of the array members, to possibly truncate what findNumCases()
				// thinks is the number of cases, when finding the first array element not pointing to code.
				if (form == 'A') {
					Prog *prog = proc->getProg();
					for (int iPtr = 0; iPtr < swi->iNumTable; ++iPtr) {
						ADDRESS uSwitch = prog->readNative4(swi->uTable + iPtr * 4);
						if (uSwitch >= prog->getLimitTextHigh()
						 || uSwitch <  prog->getLimitTextLow()) {
							if (DEBUG_SWITCH)
								LOG << "Truncating type A indirect jump array to " << iPtr << " entries "
								       "due to finding an array entry pointing outside valid code\n";
							// Found an array that isn't a pointer-to-code. Assume array has ended.
							swi->iNumTable = iPtr;
							break;
						}
					}
				}
				assert(swi->iNumTable > 0);
#endif
				swi->iLower = 0;
				swi->iUpper = swi->iNumTable - 1;
				if (expr->getOper() == opMinus && ((Binary *)expr)->getSubExp2()->isIntConst()) {
					swi->iLower = ((Const *)((Binary *)expr)->getSubExp2())->getInt();
					swi->iUpper += swi->iLower;
					expr = ((Binary *)expr)->getSubExp1();
				}
				swi->pSwitchVar = expr;
				lastStmt->setSwitchInfo(swi);
				return swi->iNumTable != 0;
			}
		} else {
			// Did not match a switch pattern. Perhaps it is a Fortran style goto with constants at the leaves of the
			// phi tree. Basically, a location with a reference, e.g. m[r28{-} - 16]{87}
			if (auto re = dynamic_cast<RefExp *>(e)) {
				if (dynamic_cast<Location *>(re->getSubExp1())) {
					// Yes, we have <location>{ref}. Follow the tree and store the constant values that <location>
					// could be assigned to in swi->dests
					auto swi = new SWITCH_INFO;
					swi->chForm = 'F';  // The "Fortran" form
					findConstantValues(re->getDef(), swi->dests);
					if (!swi->dests.empty()) {
						swi->pSwitchVar = re;
						swi->iNumTable = swi->dests.size();
						swi->iLower = 1;               // Not used, except to compute
						swi->iUpper = swi->iNumTable;  // the number of options
						swi->uTable = NO_ADDRESS;      // or for debug output
						lastStmt->setSwitchInfo(swi);
						return true;
					}
				}
			}
		}
	} else if (m_nodeType == COMPCALL) {
		assert(!m_pRtls->empty());
		if (DEBUG_SWITCH)
			LOG << "decodeIndirectJmp: COMPCALL:\n" << *m_pRtls->back() << "\n";
		const auto &stmts = m_pRtls->back()->getList();
		assert(!stmts.empty());
		auto lastStmt = (CallStatement *)stmts.back();
		Exp *e = lastStmt->getDest();
		// Indirect calls may sometimes not be propagated to, because of limited propagation (-l switch).
		// Propagate to e, but only keep the changes if the expression matches (don't want excessive propagation to
		// a genuine function pointer expression, even though it's hard to imagine).
		e = e->propagateAll();
		// We also want to replace any m[K]{-} with the actual constant from the (presumably) read-only data section
		ConstGlobalConverter cgc(proc->getProg());
		e = e->accept(cgc);
		// Simplify the result, e.g. for m[m[(r24{16} + m[0x8048d74]{-}) + 12]{-}]{-} get
		// m[m[(r24{16} + 20) + 12]{-}]{-}, want m[m[r24{16} + 32]{-}]{-}. Note also that making the
		// ConstGlobalConverter a simplifying expression modifier won't work in this case, since the simplifying
		// converter will only simplify the direct parent of the changed expression (which is r24{16} + 20).
		e = e->simplify();
		if (DEBUG_SWITCH)
			LOG << "decodeIndirect: propagated and const global converted call expression is " << *e << "\n";

		bool recognised = false;
		int i, n = sizeof hlVfc / sizeof *hlVfc;
		for (i = 0; i < n; ++i) {
			if (*e *= *hlVfc[i]) {  // *= compare ignores subscripts
				recognised = true;
				if (DEBUG_SWITCH)
					LOG << "indirect call matches form " << i << "\n";
				break;
			}
		}
		if (!recognised) return false;
		lastStmt->setDest(e);  // Keep the changes to the indirect call expression
		int K1, K2;
		Exp *vtExp, *t1;
		Prog *prog = proc->getProg();
		switch (i) {
		case 0:
			{
				// This is basically an indirection on a global function pointer.  If it is initialised, we have a
				// decodable entry point.  Note: it could also be a library function (e.g. Windows)
				// Pattern 0: global<name>{0}[0]{0}
				K2 = 0;
				if (auto re = dynamic_cast<RefExp *>(e)) e = re->getSubExp1();
				e = ((Binary *)e)->getSubExp1();  // e is global<name>{0}[0]
				if (e->isArrayIndex()
				 && (t1 = ((Binary *)e)->getSubExp2(), t1->isIntConst())
				 && ((Const *)t1)->getInt() == 0)
					e = ((Binary *)e)->getSubExp1();            // e is global<name>{0}
				if (auto re = dynamic_cast<RefExp *>(e)) e = re->getSubExp1();  // e is global<name>
				Const *con = (Const *)((Location *)e)->getSubExp1(); // e is <name>
				Global *glo = prog->getGlobal(con->getStr());
				assert(glo);
				// Set the type to pointer to function, if not already
				Type *ty = glo->getType();
				if (!ty->isPointer() && !((PointerType *)ty)->getPointsTo()->isFunc())
					glo->setType(new PointerType(new FuncType()));
				ADDRESS addr = glo->getAddress();
				// FIXME: not sure how to find K1 from here. I think we need to find the earliest(?) entry in the data
				// map that overlaps with addr
				// For now, let K1 = 0:
				K1 = 0;
				vtExp = new Const(addr);
			}
			break;
		case 1:
			{
				// Example pattern: e = m[m[r27{25} + 8]{-} + 8]{-}
				if (auto re = dynamic_cast<RefExp *>(e))
					e = re->getSubExp1();
				e = ((Location *)e)->getSubExp1();       // e = m[r27{25} + 8]{-} + 8
				Exp *rhs = ((Binary *)e)->getSubExp2();  // rhs = 8
				K2 = ((Const *)rhs)->getInt();
				Exp *lhs = ((Binary *)e)->getSubExp1();  // lhs = m[r27{25} + 8]{-}
				if (auto re = dynamic_cast<RefExp *>(lhs))
					lhs = re->getSubExp1(); // lhs = m[r27{25} + 8]
				vtExp = lhs;
				lhs = ((Unary *)lhs)->getSubExp1();      // lhs =   r27{25} + 8
				//Exp *object = ((Binary *)lhs)->getSubExp1();
				Exp *CK1 = ((Binary *)lhs)->getSubExp2();
				K1 = ((Const *)CK1)->getInt();
			}
			break;
		case 2:
			{
				// Example pattern: e = m[m[r27{25}]{-} + 8]{-}
				if (auto re = dynamic_cast<RefExp *>(e))
					e = re->getSubExp1();
				e = ((Location *)e)->getSubExp1();       // e = m[r27{25}]{-} + 8
				Exp *rhs = ((Binary *)e)->getSubExp2();  // rhs = 8
				K2 = ((Const *)rhs)->getInt();
				Exp *lhs = ((Binary *)e)->getSubExp1();  // lhs = m[r27{25}]{-}
				if (auto re = dynamic_cast<RefExp *>(lhs))
					lhs = re->getSubExp1(); // lhs = m[r27{25}]
				vtExp = lhs;
				K1 = 0;
			}
			break;
		case 3:
			{
				// Example pattern: e = m[m[r27{25} + 8]{-}]{-}
				if (auto re = dynamic_cast<RefExp *>(e))
					e = re->getSubExp1();
				e = ((Location *)e)->getSubExp1();          // e = m[r27{25} + 8]{-}
				K2 = 0;
				if (auto re = dynamic_cast<RefExp *>(e))
					e = re->getSubExp1();        // e = m[r27{25} + 8]
				vtExp = e;
				Exp *lhs = ((Unary *)e)->getSubExp1();    // lhs =   r27{25} + 8
				// Exp *object = ((Binary *)lhs)->getSubExp1();
				Exp *CK1 = ((Binary *)lhs)->getSubExp2();
				K1 = ((Const *)CK1)->getInt();
			}
			break;
		case 4:
			{
				// Example pattern: e = m[m[r27{25}]{-}]{-}
				if (auto re = dynamic_cast<RefExp *>(e))
					e = re->getSubExp1();
				e = ((Location *)e)->getSubExp1();    // e = m[r27{25}]{-}
				K2 = 0;
				if (auto re = dynamic_cast<RefExp *>(e))
					e = re->getSubExp1();  // e = m[r27{25}]
				vtExp = e;
				K1 = 0;
				// Exp *object = ((Unary *)e)->getSubExp1();
			}
			break;
		default:
			K1 = K2 = -1;  // Suppress warnings
			vtExp = (Exp *)-1;
		}
		if (DEBUG_SWITCH)
			LOG << "form " << i << ": from statement " << lastStmt->getNumber() << " get e = " << *lastStmt->getDest()
			    << ", K1 = " << K1 << ", K2 = " << K2 << ", vtExp = " << *vtExp << "\n";
		// The vt expression might not be a constant yet, because of expressions not fully propagated, or because of
		// m[K] in the expression (fixed with the ConstGlobalConverter).  If so, look it up in the defCollector in the
		// call
		vtExp = lastStmt->findDefFor(vtExp);
		if (vtExp && DEBUG_SWITCH)
			LOG << "VT expression boils down to this: " << *vtExp << "\n";

		// Danger. For now, only do if -ic given
		bool decodeThru = Boomerang::get().decodeThruIndCall;
		if (decodeThru && vtExp && vtExp->isIntConst()) {
			int addr = ((Const *)vtExp)->getInt();
			ADDRESS pfunc = prog->readNative4(addr);
			if (!prog->findProc(pfunc)) {
				// A new, undecoded procedure
				if (Boomerang::get().noDecodeChildren)
					return false;
				prog->decodeEntryPoint(pfunc);
				// Since this was not decoded, this is a significant change, and we want to redecode the current
				// function now that the callee has been decoded
				return true;
			}
		}
	}
	return false;
}

/**
 * Change the BB enclosing stmt from type COMPCALL to CALL
 */
bool
BasicBlock::undoComputedBB(Statement *stmt)
{
	const auto &stmts = m_pRtls->back()->getList();
	for (auto srit = stmts.crbegin(); srit != stmts.crend(); ++srit) {
		if (*srit == stmt) {
			m_nodeType = CALL;
			LOG << "undoComputedBB for statement " << *stmt << "\n";
			return true;
		}
	}
	return false;
}
