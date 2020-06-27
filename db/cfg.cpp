/**
 * \file
 * \brief Implementation of the CFG class.
 *
 * \authors
 * Copyright (C) 1997-2000, The University of Queensland
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

#include "cfg.h"

#include "boomerang.h"
#include "exp.h"
#include "hllcode.h"
#include "proc.h"
#include "rtl.h"
#include "signature.h"
#include "statement.h"
#include "types.h"

#include <algorithm>    // For std::find()
#include <set>

#include <cassert>
#include <cstring>

/**
 * \brief Destructor.
 * \note Destructs the component BBs as well.
 */
Cfg::~Cfg()
{
	// Delete the BBs
	for (const auto &bb : m_listBB)
		delete bb;
}

/**
 * \brief Set the pointer to the owning UserProc object.
 * \param proc  Pointer to the owning UserProc object.
 */
void
Cfg::setProc(UserProc *proc)
{
	myProc = proc;
}

/**
 * \brief Clear this CFG of all basic blocks, ready for decode.
 */
void
Cfg::clear()
{
	// Don't delete the BBs; this will delete any CaseStatements we want to save for the re-decode. Just let the garbage
	// collection take care of it.
#if 0
	for (const auto &bb : m_listBB)
		delete bb;
#endif
	m_listBB.clear();
	m_mapBB.clear();
	implicitMap.clear();
	entryBB = nullptr;
	exitBB = nullptr;
	m_bWellFormed = false;
}

/**
 * \brief Copy constructor.
 */
Cfg &
Cfg::operator =(const Cfg &other)
{
	if (this != &other) {
		m_listBB = other.m_listBB;
		m_mapBB = other.m_mapBB;
		m_bWellFormed = other.m_bWellFormed;
	}
	return *this;
}

/**
 * \brief Set the entry BB pointer.
 * \param bb  Pointer to the entry BB.
 */
void
Cfg::setEntryBB(BasicBlock *bb)
{
	entryBB = bb;
	for (const auto &b : m_listBB) {
		if (b->getType() == RET) {
			exitBB = b;
			return;
		}
	}
	// It is possible that there is no exit BB
}

/**
 * \brief Set the exit BB pointer.
 * \note Each cfg should have only one exit node now.
 * \param bb  Pointer to the exit BB.
 */
void
Cfg::setExitBB(BasicBlock *bb)
{
	exitBB = bb;
}

/**
 * Check the entry BB pointer; if null, emit error message and return true.
 *
 * \returns  true if was null.
 */
bool
Cfg::checkEntryBB()
{
	if (!entryBB) {
		std::cerr << "No entry BB for ";
		if (myProc)
			std::cerr << myProc->getName() << std::endl;
		else
			std::cerr << "unknown proc\n";
		return true;
	}
	return false;
}

/**
 * \brief Add a new basic block to this cfg.
 *
 * Checks to see if the address associated with pRtls is already in the map as
 * an incomplete BB; if so, it is completed now and a pointer to that BB is
 * returned.  Otherwise, allocates memory for a new basic block node,
 * initializes its list of RTLs with pRtls, and its type to the given type.
 *
 * The native address associated with the start of the BB is taken from pRtls,
 * and added to the map (unless 0).  NOTE: You cannot assume that the returned
 * BB will have the RTL associated with pStart as its first RTL, since the BB
 * could be split.  You can however assume that the returned BB is suitable
 * for adding out edges (i.e. if the BB is split, you get the "bottom" part of
 * the BB, not the "top" (with lower addresses at the "top")).
 *
 * \param pRtls   List of pointers to RTLs to initialise the BB with.
 * \param bbType  The type of the BB (e.g. TWOWAY).
 * \returns       Pointer to the newly-created BB, or to an existing
 *                incomplete BB that is then completed by splitting the
 *                newly-created BB.
 *
 * \throws BBAlreadyExistsError
 * The new BB partially or fully overlaps an existing completed BB.
 * (This can happen with certain kinds of forward branches.)
 */
BasicBlock *
Cfg::newBB(std::list<RTL *> *pRtls, BBTYPE bbType) throw (BBAlreadyExistsError)
{
	assert(pRtls);
	assert(bbType != INCOMPLETE);

	// First find the native address of the first RTL
	// Can't use BasicBlock::GetLowAddr(), since we don't yet have a BB!
	ADDRESS addr = pRtls->front()->getAddress();

	// If this is zero, try the next RTL (only). This may be necessary if e.g. there is a BB with a delayed branch only,
	// with its delay instruction moved in front of it (with 0 address).
	// Note: it is possible to see two RTLs with zero address with Sparc: jmpl %o0, %o1. There will be one for the delay
	// instr (if not a NOP), and one for the side effect of copying %o7 to %o1.
	// Note that orphaned BBs (for which we must compute addr here to to be 0) must not be added to the map, but they
	// have no RTLs with a non zero address.
	if ((addr == 0) && (pRtls->size() > 1)) {
		auto next = pRtls->begin();
		addr = (*++next)->getAddress();
	}

	// If this addr is non zero, check the map to see if we have a (possibly incomplete) BB here already
	// If it is zero, this is a special BB for handling delayed branches or the like
	auto mi = m_mapBB.end();
	BasicBlock *pBB = nullptr;
	if (addr != 0) {
		mi = m_mapBB.find(addr);
		if (mi != m_mapBB.end()) {
			pBB = mi->second;
			// It should be incomplete, else we have duplicated BBs.
			// Note: this can happen with forward jumps into the middle of a loop, so not error
			if (!pBB->isComplete()) {
				// Fill in the details, and return it
				pBB->setRTLs(pRtls);
				pBB->updateType(bbType);
			} else {
				// Shouldn't get here unless we've accidentally re-decoded a BB.
				assert(false);

				// This list of RTLs is not needed now
				for (const auto &rtl : *pRtls)
					delete rtl;
				if (VERBOSE)
					LOG << "throwing BBAlreadyExistsError\n";
				throw BBAlreadyExistsError(pBB);
			}
		}
	}
	if (!pBB) {
		// Else add a new BB to the back of the current list.
		pBB = new BasicBlock(pRtls, bbType);
		m_listBB.push_back(pBB);

		// Also add the address to the map from native (source) address to
		// pointer to BB, unless it's zero
		if (addr != 0) {
			m_mapBB[addr] = pBB;  // Insert the mapping
			mi = m_mapBB.find(addr);
		}
	}

	if (mi != m_mapBB.end()) {
		// Existing New         +---+ Top of new
		//          +---+       +---+
		//  +---+   |   |       +---+ Fall through
		//  |   |   |   | =>    |   |
		//  |   |   |   |       |   | Existing; rest of new discarded
		//  +---+   +---+       +---+
		//
		// Check for overlap of the just added BB with the next BB (address wise).  If there is an overlap, truncate the
		// RTL for the new BB to not overlap, and make this a fall through BB.
		// We still want to do this even if the new BB overlaps with an incomplete BB, though in this case,
		// splitBB needs to fill in the details for the "bottom" BB of the split.
		// Also, in this case, we return a pointer to the newly completed BB, so it will get out edges added
		// (if required). In the other case (i.e. we overlap with an exising, completed BB), we do not want to return a
		// pointer to the completed BB, since the out edges are already created.
		while (++mi != m_mapBB.end()) {
			auto nextAddr = mi->first;
			if (nextAddr <= pBB->getHiAddr()) {
				// Shouldn't get here as long as pRtls doesn't span an existing label.
				// FrontEnd::processProc() creates a fall BB when it encounters a label.
				assert(false);

				// Need to truncate the current BB.  Determine completeness of the existing BB
				// before calling splitBB() since it will be completed by the call.
				auto complete = mi->second->isComplete();
				pBB = splitBB(pBB, nextAddr);
				mi = m_mapBB.find(nextAddr);
				// If the existing BB was incomplete, return the "bottom" part of the BB,
				// so adding out edges will work properly.  However, if the overlapping BB
				// was already complete, throw it so out edges won't be added twice.
				if (complete)
					throw BBAlreadyExistsError(pBB);
			} else {
				break;
			}
		}

		// Existing New         +---+ Top of existing
		//  +---+               +---+
		//  |   |   +---+       +---+ Fall through
		//  |   |   |   | =>    |   |
		//  |   |   |   |       |   | New; rest of existing discarded
		//  +---+   +---+       +---+
		// Note: no need to check the other way around, because in this case, we will have called Cfg::label(), and it
		// will have split the existing BB already.
	}
	return pBB;
}

/**
 * Add an out-edge from a source BB to a destination address (a label; a new
 * BB will be created here if one does not already exist).  Also adds the
 * corresponding in-edge to the destination BB.
 *
 * \param[in,out] src  Source BB.  This BB may be split (if addr is a location
 *                     inside it), in which case src will then point to the
 *                     bottom BB.
 * \param[in] addr     Destination address (the out edge is to point to the BB
 *                     whose lowest address is addr).  An incomplete BB will
 *                     be created if required.
 */
void
Cfg::addOutEdge(BasicBlock *&src, ADDRESS addr)
{
	// Check to see if the address is in the map,
	// i.e. we already have a BB for this address.
	auto it = m_mapBB.find(addr);
	if (it == m_mapBB.end()) {
		label(src, addr);
		it = m_mapBB.find(addr);
		assert(it != m_mapBB.end());
	}
	src->addEdge(it->second);
}

/**
 * Visit a destination as a label, i.e. queue it as a new BB to create later.
 *
 * This should be called when a control transfer to addr is found, usually
 * from addOutEdge() when adding an edge from src to addr.  There are three
 * scenarios:
 * 1. addr refers to the start of an existing (complete or incomplete) BB.  If
 *    incomplete, it should already be in the queue.  If complete, it has
 *    already been processed.  In either case, there is nothing we need to do.
 * 2. addr refers to the middle of an existing (complete) BB.  Split this BB.
 *    If this is src, src is updated to point to the bottom BB after the
 *    split.  Presumably the caller will add more out-edges to src; these
 *    edges need to emanate from the bottom BB.
 * 3. There is no existing BB at addr.  Create an incomplete BB and queue it
 *    for later.
 *
 * \sa addOutEdge()
 */
void
Cfg::label(BasicBlock *&src, ADDRESS addr)
{
	auto mi = m_mapBB.find(addr);
	if (mi != m_mapBB.end())
		return;

	// New label.  Create a new incomplete BB for it.
	auto incBB = new BasicBlock();
	// Add it to the list and map.
	m_listBB.push_back(incBB);
	m_mapBB[addr] = incBB;

	// Check if the previous element in the (sorted) map overlaps this new label.
	// If so, split the previous BB at the label to complete the new BB.
	mi = m_mapBB.find(addr);
	if (mi != m_mapBB.begin()) {
		auto prevBB = (*--mi).second;
		if (prevBB->isComplete()
		 && prevBB->getLowAddr() <  addr
		 && prevBB->getHiAddr()  >= addr) {
			auto botBB = splitBB(prevBB, addr);
			if (src == prevBB) {
				// This means that the BB that we are expecting to use, usually to add out edges, has changed. We must
				// change this pointer so that the right BB gets the out edges. However, if the new BB is not the BB of
				// interest, we mustn't change src
				src = botBB;
			}
			assert(incBB == botBB);  // splitBB() should have completed this BB.
			return;
		}
	}

	// A non-overlapping incomplete BB is now in the map.  Queue it for later.
	enqueue(addr);
}

/**
 * Split the given basic block at the RTL associated with addr.  The first
 * node's type becomes fall-through and ends at the RTL prior to that
 * associated with addr.  The second node's type becomes the type of the
 * original basic block (orig), and its out-edges are those of the original
 * basic block.  In-edges of the new BB's descendants are changed.
 *
 * \pre Assumes addr is an address within the boundaries of the given
 * basic block.
 *
 * \param orig  The basic block to split.
 * \param addr  Address of RTL to become the start of the new BB.
 * \returns     The "bottom" (new) part of the split BB.
 */
BasicBlock *
Cfg::splitBB(BasicBlock *orig, ADDRESS addr)
{
	// First find which RTL has the split address; note that this could
	// fail (e.g. label in the middle of an instruction, or some weird
	// delay slot effects)
	auto &rtls = *orig->m_pRtls;
	auto ri = rtls.begin();
	for (; ri != rtls.end(); ++ri) {
		if ((*ri)->getAddress() == addr)
			break;
	}
	if (ri == rtls.end()) {
		std::cerr << "could not split BB at " << std::hex << orig->getLowAddr()
		          << " at split address " << addr << std::dec << std::endl;
		return orig;
	}
	return splitBB(orig, ri);
}

/**
 * \overload
 * \param orig  The basic block to split.
 * \param ri    Where to split orig's RTL.
 *              This iterator will remain valid after the call.
 */
BasicBlock *
Cfg::splitBB(BasicBlock *orig, std::list<RTL *>::iterator ri)
{
	// Split the RTLs at ri.
	auto &head = *orig->m_pRtls;
	auto tail = new std::list<RTL *>();
	tail->splice(tail->begin(), head, ri, head.end());

	// Check for an existing (usually incomplete) BB at the split address.
	ADDRESS addr = (*ri)->getAddress();
	BasicBlock *bot = nullptr;
	auto mi = m_mapBB.find(addr);
	if (mi != m_mapBB.end())
		bot = mi->second;

	if (!bot) {
		// Set up a new BB with information from the original BB.
		bot = new BasicBlock(*orig);
		// Put it in the graph.
		m_listBB.push_back(bot);
		m_mapBB[addr] = bot;

		// But we don't want the top BB's in-edges;
		// our only in-edge should be the out-edge from the top BB
		bot->m_InEdges.clear();
		// The "bottom" BB now starts at the implicit label.
		bot->setRTLs(tail);
	} else if (!bot->isComplete()) {
		// We have an existing BB and a map entry, but no details
		// except for in-edges.  Save them.
		auto ins = bot->m_InEdges;
		// Copy over the details now, completing the bottom BB.
		*bot = *orig;

		// Replace the in-edges (likely only one).
		bot->m_InEdges = ins;
		// The "bottom" BB now starts at the implicit label.
		bot->setRTLs(tail);
	} else {
		// Shouldn't get here, except potentially from newBB()
		// when it truncates its RTLs at a complete BB.
		assert(false);

		// bot already exists and is complete, and orig overlaps it.
		// Discard the overlapping portion of orig.  We don't want to
		// change the complete BB in any way, except to later add an
		// edge from orig to bot.
		for (const auto &rtl : *tail)
			delete rtl;
		delete tail;

		// Remove edges from orig to its successors.
		// If any exist, they should duplicate those in bot.
		for (const auto &succ : orig->m_OutEdges)
			succ->deleteInEdge(orig);
		orig->m_OutEdges.clear();
	}

	// Fix the in-edges of orig's successors.  They are now bot.
	for (const auto &succ : orig->m_OutEdges) {
		for (auto &pred : succ->m_InEdges) {
			if (pred == orig) {
				pred = bot;
				break;
			}
		}
	}

	// Update original ("top") BB's info and make it a fall-through.
	orig->updateType(FALL);
	// Erase any existing out-edges.
	orig->m_OutEdges.clear();
	orig->addEdge(bot);
	return bot;
}

/**
 * \brief Seed the queue with an initial address.
 *
 * Provide an initial address (can call several times if there are several
 * entry points).
 *
 * \note Can be some targets already in the queue now.
 *
 * \param addr  Address to seed the queue with.
 */
void
Cfg::enqueue(ADDRESS addr)
{
	targets.push(addr);
	if (Boomerang::get().traceDecoder)
		LOG << ">0x" << std::hex << addr << std::dec << "\t";
}

/**
 * \brief Return the next target from the queue of non-processed targets.
 *
 * \returns  The next address to process,
 *           or NO_ADDRESS if none (queue is empty).
 */
ADDRESS
Cfg::dequeue()
{
	while (!targets.empty()) {
		ADDRESS addr = targets.front();
		targets.pop();
		if (Boomerang::get().traceDecoder)
			LOG << "<0x" << std::hex << addr << std::dec << "\t";

		// Skip parsing any addresses that contain a completed BB.
		if (!isComplete(addr))
			return addr;
	}
	return NO_ADDRESS;
}

/**
 * \brief Return true if the given address is the start of a basic block,
 * complete or not.
 *
 * Just checks to see if there exists a BB starting with this native address.
 * If not, the address is NOT added to the map of labels to BBs.
 *
 * \param addr  Native address to look up.
 * \returns     true if addr starts a BB.
 */
bool
Cfg::existsBB(ADDRESS addr) const
{
	return !!m_mapBB.count(addr);
}

/**
 * \brief Return true if there is a completed BB already at this address.
 *
 * \param addr  Address to look up.
 * \returns     true if addr starts a completed BB.
 */
bool
Cfg::isComplete(ADDRESS addr) const
{
	auto mi = m_mapBB.find(addr);
	if (mi == m_mapBB.end())
		return false;
	return mi->second->isComplete();
}

/**
 * \brief Sorts the BBs in a cfg by first address.  Just makes it more
 * convenient to read when BBs are iterated.
 *
 * Sorts the BBs in the CFG according to the low address of each BB.  Useful
 * because it makes printouts easier, if they used iterators to traverse the
 * list of BBs.
 */
void
Cfg::sortByAddress()
{
	m_listBB.sort(BasicBlock::lessAddress);
}

#if 0 // Cruft?
/**
 * \brief Sorts the BBs in the CFG by their first DFT numbers.
 */
void
Cfg::sortByFirstDFT()
{
	m_listBB.sort(BasicBlock::lessFirstDFT);
}

/**
 * \brief Sorts the BBs in the CFG by their last DFT numbers.
 */
void
Cfg::sortByLastDFT()
{
	m_listBB.sort(BasicBlock::lessLastDFT);
}
#endif

/**
 * Transforms the input machine-dependent cfg, which has ADDRESS labels for
 * each out-edge, into a machine-independent cfg graph (i.e. a well-formed
 * graph) which has references to basic blocks for each out-edge.
 *
 * Checks that all BBs are complete, and all out edges are valid.  However,
 * ADDRESSes that are interprocedural out edges are not checked or changed.
 *
 * \returns  true if transformation was successful.
 */
bool
Cfg::wellFormCfg()
{
	m_bWellFormed = true;
	for (const auto &bb : m_listBB) {
		// Check that it's complete
		auto type = bb->getType();
		if (type == INCOMPLETE) {
			m_bWellFormed = false;
			auto mi = m_mapBB.begin();
			for (; mi != m_mapBB.end(); ++mi)
				if (mi->second == bb) break;
			if (mi == m_mapBB.end())
				std::cerr << "WellFormCfg: incomplete BB not even in map!\n";
			else
				std::cerr << "WellFormCfg: BB with native address " << std::hex << mi->first << std::dec
				          << " is incomplete\n";
		} else {
			// Complete. Test the out edges
			auto n = bb->m_OutEdges.size();
			if ((type == INVALID  && n != 0)
			 || (type == FALL     && n != 1)
			 || (type == ONEWAY   && n != 1)
			 || (type == TWOWAY   && n != 2)
			 || (type == COMPJUMP && n != 0)
			 || (type == COMPCALL && n != 1)
			 || (type == CALL     && n >  1)
			 || (type == RET      && n != 0)) {
				m_bWellFormed = false;
				std::cerr << "WellFormCfg: BB with native address " << std::hex << bb->getLowAddr() << std::dec
				          << " has " << n << " outedges\n";
			}

			int i = 0;
			for (const auto &succ : bb->m_OutEdges) {
				// Check that the out edge has been written (i.e. non-null)
				if (!succ) {
					m_bWellFormed = false;  // At least one problem
					std::cerr << "WellFormCfg: BB with native address " << std::hex << bb->getLowAddr() << std::dec
					          << " is missing outedge " << i << std::endl;
				} else {
					// Check that there is a corresponding in edge from the
					// child to here
					auto ii = std::find(succ->m_InEdges.begin(), succ->m_InEdges.end(), bb);
					if (ii == succ->m_InEdges.end()) {
						std::cerr << "WellFormCfg: No in edge to BB at " << std::hex << bb->getLowAddr()
						          << " from successor BB at " << succ->getLowAddr() << std::dec << std::endl;
						m_bWellFormed = false;  // At least one problem
					}
				}
				++i;
			}
			// Also check that each in edge has a corresponding out edge to here (could have an extra in-edge, for
			// example)
			for (const auto &pred : bb->m_InEdges) {
				auto oo = std::find(pred->m_OutEdges.begin(), pred->m_OutEdges.end(), bb);
				if (oo == pred->m_OutEdges.end()) {
					std::cerr << "WellFormCfg: No out edge to BB at " << std::hex << bb->getLowAddr()
					          << " from predecessor BB at " << pred->getLowAddr() << std::dec << std::endl;
					m_bWellFormed = false;  // At least one problem
				}
			}
		}
	}
	return m_bWellFormed;
}

/**
 * \brief Amalgamate the RTLs for pb1 and pb2, and place the result into pb2.
 *
 * This is called where a two-way branch is deleted, thereby joining a two-way
 * BB with its successor.  This happens for example when transforming Intel
 * floating point branches, and a branch on parity is deleted.  The joined BB
 * becomes the type of the successor.
 *
 * \note Assumes fallthrough of *pb1 is *pb2.
 *
 * \param pb1,pb2  Pointers to the BBs to join.
 * \returns        true if successful.
 */
bool
Cfg::joinBB(BasicBlock *pb1, BasicBlock *pb2)
{
	// Ensure that the fallthrough case for pb1 is pb2
	const auto &v = pb1->getOutEdges();
	if (v.size() != 2 || v[1] != pb2)
		return false;
	// Prepend the RTLs for pb1 to those of pb2.
	pb2->m_pRtls->splice(pb2->m_pRtls->begin(), *pb1->m_pRtls);
	pb1->bypass(pb2);  // Mash them together
	removeBB(pb1);
	return true;
}

/**
 * \brief Completely remove a BB from the CFG.
 */
void
Cfg::removeBB(BasicBlock *bb)
{
	// All in-edges should have been removed by now.
	// Out-edges can be left in and they will be deleted here.
	assert(bb->m_InEdges.empty());
	for (const auto &succ : bb->m_OutEdges)
		succ->deleteInEdge(bb);
	bb->m_OutEdges.clear();

	for (auto it = m_mapBB.begin(); it != m_mapBB.end(); ++it) {
		if (bb == it->second) {
			m_mapBB.erase(it);
			break;
		}
	}
	auto it = std::find(m_listBB.begin(), m_listBB.end(), bb);
	m_listBB.erase(it);
}

/**
 * \brief Compress the CFG.  For now, it only removes BBs that are just
 * branches.
 *
 * Given a well-formed cfg graph, optimizations are performed on the graph to
 * reduce the number of basic blocks and edges.
 *
 * Optimizations performed are:  Removal of branch chains (i.e. jumps to
 * jumps), removal of redundant jumps (i.e. jump to the next instruction),
 * merge basic blocks where possible, and remove redundant basic blocks
 * created by the previous optimizations.
 *
 * \returns false if not well formed; true otherwise.
 */
bool
Cfg::compressCfg()
{
	// must be well formed
	if (!m_bWellFormed) return false;

	// FIXME: The below was working while we still had reaching definitions.  It seems to me that it would be easy to
	// search the BB for definitions between the two branches (so we don't need reaching defs, just the SSA property of
	//  unique definition).
	//
	// Look in CVS for old code.

	// Find A -> J -> B where J is a BB that is only a jump
	// Then A -> B
	auto removes = std::list<BasicBlock *>();
	for (const auto &bbJ : m_listBB) {
		if (bbJ->m_OutEdges.size() == 1
		 && bbJ->m_pRtls->size() == 1
		 && bbJ->m_pRtls->front()->getList().size() == 1
		 && bbJ->m_pRtls->front()->getList().front()->getKind() == STMT_GOTO) {
			// Found an only-jump BB
			auto bbB = bbJ->m_OutEdges.front();
			bbJ->bypass(bbB);
			removes.push_back(bbJ);
		}
	}
	// Separate removal loop to avoid invalidating iterators in the previous loop.
	for (const auto &bb : removes) {
		removeBB(bb);
		// And delete the BB
		delete bb;
	}
	return true;
}

/**
 * \brief Reset all the traversed flags.
 *
 * To make this a useful public function, we need access to the traversed flag
 * with other public functions.
 */
void
Cfg::unTraverse()
{
	for (const auto &bb : m_listBB) {
		bb->m_iTraversed = false;
		bb->traversed = UNTRAVERSED;
	}
}

#if 0 // Cruft?
/**
 * Given a well-formed cfg graph, a partial ordering is established between
 * the nodes.  The ordering is based on the final visit to each node during a
 * depth first traversal such that if node n1 was visited for the last time
 * before node n2 was visited for the last time, n1 will be less than n2.  The
 * return value indicates if all nodes where ordered.  This will not be the
 * case for incomplete CFGs (e.g. switch table not completely recognised) or
 * where there are nodes unreachable from the entry node.
 *
 * \returns  All nodes where ordered.
 */
bool
Cfg::establishDFTOrder()
{
	// Must be well formed.
	if (!m_bWellFormed) return false;

	// Reset all the traversed flags
	unTraverse();

	if (checkEntryBB()) return false;

	int first = 0;
	int last = 0;
	unsigned numTraversed = entryBB->DFTOrder(first, last);

	return numTraversed == m_listBB.size();
}
#endif

BasicBlock *
Cfg::findRetNode() const
{
	BasicBlock *retNode = nullptr;
	for (const auto &bb : m_listBB) {
		if (bb->getType() == RET)
			return bb;
		if (bb->getType() == CALL && bb->getOutEdges().empty())
			retNode = bb;
	}
	return retNode;
}

#if 0 // Cruft?
/**
 * \brief Performs establishDFTOrder on the inverse of the graph (i.e. flips the
 * graph).
 *
 * Performs establishDFTOrder on the reverse (flip) of the graph.  Assumes
 * establishDFTOrder has already been called.
 *
 * \returns  All nodes where ordered.
 */
bool
Cfg::establishRevDFTOrder()
{
	// Must be well formed.
	if (!m_bWellFormed) return false;

	// WAS: sort by last dfs and grab the exit node
	// Why?  This does not seem like a the best way. What we need is the ret node, so let's find it.  If the CFG has
	// more than one ret node then it needs to be fixed.
	//sortByLastDFT();

	BasicBlock *retNode = findRetNode();

	if (!retNode) return false;

	// Reset all the traversed flags
	unTraverse();

	int first = 0;
	int last = 0;
	unsigned numTraversed = retNode->RevDFTOrder(first, last);

	return numTraversed == m_listBB.size();
}
#endif

/**
 * \returns  true if the CFG is well formed.
 */
bool
Cfg::isWellFormed() const
{
	return m_bWellFormed;
}

/**
 * \returns  true if there is a BB at the address given whose first RTL is an
 *           orphan, i.e. RTL::getAddress() returns 0.
 */
bool
Cfg::isOrphan(ADDRESS addr) const
{
	auto mi = m_mapBB.find(addr);
	if (mi == m_mapBB.end())
		// No entry at all
		return false;
	// Return true if the first RTL at this address has an address set to 0
	BasicBlock *pBB = mi->second;
	// If it's incomplete, it can't be an orphan
	return pBB->isComplete() && pBB->m_pRtls->front()->getAddress() == 0;
}

/**
 * Replace all instances of search with replace.  Can be type sensitive if
 * required.
 *
 * \param[in] search   A location to search for.
 * \param[in] replace  The expression with which to replace it.
 */
void
Cfg::searchAndReplace(Exp *search, Exp *replace)
{
	for (const auto &bb : m_listBB) {
		for (const auto &rtl : *bb->getRTLs()) {
			rtl->searchAndReplace(search, replace);
		}
	}
}

bool
Cfg::searchAll(Exp *search, std::list<Exp *> &result)
{
	bool found = false;
	for (const auto &bb : m_listBB) {
		for (const auto &rtl : *bb->getRTLs()) {
			found |= rtl->searchAll(search, result);
		}
	}
	return found;
}

/**
 * \brief Simplify all the expressions in the CFG.
 */
void
Cfg::simplify()
{
	if (VERBOSE)
		LOG << "simplifying...\n";
	for (const auto &bb : m_listBB)
		bb->simplify();
}

/**
 * \brief Print this cfg, mainly for debugging.
 */
void
Cfg::print(std::ostream &out, bool html) const
{
	for (const auto &bb : m_listBB)
		bb->print(out, html);
	out << std::endl;
}

bool
Cfg::setTimeStamps()
{
	auto retNode = findRetNode();
	if (!retNode)
		return false;

	// set DFS tag
	for (const auto &bb : m_listBB)
		bb->traversed = DFS_TAG;

	// set the parenthesis for the nodes as well as setting the post-order ordering between the nodes
	int time = 0;
	Ordering.clear();
	entryBB->setLoopStamps(time, Ordering);

	// set the reverse parenthesis for the nodes
	time = 0;
	entryBB->setRevLoopStamps(time);

	revOrdering.clear();
	retNode->setRevOrder(revOrdering);
	return true;
}

/**
 * Finds the common post dominator of the current immediate post dominator and
 * its successor's immediate post dominator.
 */
BasicBlock *
Cfg::commonPDom(BasicBlock *curImmPDom, BasicBlock *succImmPDom)
{
	if (!curImmPDom)
		return succImmPDom;
	if (!succImmPDom)
		return curImmPDom;
	if (curImmPDom->revOrd == succImmPDom->revOrd)
		return curImmPDom;  // ordering hasn't been done

	BasicBlock *oldCurImmPDom = curImmPDom;
	BasicBlock *oldSuccImmPDom = succImmPDom;

	int giveup = 0;
#define GIVEUP 10000
	while (giveup < GIVEUP && curImmPDom && succImmPDom && (curImmPDom != succImmPDom)) {
		if (curImmPDom->revOrd > succImmPDom->revOrd)
			succImmPDom = succImmPDom->immPDom;
		else
			curImmPDom = curImmPDom->immPDom;
		++giveup;
	}

	if (giveup >= GIVEUP) {
		if (VERBOSE)
			LOG << "failed to find commonPDom for 0x" << std::hex << oldCurImmPDom->getLowAddr() << std::dec
			    << " and 0x" << std::hex << oldSuccImmPDom->getLowAddr() << std::dec << "\n";
		return oldCurImmPDom;  // no change
	}

	return curImmPDom;
}

/**
 * Finds the immediate post dominator of each node in the graph PROC->cfg.
 * Adapted version of the dominators algorithm by Hecht and Ullman; finds
 * immediate post dominators only.
 *
 * \note Graph should be reducible.
 */
void
Cfg::findImmedPDom()
{
	// traverse the nodes in order (i.e from the bottom up)
	for (auto it = revOrdering.rbegin(); it != revOrdering.rend(); ++it) {
		const auto &curNode = *it;
		const auto &oEdges = curNode->getOutEdges();
		for (const auto &succNode : oEdges) {
			if (succNode->revOrd > curNode->revOrd)
				curNode->immPDom = commonPDom(curNode->immPDom, succNode);
		}
	}

	// make a second pass but consider the original CFG ordering this time
	for (const auto &curNode : Ordering) {
		const auto &oEdges = curNode->getOutEdges();
		if (oEdges.size() > 1)
			for (const auto &succNode : oEdges) {
				curNode->immPDom = commonPDom(curNode->immPDom, succNode);
			}
	}

	// one final pass to fix up nodes involved in a loop
	for (const auto &curNode : Ordering) {
		const auto &oEdges = curNode->getOutEdges();
		if (oEdges.size() > 1) {
			for (const auto &succNode : oEdges) {
				if (curNode->hasBackEdgeTo(succNode)
				 && curNode->getOutEdges().size() > 1
				 && succNode->immPDom
				 && succNode->immPDom->ord < curNode->immPDom->ord)
					curNode->immPDom = commonPDom(succNode->immPDom, curNode->immPDom);
				else
					curNode->immPDom = commonPDom(curNode->immPDom, succNode);
			}
		}
	}
}

/**
 * Structures all conditional headers (i.e. nodes with more than one outedge).
 */
void
Cfg::structConds()
{
	// Process the nodes in order
	for (const auto &curNode : Ordering) {
		// does the current node have more than one out edge?
		if (curNode->getOutEdges().size() > 1) {
			// if the current conditional header is a two way node and has a back edge, then it won't have a follow
			if (curNode->hasBackEdge() && curNode->getType() == TWOWAY) {
				curNode->setStructType(Cond);
				continue;
			}

			// set the follow of a node to be its immediate post dominator
			curNode->setCondFollow(curNode->immPDom);

			// set the structured type of this node
			curNode->setStructType(Cond);

			// if this is an nway header, then we have to tag each of the nodes within the body of the nway subgraph
			if (curNode->getCondType() == Case)
				curNode->setCaseHead(curNode, curNode->getCondFollow());
		}
	}
}

/**
 * \pre   The loop induced by (head,latch) has already had all its member
 *        nodes tagged.
 * \post  The type of loop has been deduced.
 */
void
Cfg::determineLoopType(BasicBlock *header, bool *&loopNodes)
{
	assert(header->getLatchNode());

	// if the latch node is a two way node then this must be a post tested loop
	if (header->getLatchNode()->getType() == TWOWAY) {
		header->setLoopType(PostTested);

		// if the head of the loop is a two way node and the loop spans more than one block  then it must also be a
		// conditional header
		if (header->getType() == TWOWAY && header != header->getLatchNode())
			header->setStructType(LoopCond);
	}

	// otherwise it is either a pretested or endless loop
	else if (header->getType() == TWOWAY) {
		// if the header is a two way node then it must have a conditional follow (since it can't have any backedges
		// leading from it). If this follow is within the loop then this must be an endless loop
		if (header->getCondFollow() && loopNodes[header->getCondFollow()->ord]) {
			header->setLoopType(Endless);

			// retain the fact that this is also a conditional header
			header->setStructType(LoopCond);
		} else
			header->setLoopType(PreTested);
	}

	// both the header and latch node are one way nodes so this must be an endless loop
	else
		header->setLoopType(Endless);
}

/**
 * \pre   The loop headed by header has been induced and all its member nodes
 *        have been tagged.
 * \post  The follow of the loop has been determined.
 */
void
Cfg::findLoopFollow(BasicBlock *header, bool *&loopNodes)
{
	assert(header->getStructType() == Loop || header->getStructType() == LoopCond);
	loopType lType = header->getLoopType();
	BasicBlock *latch = header->getLatchNode();

	if (lType == PreTested) {
		// if the 'while' loop's true child is within the loop, then its false child is the loop follow
		if (loopNodes[header->getOutEdges()[0]->ord])
			header->setLoopFollow(header->getOutEdges()[1]);
		else
			header->setLoopFollow(header->getOutEdges()[0]);
	} else if (lType == PostTested) {
		// the follow of a post tested ('repeat') loop is the node on the end of the non-back edge from the latch node
		if (latch->getOutEdges()[0] == header)
			header->setLoopFollow(latch->getOutEdges()[1]);
		else
			header->setLoopFollow(latch->getOutEdges()[0]);
	} else { // endless loop
		BasicBlock *follow = nullptr;

		// traverse the ordering array between the header and latch nodes.
		BasicBlock *latch = header->getLatchNode();
		for (int i = header->ord - 1; i > latch->ord; --i) {
			BasicBlock *&desc = Ordering[i];
			// the follow for an endless loop will have the following properties:
			//   i) it will have a parent that is a conditional header inside the loop whose follow is outside the loop
			//  ii) it will be outside the loop according to its loop stamp pair
			// iii) have the highest ordering of all suitable follows (i.e. highest in the graph)

			if (desc->getStructType() == Cond && desc->getCondFollow() && desc->getLoopHead() == header) {
				if (loopNodes[desc->getCondFollow()->ord]) {
					// if the conditional's follow is in the same loop AND is lower in the loop, jump to this follow
					if (desc->ord > desc->getCondFollow()->ord)
						i = desc->getCondFollow()->ord;
					// otherwise there is a backward jump somewhere to a node earlier in this loop. We don't need to any
					//  nodes below this one as they will all have a conditional within the loop.
					else break;
				} else {
					// otherwise find the child (if any) of the conditional header that isn't inside the same loop
					BasicBlock *succ = desc->getOutEdges()[0];
					if (loopNodes[succ->ord]) {
						if (!loopNodes[desc->getOutEdges()[1]->ord])
							succ = desc->getOutEdges()[1];
						else
							succ = nullptr;
					}
					// if a potential follow was found, compare its ordering with the currently found follow
					if (succ && (!follow || succ->ord > follow->ord))
						follow = succ;
				}
			}
		}
		// if a follow was found, assign it to be the follow of the loop under investigation
		if (follow)
			header->setLoopFollow(follow);
	}
}

/**
 * \pre   header has been detected as a loop header and has the details of the
 *        latching node.
 * \post  The nodes within the loop have been tagged.
 */
void
Cfg::tagNodesInLoop(BasicBlock *header, bool *&loopNodes)
{
	assert(header->getLatchNode());

	// traverse the ordering structure from the header to the latch node tagging the nodes determined to be within the
	// loop. These are nodes that satisfy the following:
	//  i) header.loopStamps encloses curNode.loopStamps and curNode.loopStamps encloses latch.loopStamps
	//  OR
	//  ii) latch.revLoopStamps encloses curNode.revLoopStamps and curNode.revLoopStamps encloses header.revLoopStamps
	//  OR
	//  iii) curNode is the latch node

	BasicBlock *latch = header->getLatchNode();
	for (int i = header->ord - 1; i >= latch->ord; --i) {
		if (Ordering[i]->inLoop(header, latch)) {
			// update the membership map to reflect that this node is within the loop
			loopNodes[i] = true;

			Ordering[i]->setLoopHead(header);
		}
	}
}

/**
 * \pre   The graph for curProc has been built.
 * \post  Each node is tagged with the header of the most nested loop of which
 *        it is a member (possibly none).
 *
 * The header of each loop stores information on the latching node as well as
 * the type of loop it heads.
 */
void
Cfg::structLoops()
{
	for (auto it = Ordering.rbegin(); it != Ordering.rend(); ++it) {
		BasicBlock *curNode = *it;          // the current node under investigation
		BasicBlock *latch = nullptr;        // the latching node of the loop

		// If the current node has at least one back edge into it, it is a loop header. If there are numerous back edges
		// into the header, determine which one comes form the proper latching node.
		// The proper latching node is defined to have the following properties:
		//   i) has a back edge to the current node
		//  ii) has the same case head as the current node
		// iii) has the same loop head as the current node
		//  iv) is not an nway node
		//   v) is not the latch node of an enclosing loop
		//  vi) has a lower ordering than all other suitable candiates
		// If no nodes meet the above criteria, then the current node is not a loop header

		const auto &iEdges = curNode->getInEdges();
		for (const auto &pred : iEdges) {
			if (pred->getCaseHead() == curNode->getCaseHead()  // ii)
			 && pred->getLoopHead() == curNode->getLoopHead()  // iii)
			 && (!latch || latch->ord > pred->ord)             // vi)
			 && !(pred->getLoopHead()
			   && pred->getLoopHead()->getLatchNode() == pred) // v)
			 && pred->hasBackEdgeTo(curNode))                  // i)
				latch = pred;
		}

		// if a latching node was found for the current node then it is a loop header.
		if (latch) {
			// define the map that maps each node to whether or not it is within the current loop
			auto loopNodes = new bool[Ordering.size()];
			for (unsigned int j = 0; j < Ordering.size(); ++j)
				loopNodes[j] = false;

			curNode->setLatchNode(latch);

			// the latching node may already have been structured as a conditional header. If it is not also the loop
			// header (i.e. the loop is over more than one block) then reset it to be a sequential node otherwise it
			// will be correctly set as a loop header only later
			if (latch != curNode && latch->getStructType() == Cond)
				latch->setStructType(Seq);

			// set the structured type of this node
			curNode->setStructType(Loop);

			// tag the members of this loop
			tagNodesInLoop(curNode, loopNodes);

			// calculate the type of this loop
			determineLoopType(curNode, loopNodes);

			// calculate the follow node of this loop
			findLoopFollow(curNode, loopNodes);

			// delete the space taken by the loopnodes map
			//delete[] loopNodes;
		}
	}
}

/**
 * This routine is called after all the other structuring has been done.  It
 * detects conditionals that are in fact the head of a jump into/outof a loop
 * or into a case body.  Only forward jumps are considered as unstructured
 * backward jumps will always be generated nicely.
 */
void
Cfg::checkConds()
{
	for (const auto &curNode : Ordering) {
		const auto &oEdges = curNode->getOutEdges();

		// consider only conditional headers that have a follow and aren't case headers
		if ((curNode->getStructType() == Cond || curNode->getStructType() == LoopCond)
		 && curNode->getCondFollow()
		 && curNode->getCondType() != Case) {
			// define convenient aliases for the relevant loop and case heads and the out edges
			BasicBlock *myLoopHead = (curNode->getStructType() == LoopCond ? curNode : curNode->getLoopHead());
			BasicBlock *follLoopHead = curNode->getCondFollow()->getLoopHead();

			// analyse whether this is a jump into/outof a loop
			if (myLoopHead != follLoopHead) {
				// we want to find the branch that the latch node is on for a jump out of a loop
				if (myLoopHead) {
					BasicBlock *myLoopLatch = myLoopHead->getLatchNode();

					// does the then branch goto the loop latch?
					if (oEdges[BTHEN]->isAncestorOf(myLoopLatch) || oEdges[BTHEN] == myLoopLatch) {
						curNode->setUnstructType(JumpInOutLoop);
						curNode->setCondType(IfElse);
					}
					// does the else branch goto the loop latch?
					else if (oEdges[BELSE]->isAncestorOf(myLoopLatch) || oEdges[BELSE] == myLoopLatch) {
						curNode->setUnstructType(JumpInOutLoop);
						curNode->setCondType(IfThen);
					}
				}

				if (curNode->getUnstructType() == Structured && follLoopHead) {
					// find the branch that the loop head is on for a jump into a loop body. If a branch has already
					// been found, then it will match this one anyway

					// does the else branch goto the loop head?
					if (oEdges[BTHEN]->isAncestorOf(follLoopHead) || oEdges[BTHEN] == follLoopHead) {
						curNode->setUnstructType(JumpInOutLoop);
						curNode->setCondType(IfElse);
					}

					// does the else branch goto the loop head?
					else if (oEdges[BELSE]->isAncestorOf(follLoopHead) || oEdges[BELSE] == follLoopHead) {
						curNode->setUnstructType(JumpInOutLoop);
						curNode->setCondType(IfThen);
					}
				}
			}

			// this is a jump into a case body if either of its children don't have the same same case header as itself
			if (curNode->getUnstructType() == Structured
			 && (curNode->getCaseHead() != curNode->getOutEdges()[BTHEN]->getCaseHead()
			  || curNode->getCaseHead() != curNode->getOutEdges()[BELSE]->getCaseHead())) {
				BasicBlock *myCaseHead = curNode->getCaseHead();
				BasicBlock *thenCaseHead = curNode->getOutEdges()[BTHEN]->getCaseHead();
				BasicBlock *elseCaseHead = curNode->getOutEdges()[BELSE]->getCaseHead();

				if (thenCaseHead == myCaseHead
				 && (!myCaseHead || elseCaseHead != myCaseHead->getCondFollow())) {
					curNode->setUnstructType(JumpIntoCase);
					curNode->setCondType(IfElse);
				} else if (elseCaseHead == myCaseHead
				        && (!myCaseHead || thenCaseHead != myCaseHead->getCondFollow())) {
					curNode->setUnstructType(JumpIntoCase);
					curNode->setCondType(IfThen);
				}
			}
		}

		// for 2 way conditional headers that don't have a follow (i.e. are the source of a back edge) and haven't been
		// structured as latching nodes, set their follow to be the non-back edge child.
		if (curNode->getStructType() == Cond
		 && !curNode->getCondFollow()
		 && curNode->getCondType() != Case
		 && curNode->getUnstructType() == Structured) {
			// latching nodes will already have been reset to Seq structured type
			if (curNode->hasBackEdge()) {
				if (curNode->hasBackEdgeTo(curNode->getOutEdges()[BTHEN])) {
					curNode->setCondType(IfThen);
					curNode->setCondFollow(curNode->getOutEdges()[BELSE]);
				} else {
					curNode->setCondType(IfElse);
					curNode->setCondFollow(curNode->getOutEdges()[BTHEN]);
				}
			}
		}
	}
}

/**
 * \brief Structures the control flow graph.
 */
void
Cfg::structure()
{
	if (structured) {
		unTraverse();
		return;
	}
	if (!setTimeStamps())
		return;
	findImmedPDom();
	if (!Boomerang::get().noDecompile) {
		structConds();
		structLoops();
		checkConds();
	}
	structured = true;
}

#if 0 // Cruft?
void
Cfg::addJunctionStatements()
{
	for (const auto &bb : m_listBB) {
		if (bb->getInEdges().size() > 1 && !dynamic_cast<JunctionStatement *>(bb->getFirstStmt())) {
			assert(bb->getRTLs());
			auto j = new JunctionStatement();
			j->setBB(bb);
			bb->getRTLs()->front()->prependStmt(j);
		}
	}
}

void
Cfg::removeJunctionStatements()
{
	for (const auto &bb : m_listBB) {
		if (dynamic_cast<JunctionStatement *>(bb->getFirstStmt())) {
			assert(bb->getRTLs());
			bb->getRTLs()->front()->deleteFirstStmt();
		}
	}
}
#endif

void
Cfg::removeUnneededLabels(HLLCode *hll)
{
	hll->RemoveUnusedLabels(Ordering.size());
}

#define BBINDEX 0     // Non zero to print <index>: before <statement number>
#define BACK_EDGES 0  // Non zero to generate green back edges
void
Cfg::generateDot(std::ostream &os) const
{
	const BasicBlock *ret = nullptr;

	// The nodes
	for (const auto &bb : m_listBB) {
		os << std::hex
		   << "\t\tbb" << bb->getLowAddr()
		   << " [label=\""
		   << std::dec;

#if BBINDEX
		os << indices[bb];
#endif
		// Print the first statement's number,
		// or the BB's address if no statements.
		if (auto first = bb->getFirstStmt()) {
#if BBINDEX
			os << ":";
#endif
			os << first->getNumber() << " ";
		} else {
			os << "bb" << std::hex << bb->getLowAddr() << std::dec << " ";
		}

		switch (bb->getType()) {
		case INCOMPLETE: os << "incomplete"; break;
		case INVALID:    os << "invalid";    break;
		case FALL:       os << "fall";       break;
		case ONEWAY:     os << "oneway";     break;
		case TWOWAY:
			os << "twoway";
			if (bb->getCond()) {
				os << "\\n";
				bb->getCond()->print(os);
			}
			os << "\",shape=diamond];\n";
			continue;
		case NWAY:
			os << "nway";
			if (auto dest = bb->getDest())
				os << "\\n" << *dest;
			os << "\",shape=trapezium];\n";
			continue;
		case COMPJUMP:   os << "compjump";   break;
		case COMPCALL:   os << "compcall";   break;
		case CALL:
			os << "call";
			if (auto dest = bb->getDestProc())
				os << "\\n" << dest->getName();
			break;
		case RET:
			os << "ret\",shape=triangle];\n";
			// Remember the (unique) return BB
			ret = bb;
			continue;
		}
		os << "\"];\n";
	}

	// Force the one return node to be at the bottom (max rank).
	// Otherwise, with all its in-edges, it will end up in the middle.
	if (ret)
		os << "\t\t{rank=max; bb" << std::hex << ret->getLowAddr() << std::dec << "}\n";

	// Close the subgraph
	os << "\t}\n";

	// Now the edges
	for (const auto &bb : m_listBB) {
		const auto &outEdges = bb->getOutEdges();
		for (unsigned int j = 0; j < outEdges.size(); ++j) {
			os << std::hex
			   << "\tbb" << bb->getLowAddr()
			   << " -> bb" << outEdges[j]->getLowAddr()
			   << " [color=blue"
			   << std::dec;
			if (bb->getType() == TWOWAY) {
				if (j == 0)
					os << ",label=\"true\"";
				else
					os << ",label=\"false\"";
			}
			os << "];\n";
		}
	}
#if BACK_EDGES
	for (const auto &bb : m_listBB) {
		const auto &inEdges = bb->getInEdges();
		for (const auto &edge : inEdges) {
			os << std::hex
			   << "\tbb" << bb->getLowAddr()
			   << " -> bb" << edge->getLowAddr()
			   << " [color=green];\n"
			   << std::dec;
		}
	}
#endif
}



////////////////////////////////////
//           Liveness             //
////////////////////////////////////

static void
updateWorkListRev(BasicBlock *currBB, std::list<BasicBlock *> &workList, std::set<BasicBlock *> &workSet)
{
	// Insert inedges of currBB into the worklist, unless already there
	const auto &ins = currBB->getInEdges();
	for (const auto &currIn : ins) {
		if (!workSet.count(currIn)) {
			workList.push_front(currIn);
			workSet.insert(currIn);
		}
	}
}

static int progress = 0;
void
Cfg::findInterferences(ConnectionGraph &cg)
{
	if (m_listBB.empty()) return;

	auto workList = std::list<BasicBlock *>(m_listBB);  // List of BBs still to be processed
	// Set of the same; used for quick membership test
	auto workSet = std::set<BasicBlock *>(m_listBB.begin(), m_listBB.end());

	bool change;
	int count = 0;
	while (!workList.empty() && count < 100000) {
		++count;  // prevent infinite loop
		if (++progress > 20) {
			std::cout << "i" << std::flush;
			progress = 0;
		}
		BasicBlock *currBB = workList.back();
		workList.pop_back();
		workSet.erase(currBB);
		// Calculate live locations and interferences
		change = currBB->calcLiveness(cg, myProc);
		if (change) {
			if (DEBUG_LIVENESS) {
				LOG << "Revisiting BB ending with stmt ";
				Statement *last = nullptr;
				if (!currBB->m_pRtls->empty()) {
					const auto &stmts = currBB->m_pRtls->back()->getList();
					if (!stmts.empty())
						last = stmts.back();
				}
				if (last)
					LOG << last->getNumber();
				else
					LOG << "<none>";
				LOG << " due to change\n";
			}
			updateWorkListRev(currBB, workList, workSet);
		}
	}
}

/**
 * Split the given BB at the RTL given, and turn it into the BranchStatement
 * given.  Sort out all the in- and out-edges.
 *
 * \code
 *  bbA-> +----+    +----+ <-bbA
 * Change | A  | to | A  | where A and B could be empty.  S is the string
 *        |    |    |    | instruction (which will branch to itself and to the
 *  *ri-> +----+    +----+ start of the next instruction, i.e. the start of B,
 *        | S  |      |    if B is non-empty).
 *        +----+      V
 *        | B  |    +----+ <-bbS
 *        |    |    +-b1-+            b1 is a branch for the skip part
 *        +----+      |               (taken edge to bbB, fall edge to bbR).
 *                    V
 *                  +----+ <-bbR
 *                  | S' |            S' = S less the skip and repeat parts.
 *                  +-b2-+            b2 is a branch for the repeat part
 *                    |               (taken edge to bbS, fall edge to bbB).
 *                    V
 *                  +----+ <-bbB
 *                  | B  |
 *                  |    |
 *                  +----+
 * \endcode
 *
 * S is an RTL with 6 statements representing one string instruction (so this
 * function is highly specialised for the job of replacing the %SKIP and %RPT
 * parts of string instructions).
 */
void
Cfg::splitForBranch(BasicBlock *bbA, std::list<RTL *>::iterator ri)
{
#if 0
	std::cerr << "splitForBranch before:\n";
	bbA->print(std::cerr);
	std::cerr << "\n";
#endif

	assert(ri != bbA->m_pRtls->end());
	auto rtl = *ri;

	// Split off A if it exists.
	BasicBlock *bbS = bbA;
	if (ri != bbA->m_pRtls->begin()) {
		bbS = splitBB(bbA, ri);
	} else {
		bbA = nullptr;
	}

	ADDRESS addr = rtl->getAddress();
	auto &stmts = rtl->getList();
	assert(stmts.size() >= 4);  // They vary; at least 5 or 6

	// Replace %SKIP and %RPT assignments with b1 and b2, respectively.
	auto b1 = new BranchStatement(addr + 2);  // FIXME:  Assumes string instruction is 2 bytes long.
	if (auto as = dynamic_cast<Assign *>(stmts.front()))
		b1->setCondExpr(as->getRight());
	stmts.front() = b1;  // FIXME:  Leaks the old statement.
	auto b2 = new BranchStatement(addr);
	if (auto as = dynamic_cast<Assign *>(stmts.back()))
		b2->setCondExpr(as->getRight());
	stmts.back() = b2;  // FIXME:  Leaks the old statement.

	// Split S's RTL after b1.  New RTL contains S'.
	auto rtlR = new RTL(addr + 1);  // FIXME:  Assumes string instruction is > 1 byte long.
	auto &tail = rtlR->getList();
	tail.splice(tail.begin(), stmts, ++stmts.begin(), stmts.end());
	bbS->m_pRtls->insert(++ri, rtlR);
	auto bbR = splitBB(bbS, --ri);

	// Split off B if it exists.
	BasicBlock *bbB;
	if (++ri != bbR->m_pRtls->end()) {
		bbB = splitBB(bbR, ri);
	} else {
		// Assume original BB is a fall BB and falls to an existing B.
		assert(bbR->m_OutEdges.size() == 1);
		bbB = bbR->m_OutEdges[0];
	}

	// Set the out-edges for S.  First is the taken (true) leg.
	bbS->updateType(TWOWAY);
	bbS->addEdge(bbB);
	std::swap(bbS->m_OutEdges[0], bbS->m_OutEdges[1]);

	// Set the out-edges for R.
	bbR->updateType(TWOWAY);
	bbR->addEdge(bbS);
	std::swap(bbR->m_OutEdges[0], bbR->m_OutEdges[1]);

#if 0
	std::cerr << "splitForBranch after:\n";
	if (bbA) bbA->print(std::cerr); else std::cerr << "<null>\n";
	bbS->print(std::cerr);
	bbR->print(std::cerr);
	bbB->print(std::cerr);
	std::cerr << "\n";
#endif
}

/*
 * \brief Check for indirect jumps and calls in all my BBs; decode any new
 * code.
 *
 * Check for indirect jumps and calls.  If any found, decode the extra code
 * and return true.
 */
bool
Cfg::decodeIndirectJmp(UserProc *proc)
{
	bool res = false;
	for (const auto &bb : m_listBB) {
		res |= bb->decodeIndirectJmp(proc);
	}
	return res;
}

/**
 * \brief Change the BB enclosing stmt to be CALL, not COMPCALL.
 */
void
Cfg::undoComputedBB(Statement *stmt)
{
	for (const auto &bb : m_listBB) {
		if (bb->undoComputedBB(stmt))
			break;
	}
}

/**
 * \brief Find or create an implicit assign for x.
 */
Statement *
Cfg::findImplicitAssign(Exp *x)
{
	Statement *def;
	auto it = implicitMap.find(x);
	if (it == implicitMap.end()) {
		// A use with no explicit definition. Create a new implicit assignment
		x = x->clone();  // In case the original gets changed
		def = new ImplicitAssign(x);
		entryBB->prependStmt(def, myProc);
		// Remember it for later so we don't insert more than one implicit assignment for any one location
		// We don't clone the copy in the map. So if the location is a m[...], the same type information is available in
		// the definition as at all uses
		implicitMap[x] = def;
	} else {
		// Use an existing implicit assignment
		def = it->second;
	}
	return def;
}

/**
 * \brief Find the existing implicit assign for x (if any).
 */
Statement *
Cfg::findTheImplicitAssign(Exp *x)
{
	// As per the above, but don't create an implicit if it doesn't already exist
	auto it = implicitMap.find(x);
	if (it == implicitMap.end())
		return nullptr;
	return it->second;
}

/**
 * \brief Find existing implicit assign for parameter p.
 */
Statement *
Cfg::findImplicitParamAssign(Parameter *param)
{
	// As per the above, but for parameters (signatures don't get updated with opParams)
	auto it = implicitMap.find(param->exp);
	if (it == implicitMap.end()) {
		Exp *eParam = Location::param(param->name);
		it = implicitMap.find(eParam);
	}
	if (it == implicitMap.end())
		return nullptr;
	return it->second;
}

/**
 * \brief Remove an existing implicit assignment for x.
 */
void
Cfg::removeImplicitAssign(Exp *x)
{
	auto it = implicitMap.find(x);
	assert(it != implicitMap.end());
	Statement *ia = it->second;
	implicitMap.erase(it);        // Delete the mapping
	myProc->removeStatement(ia);  // Remove the actual implicit assignment statement as well
}
