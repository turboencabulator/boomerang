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
 * \brief Copy constructor.
 * \param bb  The BB to copy from.
 */
BasicBlock::BasicBlock(const BasicBlock &bb) :
	m_structType(bb.m_structType),
	m_loopCondType(bb.m_loopCondType),
	m_loopHead(bb.m_loopHead),
	m_caseHead(bb.m_caseHead),
	m_condFollow(bb.m_condFollow),
	m_loopFollow(bb.m_loopFollow),
	m_latchNode(bb.m_latchNode),
	m_nodeType(bb.m_nodeType),
	m_iLabelNum(bb.m_iLabelNum),
	m_bIncomplete(bb.m_bIncomplete),
	m_bJumpReqd(bb.m_bJumpReqd),
	m_InEdges(bb.m_InEdges),
	m_OutEdges(bb.m_OutEdges),
	m_iNumOutEdges(bb.m_iNumOutEdges),
// From Doug's code
	ord(bb.ord),
	revOrd(bb.revOrd),
	inEdgesVisited(bb.inEdgesVisited),
	numForwardInEdges(bb.numForwardInEdges),
	traversed(bb.traversed),
	hllLabel(bb.hllLabel),
	indentLevel(bb.indentLevel),
	immPDom(bb.immPDom),
	loopHead(bb.loopHead),
	caseHead(bb.caseHead),
	condFollow(bb.condFollow),
	loopFollow(bb.loopFollow),
	latchNode(bb.latchNode),
	sType(bb.sType),
	usType(bb.usType)
{
	setRTLs(bb.m_pRtls);
}

/**
 * \brief Private constructor.  Called by Cfg::newBB.
 * \param pRtls
 * \param bbType
 * \param iNumOutEdges
 */
BasicBlock::BasicBlock(std::list<RTL *> *pRtls, BBTYPE bbType, int iNumOutEdges) :
	m_nodeType(bbType),
	m_bIncomplete(false),
	m_iNumOutEdges(iNumOutEdges)
{
	assert(bbType != ONEWAY   || iNumOutEdges == 1);
	assert(bbType != TWOWAY   || iNumOutEdges == 2);
	assert(bbType != CALL     || iNumOutEdges <= 1);
	assert(bbType != RET      || iNumOutEdges == 0);
	assert(bbType != FALL     || iNumOutEdges == 1);
	assert(bbType != COMPJUMP || iNumOutEdges == 0);
	assert(bbType != COMPCALL || iNumOutEdges == 1);
	assert(bbType != INVALID  || iNumOutEdges == 0);

	m_OutEdges.reserve(iNumOutEdges);  // Reserve the space; values added with Cfg::addOutEdge()

	// Set the RTLs
	setRTLs(pRtls);
}

/**
 * \brief Returns nonzero if this BB has a label, in the sense that a label is
 * required in the translated source code.
 *
 * Check if this BB has a label. If so, return the numeric value of the label
 * (nonzero integer).  If not, returns zero.  See also Cfg::setLabel().
 *
 * \returns  An integer unique to this BB, or zero.
 */
int
BasicBlock::getLabel() const
{
	return m_iLabelNum;
}

bool
BasicBlock::isCaseOption() const
{
	if (caseHead)
		for (const auto &edge : caseHead->m_OutEdges)
			if (edge == this)
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
 * Update the type and number of out edges.  Used for example where a COMPJUMP
 * type is updated to an NWAY when a switch idiom is discovered.
 *
 * \param[in] bbType        The new type.
 * \param[in] iNumOutEdges  New number of outedges.
 */
void
BasicBlock::updateType(BBTYPE bbType, int iNumOutEdges)
{
	assert(bbType != ONEWAY   || iNumOutEdges == 1);
	assert(bbType != TWOWAY   || iNumOutEdges == 2);
	assert(bbType != CALL     || iNumOutEdges <= 1);
	assert(bbType != RET      || iNumOutEdges == 0);
	assert(bbType != FALL     || iNumOutEdges == 1);
	assert(bbType != COMPJUMP || iNumOutEdges == 0);
	assert(bbType != COMPCALL || iNumOutEdges == 1);
	assert(bbType != INVALID  || iNumOutEdges == 0);

	m_nodeType = bbType;
	m_iNumOutEdges = iNumOutEdges;
	//m_OutEdges.resize(iNumOutEdges);
}

/**
 * Sets the "jump required" bit.  This means that this BB is an orphan (not
 * generated from input code, not part of the original program), and that the
 * "fall through" out edge (m_OutEdges[1]) needs to be implemented as a jump.
 * The back end needs to take heed of this bit.
 */
void
BasicBlock::setJumpReqd()
{
	m_bJumpReqd = true;
}

/**
 * \brief Check if jump is required (see above).
 *
 * Returns the "jump required" bit.  See above for details.
 *
 * \returns  true if a jump is required.
 */
bool
BasicBlock::isJumpReqd() const
{
	return m_bJumpReqd;
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
	if (m_iLabelNum) os << "L" << m_iLabelNum << ": ";
	switch (m_nodeType) {
	case ONEWAY:    os << "Oneway BB";        break;
	case TWOWAY:    os << "Twoway BB";        break;
	case NWAY:      os << "Nway BB";          break;
	case CALL:      os << "Call BB";          break;
	case RET:       os << "Ret BB";           break;
	case FALL:      os << "Fall BB";          break;
	case COMPJUMP:  os << "Computed jump BB"; break;
	case COMPCALL:  os << "Computed call BB"; break;
	case INVALID:   os << "Invalid BB";       break;
	}
	os << ":\n";
	os << "in edges:" << std::hex;
	for (const auto &edge : m_InEdges)
		os << " " << edge->getHiAddr();
	os << std::dec << "\n";
	os << "out edges:" << std::hex;
	for (const auto &edge : m_OutEdges)
		os << " " << edge->getLowAddr();
	os << std::dec << "\n";
	if (m_pRtls) {  // Can be zero if e.g. INVALID
		if (html)
			os << "<table>\n";
		for (const auto &rtl : *m_pRtls)
			rtl->print(os, html);
		if (html)
			os << "</table>\n";
	}
	if (m_bJumpReqd) {
		if (html)
			os << "<br>";
		os << "Synthetic out edge(s) to";
		for (int i = 0; i < m_iNumOutEdges; ++i) {
			const auto &outEdge = m_OutEdges[i];
			if (outEdge && outEdge->m_iLabelNum)
				os << " L" << outEdge->m_iLabelNum;
		}
		os << "\n";
	}
}

bool
BasicBlock::isBackEdge(int inEdge) const
{
	const auto &in = m_InEdges[inEdge];
	return this == in || (m_DFTfirst < in->m_DFTfirst && m_DFTlast > in->m_DFTlast);
}

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
std::vector<BasicBlock *> &
BasicBlock::getInEdges()
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
std::vector<BasicBlock *> &
BasicBlock::getOutEdges()
{
	return m_OutEdges;
}

/**
 * \brief Set an in edge to a new value; same number of in edges as before.
 *
 * Change the given in-edge (0 is first) to the given value.  Needed for
 * example when duplicating BBs.
 *
 * \param i           Index (0 based) of in-edge to change.
 * \param pNewInEdge  Pointer to BB that will be a new parent.
 */
void
BasicBlock::setInEdge(int i, BasicBlock *pNewInEdge)
{
	m_InEdges[i] = pNewInEdge;
}

/**
 * \brief Set an out edge to a new value; same number of out edges as before.
 *
 * Change the given out-edge (0 is first) to the given value.  Needed for
 * example when duplicating BBs.
 *
 * \note Cannot add an additional out-edge with this function; use addOutEdge
 * for this rare case.
 *
 * \param i            Index (0 based) of out-edge to change.
 * \param pNewOutEdge  Pointer to BB that will be the new successor.
 */
void
BasicBlock::setOutEdge(int i, BasicBlock *pNewOutEdge)
{
	if (m_OutEdges.empty()) {
		assert(i == 0);
		m_OutEdges.push_back(pNewOutEdge);
	} else {
		assert(i < (int)m_OutEdges.size());
		m_OutEdges[i] = pNewOutEdge;
	}
}

/**
 * \brief Get the n-th out edge, or 0 if it does not exist.
 *
 * Returns the i-th out edge of this BB; counting starts at 0.
 *
 * \param i  Index (0 based) of the desired out edge.
 * \returns  The i-th out edge; 0 if there is no such out edge.
 */
BasicBlock *
BasicBlock::getOutEdge(unsigned int i)
{
	if (i < m_OutEdges.size())
		return m_OutEdges[i];
	return nullptr;
}

/**
 * Given an address, returns the outedge which corresponds to that address or
 * 0 if there was no such outedge.
 *
 * \param a  The address.
 */
BasicBlock *
BasicBlock::getCorrectOutEdge(ADDRESS a) const
{
	for (const auto &edge : m_OutEdges)
		if (edge->getLowAddr() == a)
			return edge;
	return nullptr;
}

/**
 * \brief Add an in-edge.
 *
 * Add the given in-edge.  Needed for example when duplicating BBs.
 *
 * \param pNewInEdge  Pointer to BB that will be a new parent.
 */
void
BasicBlock::addInEdge(BasicBlock *pNewInEdge)
{
	m_InEdges.push_back(pNewInEdge);
}

/**
 * \brief Delete an in-edge.
 *
 * Delete the in-edge from the given BB.  Needed for example when duplicating
 * BBs.
 *
 * \param it  Iterator to BB that will no longer be a parent.
 *
 * \par Side Effects
 * The iterator argument is incremented.
 *
 * \par Usage
 * It should be used like this:
 *     if (pred) deleteInEdge(it) else ++it;
 */
void
BasicBlock::deleteInEdge(std::vector<BasicBlock *>::iterator &it)
{
	it = m_InEdges.erase(it);
}

void
BasicBlock::deleteInEdge(BasicBlock *edge)
{
	for (auto it = m_InEdges.begin(); it != m_InEdges.end(); ++it) {
		if (*it == edge) {
			m_InEdges.erase(it);
			break;
		}
	}
}

void
BasicBlock::deleteEdge(BasicBlock *edge)
{
	edge->deleteInEdge(this);
	for (auto it = m_OutEdges.begin(); it != m_OutEdges.end(); ++it) {
		if (*it == edge) {
			m_OutEdges.erase(it);
			--m_iNumOutEdges;
			break;
		}
	}
}

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
	++first;
	m_DFTfirst = first;

	unsigned numTraversed = 1;
	m_iTraversed = true;

	for (const auto &child : m_OutEdges) {
		if (!child->m_iTraversed)
			numTraversed += child->DFTOrder(first, last);
	}

	++last;
	m_DFTlast = last;

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
	++first;
	m_DFTrevfirst = first;

	unsigned numTraversed = 1;
	m_iTraversed = true;

	for (const auto &parent : m_InEdges) {
		if (!parent->m_iTraversed)
			numTraversed += parent->RevDFTOrder(first, last);
	}

	++last;
	m_DFTrevlast = last;

	return numTraversed;
}

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
	assert(m_iNumOutEdges == 2);
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
			m_iNumOutEdges = 1;
			if (VERBOSE)
				LOG << "redundant edge to 0x" << std::hex << redundant->getLowAddr() << std::dec << " inedges: ";
			auto rinedges = std::vector<BasicBlock *>();
			rinedges.swap(redundant->m_InEdges);
			for (const auto &edge : rinedges) {
				if (VERBOSE)
					LOG << "0x" << std::hex << edge->getLowAddr() << std::dec << " ";
				if (edge != this)
					redundant->m_InEdges.push_back(edge);
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
			m_iNumOutEdges = 1;
			if (VERBOSE)
				LOG << "redundant edge to 0x" << std::hex << redundant->getLowAddr() << std::dec << " inedges: ";
			auto rinedges = std::vector<BasicBlock *>();
			rinedges.swap(redundant->m_InEdges);
			for (const auto &edge : rinedges) {
				if (VERBOSE)
					LOG << "0x" << std::hex << edge->getLowAddr() << std::dec << " ";
				if (edge != this)
					redundant->m_InEdges.push_back(edge);
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
	for (const auto &edge : m_InEdges)
		if (!edge->hasBackEdgeTo(this)
		 &&  edge->traversed != DFS_CODEGEN)
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

	BasicBlock *child = nullptr;
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
				cond = new Unary(opNot, cond);
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
					cond = new Unary(opNot, cond->clone());
					cond = cond->simplify();
				}
				if (cType == IfThenElse)
					hll->AddIfElseCondHeader(indLevel, cond);
				else
					hll->AddIfCondHeader(indLevel, cond);
			}

			// write code for the body of the conditional
			if (cType != Case) {
				BasicBlock *succ = (cType == IfElse ? m_OutEdges[BELSE] : m_OutEdges[BTHEN]);

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
						caseVal.setInt(((int *)psi->uTable)[i]);  // Yes, use the table value itself
						                                          // Note that uTable has the address of an int array
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

		child = m_OutEdges[0];
		if (m_OutEdges.size() != 1) {
			BasicBlock *other = m_OutEdges[1];
			LOG << "found seq with more than one outedge!\n";
			if (getDest()->isIntConst()
			 && ((Const *)getDest())->getInt() == (int)child->getLowAddr()) {
				other = child;
				child = m_OutEdges[1];
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
		if (child->traversed == DFS_CODEGEN
		 || ((child->loopHead != loopHead) && (!child->allParentsGenerated() || isIn(followSet, child)))
		 || (latch && latch->loopHead && latch->loopHead->loopFollow == child)
		 || !(caseHead == child->caseHead || (caseHead && child == caseHead->condFollow)))
			emitGotoAndLabel(hll, indLevel, child);
		else {
			if (caseHead && child == caseHead->condFollow) {
				// generate the 'break' statement
				hll->AddCaseCondOptionEnd(indLevel);
			} else if (!caseHead || caseHead != child->caseHead || !child->isCaseOption())
				child->generateCode(hll, indLevel, latch, followSet, gotoSet, proc);
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
	loopStamps[0] = time;

	// recurse on unvisited children and set inedges for all children
	for (const auto &edge : m_OutEdges) {
		// set the in edge from this child to its parent (the current node)
		// (not done here, might be a problem)
		// outEdges[i]->inEdges.Add(this);

		// recurse on this child if it hasn't already been visited
		if (edge->traversed != DFS_LNUM)
			edge->setLoopStamps(++time, order);
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
	revLoopStamps[0] = time;

	// recurse on the unvisited children in reverse order
	for (auto rit = m_OutEdges.rbegin(); rit != m_OutEdges.rend(); ++rit) {
		const auto &edge = *rit;
		// recurse on this child if it hasn't already been visited
		if (edge->traversed != DFS_RNUM)
			edge->setRevLoopStamps(++time);
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
	for (const auto &edge : m_InEdges)
		if (edge->traversed != DFS_PDOM)
			edge->setRevOrder(order);

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
		for (const auto &edge : m_OutEdges)
			if (!hasBackEdgeTo(edge)
			 && edge->traversed != DFS_CASE
			 && edge != follow)
				edge->setCaseHead(head, follow);
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
		if (!u->isSubscript()) continue;  // Only interested in subscripted vars
		RefExp *r = (RefExp *)u;
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
		liveLocs.insert(u);
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
	for (const auto &currBB : m_OutEdges) {
		// First add the non-phi liveness
		liveout.makeUnion(currBB->liveIn);
		int j = currBB->whichPred(this);
		// The first RTL will have the phi functions, if any
		if (!currBB->m_pRtls || currBB->m_pRtls->empty())
			continue;
		const auto &stmts = currBB->m_pRtls->front()->getList();
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
 * With array processing, we get a new form, call it form 'o' (don't confuse
 * with form 'O'):
 *
 * Pattern: \<base\>{}[\<index\>]{}
 *
 * where \<index\> could be \<var\> - \<Kmin\>
 *
 * NOT COMPLETED YET!
 */
static Exp *formo = new RefExp(
    new Binary(opArrayIndex,
        new RefExp(
            new Terminal(opWild),
            (Statement *)-1),
        new Terminal(opWild)),
    (Statement *)-1);

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
static Exp *formr = new Binary(opPlus,
    new Terminal(opPC),
    Location::memOf(new Binary(opPlus,
        new Terminal(opPC),
        new Binary(opMinus,
            new Binary(opMult,
                new Terminal(opWild),
                new Const(4)),
            new Terminal(opWildIntConst)))));

static Exp *hlForms[] = { forma, formA, formo, formO, formR, formr };
static char chForms[] = {   'a',   'A',   'o',   'O',   'R',   'r' };

#ifdef GARBAGE_COLLECTOR
void
init_basicblock()
{
	Exp **gc_pointers = (Exp **)GC_MALLOC_UNCOLLECTABLE(6 * sizeof *gc_pointers);
	gc_pointers[0] = forma;
	gc_pointers[1] = formA;
	gc_pointers[2] = formo;
	gc_pointers[3] = formO;
	gc_pointers[4] = formR;
	gc_pointers[5] = formr;
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
        Location::memOf(
            new Terminal(opWild)),
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
    Location::memOf(
        new Terminal(opWild)));

static Exp *hlVfc[] = { vfc_funcptr, vfc_both, vfc_vto, vfc_vfo, vfc_none };

static void
findSwParams(char form, Exp *e, Exp *&expr, ADDRESS &T)
{
	switch (form) {
	case 'a':
		{
			// Pattern: <base>{}[<index>]{}
			e = ((RefExp *)e)->getSubExp1();
			Exp *base = ((Binary *)e)->getSubExp1();
			if (base->isSubscript())
				base = ((RefExp *)base)->getSubExp1();
			Exp *con = ((Location *)base)->getSubExp1();
			const char *gloName = ((Const *)con)->getStr();
			UserProc *p = ((Location *)base)->getProc();
			Prog *prog = p->getProg();
			T = prog->getGlobalAddr(gloName);
			expr = ((Binary *)e)->getSubExp2();
		}
		break;
	case 'A':
		{
			// Pattern: m[<expr> * 4 + T ]
			if (e->isSubscript()) e = e->getSubExp1();
			// b will be (<expr> * 4) + T
			Binary *b = (Binary *)((Location *)e)->getSubExp1();
			Const *TT = (Const *)b->getSubExp2();
			T = (ADDRESS)TT->getInt();
			b = (Binary *)b->getSubExp1();  // b is now <expr> * 4
			expr = b->getSubExp1();
		}
		break;
	case 'O':
		{
			// Pattern: m[<expr> * 4 + T ] + T
			T = ((Const *)((Binary *)e)->getSubExp2())->getInt();
			// l = m[<expr> * 4 + T ]:
			Exp *l = ((Binary *)e)->getSubExp1();
			if (l->isSubscript()) l = l->getSubExp1();
			// b = <expr> * 4 + T:
			Binary *b = (Binary *)((Location *)l)->getSubExp1();
			// b = <expr> * 4:
			b = (Binary *)b->getSubExp1();
			// expr = <expr>:
			expr = b->getSubExp1();
		}
		break;
	case 'R':
		{
			// Pattern: %pc + m[%pc + (<expr> * 4) + k]
			T = 0;  // ?
			// l = m[%pc  + (<expr> * 4) + k]:
			Exp *l = ((Binary *)e)->getSubExp2();
			if (l->isSubscript()) l = l->getSubExp1();
			// b = %pc + (<expr> * 4) + k:
			Binary *b = (Binary *)((Location *)l)->getSubExp1();
			// b = (<expr> * 4) + k:
			b = (Binary *)b->getSubExp2();
			// b = <expr> * 4:
			b = (Binary *)b->getSubExp1();
			// expr = <expr>:
			expr = b->getSubExp1();
		}
		break;
	case 'r':
		{
			// Pattern: %pc + m[%pc + ((<expr> * 4) - k)] - k
			T = 0;  // ?
			// b = %pc + m[%pc + ((<expr> * 4) - k)]:
			Binary *b = (Binary *)((Binary *)e)->getSubExp1();
			// l = m[%pc + ((<expr> * 4) - k)]:
			Exp *l = b->getSubExp2();
			if (l->isSubscript()) l = l->getSubExp1();
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
	}
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
	for (const auto &edge : m_InEdges) {
		if (edge->m_nodeType != TWOWAY)  // look for a two-way BB
			continue;  // Ignore all others
		assert(!edge->m_pRtls->empty());
		const auto &stmts = edge->m_pRtls->back()->getList();
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
findConstantValues(Statement *s, std::list<int> &dests)
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
			ADDRESS T;
			Exp *expr;
			findSwParams(form, e, expr, T);
			if (expr) {
				swi->uTable = T;
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
				swi->iUpper = swi->iNumTable - 1;
				if (expr->getOper() == opMinus && ((Binary *)expr)->getSubExp2()->isIntConst()) {
					swi->iLower = ((Const *)((Binary *)expr)->getSubExp2())->getInt();
					swi->iUpper += swi->iLower;
					expr = ((Binary *)expr)->getSubExp1();
				} else
					swi->iLower = 0;
				swi->pSwitchVar = expr;
				lastStmt->setDest((Exp *)nullptr);
				lastStmt->setSwitchInfo(swi);
				return swi->iNumTable != 0;
			}
		} else {
			// Did not match a switch pattern. Perhaps it is a Fortran style goto with constants at the leaves of the
			// phi tree. Basically, a location with a reference, e.g. m[r28{-} - 16]{87}
			if (e->isSubscript()) {
				Exp *sub = ((RefExp *)e)->getSubExp1();
				if (sub->isLocation()) {
					// Yes, we have <location>{ref}. Follow the tree and store the constant values that <location>
					// could be assigned to in dests
					std::list<int> dests;
					findConstantValues(((RefExp *)e)->getDef(), dests);
					// The switch info wants an array of native addresses
					if (int n = dests.size()) {
						auto destArray = new int[n];
						auto ii = dests.begin();
						for (int i = 0; i < n; ++i)
							destArray[i] = *ii++;
						auto swi = new SWITCH_INFO;
						swi->chForm = 'F';  // The "Fortran" form
						swi->pSwitchVar = e;
						swi->uTable = (ADDRESS)destArray;  // Abuse the uTable member as a pointer
						swi->iNumTable = n;
						swi->iLower = 1;  // Not used, except to compute
						swi->iUpper = n;  // the number of options
						lastStmt->setDest((Exp *)nullptr);
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
				if (e->isSubscript()) e = e->getSubExp1();
				e = ((Binary *)e)->getSubExp1();  // e is global<name>{0}[0]
				if (e->isArrayIndex()
				 && (t1 = ((Binary *)e)->getSubExp2(), t1->isIntConst())
				 && ((Const *)t1)->getInt() == 0)
					e = ((Binary *)e)->getSubExp1();            // e is global<name>{0}
				if (e->isSubscript()) e = e->getSubExp1();  // e is global<name>
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
				if (e->isSubscript())
					e = ((RefExp *)e)->getSubExp1();
				e = ((Location *)e)->getSubExp1();       // e = m[r27{25} + 8]{-} + 8
				Exp *rhs = ((Binary *)e)->getSubExp2();  // rhs = 8
				K2 = ((Const *)rhs)->getInt();
				Exp *lhs = ((Binary *)e)->getSubExp1();  // lhs = m[r27{25} + 8]{-}
				if (lhs->isSubscript())
					lhs = ((RefExp *)lhs)->getSubExp1(); // lhs = m[r27{25} + 8]
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
				if (e->isSubscript())
					e = ((RefExp *)e)->getSubExp1();
				e = ((Location *)e)->getSubExp1();       // e = m[r27{25}]{-} + 8
				Exp *rhs = ((Binary *)e)->getSubExp2();  // rhs = 8
				K2 = ((Const *)rhs)->getInt();
				Exp *lhs = ((Binary *)e)->getSubExp1();  // lhs = m[r27{25}]{-}
				if (lhs->isSubscript())
					lhs = ((RefExp *)lhs)->getSubExp1(); // lhs = m[r27{25}]
				vtExp = lhs;
				K1 = 0;
			}
			break;
		case 3:
			{
				// Example pattern: e = m[m[r27{25} + 8]{-}]{-}
				if (e->isSubscript())
					e = ((RefExp *)e)->getSubExp1();
				e = ((Location *)e)->getSubExp1();          // e = m[r27{25} + 8]{-}
				K2 = 0;
				if (e->isSubscript())
					e = ((RefExp *)e)->getSubExp1();        // e = m[r27{25} + 8]
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
				if (e->isSubscript())
					e = ((RefExp *)e)->getSubExp1();
				e = ((Location *)e)->getSubExp1();    // e = m[r27{25}]{-}
				K2 = 0;
				if (e->isSubscript())
					e = ((RefExp *)e)->getSubExp1();  // e = m[r27{25}]
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
		bool decodeThru = Boomerang::get()->decodeThruIndCall;
		if (decodeThru && vtExp && vtExp->isIntConst()) {
			int addr = ((Const *)vtExp)->getInt();
			ADDRESS pfunc = prog->readNative4(addr);
			if (!prog->findProc(pfunc)) {
				// A new, undecoded procedure
				if (Boomerang::get()->noDecodeChildren)
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
 * Called when a switch has been identified.  Visits the destinations of the
 * switch, adds out edges to the BB, etc.
 *
 * \note Used to be called as soon as a switch statement is discovered, but
 * this causes decoded but unanalysed BBs (statements not numbered, locations
 * not SSA renamed, etc.) to appear in the CFG.  This caused problems when
 * there were nested switch statements.  Now only called when re-decoding a
 * switch statement.
 *
 * \param proc  Pointer to the UserProc object for this code.
 */
void
BasicBlock::processSwitch(UserProc *proc)
{
	const auto &last = m_pRtls->back();
	CaseStatement *lastStmt = (CaseStatement *)last->getHlStmt();
	SWITCH_INFO *si = lastStmt->getSwitchInfo();

	if (DEBUG_SWITCH) {
		LOG << "processing switch statement type " << si->chForm << " with table at 0x" << std::hex << si->uTable << std::dec << ", ";
		if (si->iNumTable)
			LOG << si->iNumTable << " entries, ";
		LOG << "lo= " << si->iLower << ", hi= " << si->iUpper << "\n";
	}
	int iNum = si->iUpper - si->iLower + 1;
	// Emit an NWAY BB instead of the COMPJUMP. Also update the number of out edges.
	updateType(NWAY, iNum);

	Prog *prog = proc->getProg();
	Cfg *cfg = proc->getCFG();
	// Where there are repeated switch cases, we have repeated out-edges from the BB. Example:
	// switch (x) {
	//   case 3: case 5:
	//      do something;
	//      break;
	//   case 4: case 10:
	//      do something else
	// ... }
	// The switch statement is emitted assuming one out-edge for each switch value, which is assumed to be iLower+i
	// for the ith zero-based case. It may be that the code for case 5 above will be a goto to the code for case 3,
	// but a smarter back end could group them
	std::list<ADDRESS> dests;
	for (int i = 0; i < iNum; ++i) {
		ADDRESS uSwitch;
		// Get the destination address from the switch table.
		if (si->chForm == 'H') {
			int iValue = prog->readNative4(si->uTable + i*2);
			if (iValue == -1) continue;
			uSwitch = prog->readNative4(si->uTable + i*8 + 4);
		} else if (si->chForm == 'F') {
			uSwitch = ((int *)si->uTable)[i];
		} else {
			uSwitch = prog->readNative4(si->uTable + i*4);
		}
		if (si->chForm == 'O' || si->chForm == 'R' || si->chForm == 'r')
			// Offset: add table address to make a real pointer to code.  For type R, the table is relative to the
			// branch, so take iOffset. For others, iOffset is 0, so no harm
			uSwitch += si->uTable - si->iOffset;
		if (uSwitch < prog->getLimitTextHigh()) {
			//tq.visit(cfg, uSwitch, this);
			cfg->addOutEdge(this, uSwitch, true);
			// Remember to decode the newly discovered switch code arms, if necessary
			// Don't do it right now, in case there are recursive switch statements (e.g. app7win.exe from
			// hackthissite.org)
			dests.push_back(uSwitch);
		} else {
			LOG << "switch table entry branches to past end of text section 0x" << std::hex << uSwitch << std::dec << "\n";
#if 1
			// TMN: If we reached an array entry pointing outside the program text, we can be quite confident the array
			// has ended. Don't try to pull any more data from it.
			LOG << "Assuming the end of the pointer-array has been reached at index " << i << "\n";
			// TODO: Elevate this logic to the code calculating iNumTable, but still leave this code as a safeguard.
			// Q: Should m_iNumOutEdges really be adjusted (iNum - i) ?
			assert(m_iNumOutEdges >= (iNum - i));
			m_iNumOutEdges -= (iNum - i);
			break;
#else
			--m_iNumOutEdges;  // FIXME: where is this set?
#endif
		}
	}
	// Decode the newly discovered switch code arms, if any, and if not already decoded
	int count = 0;
	for (const auto &dest : dests) {
		char tmp[1024];
		sprintf(tmp, "before decoding fragment %i of %i (%x)", ++count, dests.size(), dest);
		Boomerang::get()->alert_decompile_debug_point(proc, tmp);
		prog->decodeFragment(proc, dest);
	}
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
