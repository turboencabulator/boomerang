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
#include "log.h"
#include "proc.h"
#include "rtl.h"
#include "signature.h"
#include "statement.h"
#include "types.h"

#include <algorithm>    // For std::find()
#include <sstream>

#include <cassert>
#include <cstring>

static void delete_lrtls(std::list<RTL *> *pLrtl);

/**
 * \brief Constructor.
 */
Cfg::Cfg()
{
}

/**
 * \brief Destructor.
 * \note Destructs the component BBs as well.
 */
Cfg::~Cfg()
{
	// Delete the BBs
	for (const auto &bb : m_listBB) {
		delete bb;
	}
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
	callSites.clear();
	lastLabel = 0;
}

/**
 * \brief Copy constructor.
 */
const Cfg &
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
 * initializes its list of RTLs with pRtls, its type to the given type, and
 * allocates enough space to hold pointers to the out-edges (based on given
 * numOutEdges).
 *
 * The native address associated with the start of the BB is taken from pRtls,
 * and added to the map (unless 0).  NOTE: You cannot assume that the returned
 * BB will have the RTL associated with pStart as its first RTL, since the BB
 * could be split.  You can however assume that the returned BB is suitable
 * for adding out edges (i.e. if the BB is split, you get the "bottom" part of
 * the BB, not the "top" (with lower addresses at the "top").  Returns null if
 * not successful, or if there already exists a completed BB at this address
 * (this can happen with certain kinds of forward branches).
 *
 * \param pRtls         List of pointers to RTLs to initialise the BB with.
 * \param bbType        The type of the BB (e.g. TWOWAY).
 * \param iNumOutEdges  Number of out edges this BB will eventually have.
 * \returns             Pointer to the newly created BB, or null if there is
 *                      already an incomplete BB with the same address.
 */
BasicBlock *
Cfg::newBB(std::list<RTL *> *pRtls, BBTYPE bbType, int iNumOutEdges) throw (BBAlreadyExistsError)
{
	MAPBB::iterator mi;
	BasicBlock *pBB;

	assert(bbType != ONEWAY   || iNumOutEdges == 1);
	assert(bbType != TWOWAY   || iNumOutEdges == 2);
	assert(bbType != CALL     || iNumOutEdges <= 1);
	assert(bbType != RET      || iNumOutEdges == 0);
	assert(bbType != FALL     || iNumOutEdges == 1);
	assert(bbType != COMPJUMP || iNumOutEdges == 0);
	assert(bbType != COMPCALL || iNumOutEdges == 1);
	assert(bbType != INVALID  || iNumOutEdges == 0);

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
	bool bDone = false;
	if (addr != 0) {
		mi = m_mapBB.find(addr);
		if (mi != m_mapBB.end() && mi->second) {
			pBB = mi->second;
			// It should be incomplete, or the pBB there should be zero (we have called Label but not yet created the BB
			// for it).  Else we have duplicated BBs. Note: this can happen with forward jumps into the middle of a
			// loop, so not error
			if (!pBB->m_bIncomplete) {
				// This list of RTLs is not needed now
				delete_lrtls(pRtls);
				if (VERBOSE)
					LOG << "throwing BBAlreadyExistsError\n";
				throw BBAlreadyExistsError(pBB);
			} else {
				// Fill in the details, and return it
				pBB->setRTLs(pRtls);
				pBB->m_nodeType = bbType;
				pBB->m_iNumOutEdges = iNumOutEdges;
				pBB->m_bIncomplete = false;
			}
			bDone = true;
		}
	}
	if (!bDone) {
		// Else add a new BB to the back of the current list.
		pBB = new BasicBlock(pRtls, bbType, iNumOutEdges);
		m_listBB.push_back(pBB);

		// Also add the address to the map from native (source) address to
		// pointer to BB, unless it's zero
		if (addr != 0) {
			m_mapBB[addr] = pBB;  // Insert the mapping
			mi = m_mapBB.find(addr);
		}
	}

	if (addr != 0 && (mi != m_mapBB.end())) {
		// Existing New         +---+ Top of new
		//          +---+       +---+
		//  +---+   |   |       +---+ Fall through
		//  |   |   |   | =>    |   |
		//  |   |   |   |       |   | Existing; rest of new discarded
		//  +---+   +---+       +---+
		//
		// Check for overlap of the just added BB with the next BB (address wise).  If there is an overlap, truncate the
		// std::list<Exp*> for the new BB to not overlap, and make this a fall through BB.
		// We still want to do this even if the new BB overlaps with an incomplete BB, though in this case,
		// splitBB needs to fill in the details for the "bottom" BB of the split.
		// Also, in this case, we return a pointer to the newly completed BB, so it will get out edges added
		// (if required). In the other case (i.e. we overlap with an exising, completed BB), we want to return nullptr, since
		// the out edges are already created.
		if (++mi != m_mapBB.end()) {
			BasicBlock *pNextBB = mi->second;
			ADDRESS uNext = mi->first;
			bool bIncomplete = pNextBB->m_bIncomplete;
			if (uNext <= pRtls->back()->getAddress()) {
				// Need to truncate the current BB. We use splitBB(), but pass it pNextBB so it doesn't create a new BB
				// for the "bottom" BB of the split pair
				splitBB(pBB, uNext, pNextBB);
				// If the overlapped BB was incomplete, return the "bottom" part of the BB, so adding out edges will
				// work properly.
				if (bIncomplete) {
					return pNextBB;
				}
				// However, if the overlapping BB was already complete, return nullptr, so out edges won't be added twice
				throw BBAlreadyExistsError(pNextBB);
			}
		}

		// Existing New         +---+ Top of existing
		//  +---+               +---+
		//  |   |   +---+       +---+ Fall through
		//  |   |   |   | =>    |   |
		//  |   |   |   |       |   | New; rest of existing discarded
		//  +---+   +---+       +---+
		// Note: no need to check the other way around, because in this case, we will have called Cfg::Label(), and it
		// will have split the existing BB already.
	}
	assert(pBB);
	return pBB;
}

/**
 * Allocates space for a new, incomplete BB, and the given address is added to
 * the map.  This BB will have to be completed before calling wellFormCfg().
 * This function will commonly be called via addOutEdge().
 *
 * Use this function when there are outedges to BBs that are not created yet.
 */
BasicBlock *
Cfg::newIncompleteBB(ADDRESS addr)
{
	// Create a new (basically empty) BB
	auto pBB = new BasicBlock();
	// Add it to the list
	m_listBB.push_back(pBB);
	m_mapBB[addr] = pBB;  // Insert the mapping
	return pBB;
}

/**
 * \brief Add an out edge to this BB (and the in-edge to the dest BB).  May
 * also set a label.
 *
 * Adds an out-edge to the basic block pBB by filling in the first slot that
 * is empty.
 *
 * \note Overloaded with address as 2nd argument.  A pointer to a BB is given
 * here.
 *
 * \note Does not increment m_iNumOutEdges; this is supposed to be constant
 * for a BB.  (But see BasicBlock::addNewOutEdge().)
 *
 * \param pBB      Source BB (to have the out edge added to).
 * \param pDestBB  Destination BB (to have the out edge point to).
 */
void
Cfg::addOutEdge(BasicBlock *pBB, BasicBlock *pDestBB, bool bSetLabel /* = false */)
{
	// Add the given BB pointer to the list of out edges
	pBB->m_OutEdges.push_back(pDestBB);
	// Note that the number of out edges is set at constructor time, not incremented here.
	// Add the in edge to the destination BB
	pDestBB->m_InEdges.push_back(pBB);
	if (bSetLabel) setLabel(pDestBB);   // Indicate "label required"
}

/**
 * \brief Add an out edge to this BB (and the in-edge to the dest BB).  May
 * also set a label.
 *
 * Adds an out-edge to the basic block pBB by filling in the first slot that
 * is empty.
 *
 * \note Calls the above.  An address is given here; the out edge will be
 * filled in as a pointer to a BB.  An incomplete BB will be created if
 * required.  If bSetLabel is true, the destination BB will have its "label
 * required" bit set.
 *
 * \param pBB        Source BB (to have the out edge added to).
 * \param addr       Source address of destination (the out edge is to point
 *                   to the BB whose lowest address is addr).
 * \param bSetLabel  If true, set a label at the destination address.  Set
 *                   true on "true" branches of labels.
 */
void
Cfg::addOutEdge(BasicBlock *pBB, ADDRESS addr, bool bSetLabel /* = false */)
{
	// Check to see if the address is in the map, i.e. we already have a BB for this address
	BasicBlock *pDestBB;
	auto it = m_mapBB.find(addr);
	if (it != m_mapBB.end() && it->second) {
		// Just add this BasicBlock* to the list of out edges
		pDestBB = it->second;
	} else {
		// Else, create a new incomplete BB, add that to the map, and add the new BB as the out edge
		pDestBB = newIncompleteBB(addr);
	}
	addOutEdge(pBB, pDestBB, bSetLabel);
}

/**
 * \brief Return true if the given address is the start of a basic block,
 * complete or not.
 *
 * Just checks to see if there exists a BB starting with this native address.
 * If not, the address is NOT added to the map of labels to BBs.
 *
 * \param uNativeAddr  Native address to look up.
 * \returns            true if uNativeAddr starts a BB.
 *
 * \note Must ignore entries with a null pBB, since these are caused by calls
 * to Label that failed, i.e. the instruction is not decoded yet.
 */
bool
Cfg::existsBB(ADDRESS uNativeAddr) const
{
	auto mi = m_mapBB.find(uNativeAddr);
	return (mi != m_mapBB.end() && mi->second);
}

/**
 * Split the given basic block at the RTL associated with uNativeAddr.  The
 * first node's type becomes fall-through and ends at the RTL prior to that
 * associated with uNativeAddr.  The second node's type becomes the type of
 * the original basic block (pBB), and its out-edges are those of the original
 * basic block.  In edges of the new BB's descendants are changed.
 *
 * If pNewBB is non-null, it is retained as the "bottom" part of the split,
 * i.e. splitBB just changes the "top" BB to not overlap the existing one.
 *
 * \pre Assumes uNativeAddr is an address within the boundaries of the given
 * basic block.
 *
 * \param pBB          Pointer to the BB to be split.
 * \param uNativeAddr  Address of RTL to become the start of the new BB.
 * \param pNewBB       If non-null, it remains as the "bottom" part of the BB,
 *                     and splitBB only modifies the top part to not overlap.
 * \param bDelRtls     If true, deletes the RTLs removed from the existing BB
 *                     after the split point.  Only used if there is an
 *                     overlap with existing instructions
 * \returns            A pointer to the "bottom" (new) part of the split BB.
 */
BasicBlock *
Cfg::splitBB(BasicBlock *pBB, ADDRESS uNativeAddr, BasicBlock *pNewBB /* = nullptr */, bool bDelRtls /* = false */)
{
	// First find which RTL has the split address; note that this could fail (e.g. label in the middle of an
	// instruction, or some weird delay slot effects)
	auto ri = pBB->m_pRtls->begin();
	for (; ri != pBB->m_pRtls->end(); ++ri) {
		if ((*ri)->getAddress() == uNativeAddr)
			break;
	}
	if (ri == pBB->m_pRtls->end()) {
		std::cerr << "could not split BB at " << std::hex << pBB->getLowAddr() << " at split address " << uNativeAddr << std::endl;
		return pBB;
	}

	// If necessary, set up a new basic block with information from the original bb
	if (!pNewBB) {
		pNewBB = new BasicBlock(*pBB);
		// Put it in the graph
		m_listBB.push_back(pNewBB);
		// Put the implicit label into the map. Need to do this before the addOutEdge() below
		m_mapBB[uNativeAddr] = pNewBB;

		// But we don't want the top BB's in edges; our only in-edge should be the out edge from the top BB
		pNewBB->m_InEdges.clear();
		// There must be a label here; else would not be splitting.  Give it a new label
		pNewBB->m_iLabelNum = ++lastLabel;
		// The "bottom" BB now starts at the implicit label, so we create a new list that starts at ri. We need a new
		// list, since it is different from the original BB's list. We don't have to "deep copy" the RTLs themselves,
		// since they will never overlap
		pNewBB->setRTLs(new std::list<RTL *>(ri, pBB->m_pRtls->end()));
	} else if (pNewBB->m_bIncomplete) {
		// We have an existing BB and a map entry, but no details except for in-edges and m_bHasLabel.
		// First save the in-edges and m_iLabelNum
		auto ins = pNewBB->m_InEdges;
		auto label = pNewBB->m_iLabelNum;
		// Copy over the details now, completing the bottom BB
		*pNewBB = *pBB;  // Assign the BB, copying fields. This will set m_bIncomplete false

		// Replace the in edges (likely only one)
		pNewBB->m_InEdges = ins;
		// Replace the label (must be one, since we are splitting this BB!)
		pNewBB->m_iLabelNum = label;
		// The "bottom" BB now starts at the implicit label
		// We need to create a new list of RTLs, as per above
		pNewBB->setRTLs(new std::list<RTL *>(ri, pBB->m_pRtls->end()));
	}
	// else pNewBB exists and is complete. We don't want to change the complete BB in any way, except to later add one
	// in-edge

	// Fix the in-edges of pBB's descendants. They are now pNewBB
	// Note: you can't believe m_iNumOutEdges at the time that this function may get called
	for (const auto &pDescendant : pBB->m_OutEdges) {
		// Search through the in edges for pBB (old ancestor)
		bool found = false;
		for (auto &edge : pDescendant->m_InEdges) {
			if (edge == pBB) {
				// Replace with a pointer to the new ancestor
				edge = pNewBB;
				found = true;
				break;
			}
		}
		// That pointer should have been found!
		assert(found);
	}

	// The old BB needs to have part of its list of RTLs erased, since the instructions overlap
	if (bDelRtls) {
		// Delete the RTLs they point to
		for (auto it = ri; it != pBB->m_pRtls->end(); ++it) {
			delete *it;
		}
	}
	// Delete the list of pointers
	pBB->m_pRtls->erase(ri, pBB->m_pRtls->end());

	// Update original ("top") basic block's info and make it a fall-through
	pBB->updateType(FALL, 1);
	// Erase any existing out edges
	pBB->m_OutEdges.clear();
	addOutEdge(pBB, uNativeAddr);
	return pNewBB;
}

/**
 * Checks whether the given native address is a label (explicit or non
 * explicit) or not.  Returns true if the native address is that of an
 * explicit or non explicit label, false otherwise.  Returns false for
 * incomplete BBs.  So it returns true iff the address has already been
 * decoded in some BB.  If it was not already a label (i.e. the first
 * instruction of some BB), the BB is split so that it becomes a label.
 *
 * Explicit labels are addresses that have already been tagged as being labels
 * due to transfers of control to that address, and are therefore the start of
 * some BB.  Non explicit labels are those that belong to basic blocks that
 * have already been constructed (i.e. have previously been parsed) and now
 * need to be made explicit labels.  In the case of non explicit labels, the
 * basic block is split into two and types and edges are adjusted accordingly.
 * If pCurBB is the BB that gets split, it is changed to point to the address
 * of the new (lower) part of the split BB.
 *
 * If there is an incomplete entry in the table for this address which
 * overlaps with a completed address, the completed BB is split and the BB for
 * this address is completed.
 *
 * \param uNativeAddr  Native (source) address to check.
 * \param pCurBB       See above.
 * \returns            true if uNativeAddr is a label, i.e. (now) the start of
 *                     a BB.
 *
 * \note pCurBB may be modified (as above).
 */
bool
Cfg::label(ADDRESS uNativeAddr, BasicBlock *&pCurBB)
{
	// check if the native address is in the map already (explicit label)
	auto mi = m_mapBB.find(uNativeAddr);
	if (mi == m_mapBB.end()) {  // not in the map
		// If not an explicit label, temporarily add the address to the map
		m_mapBB[uNativeAddr] = (BasicBlock *)0;  // no BasicBlock* yet

		// get an iterator to the new native address and check if the previous element in the (sorted) map overlaps
		// this new native address; if so, it's a non-explicit label which needs to be made explicit by splitting the
		// previous BB.
		mi = m_mapBB.find(uNativeAddr);

		auto newi = mi;
		bool bSplit = false;
		BasicBlock *pPrevBB = nullptr;
		if (newi != m_mapBB.begin()) {
			pPrevBB = (*--mi).second;
			if (!pPrevBB->m_bIncomplete
			 && (pPrevBB->getLowAddr() <  uNativeAddr)
			 && (pPrevBB->getHiAddr()  >= uNativeAddr)) {
				bSplit = true;
			}
		}
		if (bSplit) {
			// Non-explicit label. Split the previous BB
			BasicBlock *pNewBB = splitBB(pPrevBB, uNativeAddr);
			if (pCurBB == pPrevBB) {
				// This means that the BB that we are expecting to use, usually to add out edges, has changed. We must
				// change this pointer so that the right BB gets the out edges. However, if the new BB is not the BB of
				// interest, we mustn't change pCurBB
				pCurBB = pNewBB;
			}
			return true;  // wasn't a label, but already parsed
		} else {  // not a non-explicit label
			// We don't have to erase this map entry. Having a null BasicBlock pointer is coped with in newBB() and
			// addOutEdge(); when eventually the BB is created, it will replace this entry.  We should be currently
			// processing this BB. The map will be corrected when newBB is called with this address.
			return false;  // was not already parsed
		}
	} else {  // We already have uNativeAddr in the map
		if (mi->second && !mi->second->m_bIncomplete) {
			// There is a complete BB here. Return true.
			return true;
		}

		// We are finalising an incomplete BB. Still need to check previous map entry to see if there is a complete BB
		// overlapping
		bool bSplit = false;
		BasicBlock *pPrevBB, *pBB = mi->second;
		if (mi != m_mapBB.begin()) {
			pPrevBB = (*--mi).second;
			if (!pPrevBB->m_bIncomplete
			 && (pPrevBB->getLowAddr() <  uNativeAddr)
			 && (pPrevBB->getHiAddr()  >= uNativeAddr))
				bSplit = true;
		}
		if (bSplit) {
			// Pass the third parameter to splitBB, because we already have an (incomplete) BB for the "bottom" BB of
			// the split
			splitBB(pPrevBB, uNativeAddr, pBB);  // non-explicit label
			return true;  // wasn't a label, but already parsed
		}
		// A non overlapping, incomplete entry is in the map.
		return false;
	}
}

/**
 * \brief Return true if there is an incomplete BB already at this address.
 *
 * Checks whether the given native address is in the map.  If not, returns
 * false.  If so, returns true if it is incomplete.  Otherwise, returns false.
 *
 * \param uAddr  Address to look up.
 * \returns      true if uAddr starts an incomplete BB.
 */
bool
Cfg::isIncomplete(ADDRESS uAddr) const
{
	auto mi = m_mapBB.find(uAddr);
	if (mi == m_mapBB.end())
		// No entry at all
		return false;
	// Else, there is a BB there. If it's incomplete, return true
	BasicBlock *pBB = mi->second;
	return pBB->m_bIncomplete;
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
		if (bb->m_bIncomplete) {
			m_bWellFormed = false;
			auto itm = m_mapBB.begin();
			for (; itm != m_mapBB.end(); ++itm)
				if (itm->second == bb) break;
			if (itm == m_mapBB.end())
				std::cerr << "WellFormCfg: incomplete BB not even in map!\n";
			else
				std::cerr << "WellFormCfg: BB with native address " << std::hex << itm->first
				          << " is incomplete\n";
		} else {
			// Complete. Test the out edges
			assert((int)bb->m_OutEdges.size() == bb->m_iNumOutEdges);
			for (int i = 0; i < bb->m_iNumOutEdges; ++i) {
				// check if address is interprocedural
				//if (!bb->m_OutEdgeInterProc[i])
				{
					const auto &outedge = bb->m_OutEdges[i];

					// Check that the out edge has been written (i.e. nonzero)
					if (!outedge) {
						m_bWellFormed = false;  // At least one problem
						ADDRESS addr = bb->getLowAddr();
						std::cerr << "WellFormCfg: BB with native address " << std::hex << addr
						          << " is missing outedge " << i << std::endl;
					} else {
						// Check that there is a corresponding in edge from the
						// child to here
						auto ii = std::find(outedge->m_InEdges.begin(), outedge->m_InEdges.end(), bb);
						if (ii == outedge->m_InEdges.end()) {
							std::cerr << "WellFormCfg: No in edge to BB at " << std::hex << bb->getLowAddr()
							          << " from successor BB at " << outedge->getLowAddr() << std::endl;
							m_bWellFormed = false;  // At least one problem
						}
					}
				}
			}
			// Also check that each in edge has a corresponding out edge to here (could have an extra in-edge, for
			// example)
			for (const auto &inedge : bb->m_InEdges) {
				auto oo = std::find(inedge->m_OutEdges.begin(), inedge->m_OutEdges.end(), bb);
				if (oo == inedge->m_OutEdges.end()) {
					std::cerr << "WellFormCfg: No out edge to BB at " << std::hex << bb->getLowAddr()
					          << " from predecessor BB at " << inedge->getLowAddr() << std::endl;
					m_bWellFormed = false;  // At least one problem
				}
			}
		}
	}
	return m_bWellFormed;
}

/**
 * Given two basic blocks that belong to a well-formed graph, merges the
 * second block onto the first one and returns the new block.  The in and out
 * edges links are updated accordingly.  Note that two basic blocks can only
 * be merged if each has a unique out-edge and in-edge respectively, and these
 * edges correspond to each other.
 *
 * \returns  true if the blocks are merged.
 */
bool
Cfg::mergeBBs(BasicBlock *pb1, BasicBlock *pb2)
{
	// Can only merge if pb1 has only one outedge to pb2, and pb2 has only one in-edge, from pb1. This can only be done
	// after the in-edges are done, which can only be done on a well formed CFG.
	if (!m_bWellFormed) return false;
	if (pb1->m_iNumOutEdges != 1) return false;
	if (pb2->m_InEdges.size() != 1) return false;
	if (pb1->m_OutEdges[0] != pb2) return false;
	if (pb2->m_InEdges[0] != pb1) return false;

	// Merge them! We remove pb1 rather than pb2, since this is also what is needed for many optimisations, e.g. jump to
	// jump.
	completeMerge(pb1, pb2, true);
	return true;
}

/**
 * Completes the merge of pb1 and pb2 by adjusting in and out edges.  No
 * checks are made that the merge is valid (hence this is a private function).
 * Deletes pb1 if bDelete is true.
 *
 * \param pb1,pb2  Pointers to the two BBs to merge.
 * \param bDelete  If true, pb1 is deleted as well.
 */
void
Cfg::completeMerge(BasicBlock *pb1, BasicBlock *pb2, bool bDelete = false)
{
	// First we replace all of pb1's predecessors' out edges that used to point to pb1 (usually only one of these) with
	// pb2
	for (const auto &pPred : pb1->m_InEdges) {
		for (int j = 0; j < pPred->m_iNumOutEdges; ++j) {
			if (pPred->m_OutEdges[j] == pb1)
				pPred->m_OutEdges[j] = pb2;
		}
	}

	// Now we replace pb2's in edges by pb1's inedges
	pb2->m_InEdges = pb1->m_InEdges;

	if (bDelete) {
		// Finally, we delete pb1 from the BB list. Note: remove(pb1) should also work, but it would involve member
		// comparison (not implemented), and also would attempt to remove ALL elements of the list with this value (so
		// it has to search the whole list, instead of an average of half the list as we have here).
		for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
			if (*it == pb1) {
				m_listBB.erase(it);
				break;
			}
		}
	}
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
	// Prepend the RTLs for pb1 to those of pb2. Since they will be pushed to the front of pb2, push them in reverse
	// order
	for (auto it = pb1->m_pRtls->rbegin(); it != pb1->m_pRtls->rend(); ++it) {
		pb2->m_pRtls->push_front(*it);
	}
	completeMerge(pb1, pb2);  // Mash them together
	// pb1 no longer needed. Remove it from the list of BBs.  This will also delete *pb1. It will be a shallow delete,
	// but that's good because we only did shallow copies to *pb2
	auto bbit = std::find(m_listBB.begin(), m_listBB.end(), pb1);
	m_listBB.erase(bbit);
	return true;
}

/**
 * \brief Completely remove a BB from the CFG.
 */
void
Cfg::removeBB(BasicBlock *bb)
{
	auto bbit = std::find(m_listBB.begin(), m_listBB.end(), bb);
	m_listBB.erase(bbit);
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
	for (const auto &bb : m_listBB) {  // Pointer to A
		for (auto it1 = bb->m_OutEdges.begin(); it1 != bb->m_OutEdges.end(); ++it1) {
			BasicBlock *pSucc = (*it1);  // Pointer to J
			if (pSucc->m_InEdges.size() == 1
			 && pSucc->m_OutEdges.size() == 1
			 && pSucc->m_pRtls->size() == 1
			 && pSucc->m_pRtls->front()->getList().size() == 1
			 && pSucc->m_pRtls->front()->getList().front()->isGoto()) {
				// Found an out-edge to an only-jump BB
#if 0
				std::cout << "outedge to jump detected at " << std::hex << bb->getLowAddr()
				          << " to " << pSucc->getLowAddr()
				          << " to " << pSucc->m_OutEdges.front()->getLowAddr() << std::dec << std::endl;
#endif
				// Point this outedge of A to the dest of the jump (B)
				*it1 = pSucc->m_OutEdges.front();
				// Now pSucc still points to J; *it1 points to B.  Almost certainly, we will need a jump in the low
				// level C that may be generated. Also force a label for B
				bb->m_bJumpReqd = true;
				setLabel(*it1);
				// Find the in-edge from B to J; replace this with an in-edge to A
				for (auto &edge : (*it1)->m_InEdges) {
					if (edge == pSucc)
						edge = bb;  // Point to A
				}
				// Remove the in-edge from J to A. First find the in-edge
				auto it2 = std::find(pSucc->m_InEdges.begin(), pSucc->m_InEdges.end(), bb);
				assert(it2 != pSucc->m_InEdges.end());
				pSucc->deleteInEdge(it2);
				// If nothing else uses this BB (J), remove it from the CFG
				if (pSucc->m_InEdges.size() == 0) {
					for (auto it3 = m_listBB.begin(); it3 != m_listBB.end(); ++it3) {
						if (*it3 == pSucc) {
							m_listBB.erase(it3);  // FIXME:  Invalidates iterators
							// And delete the BB
							delete pSucc;
							break;
						}
					}
				}
			}
		}
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

	int first = 0;
	int last = 0;
	unsigned numTraversed;

	if (checkEntryBB()) return false;

	numTraversed = entryBB->DFTOrder(first, last);

	return numTraversed == m_listBB.size();
}

BasicBlock *
Cfg::findRetNode() const
{
	BasicBlock *retNode = nullptr;
	for (const auto &bb : m_listBB) {
		if (bb->getType() == RET) {
			retNode = bb;
			break;
		} else if (bb->getType() == CALL) {
			Proc *p = bb->getCallDestProc();
			if (p && !strcmp(p->getName(), "exit"))
				retNode = bb;
		}
	}
	return retNode;
}

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
	unsigned numTraversed;

	numTraversed = retNode->RevDFTOrder(first, last);

	return numTraversed == m_listBB.size();
}

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
Cfg::isOrphan(ADDRESS uAddr) const
{
	auto mi = m_mapBB.find(uAddr);
	if (mi == m_mapBB.end())
		// No entry at all
		return false;
	// Return true if the first RTL at this address has an address set to 0
	BasicBlock *pBB = mi->second;
	// If it's incomplete, it can't be an orphan
	if (pBB->m_bIncomplete) return false;
	return pBB->m_pRtls->front()->getAddress() == 0;
}

/**
 * \brief Return an index for the given BasicBlock*.
 *
 * Given a pointer to a basic block, return an index (e.g. 0 for the first
 * basic block, 1 for the next, ... n-1 for the last BB.
 *
 * \note     Linear search:  O(N) complexity.
 * \returns  Index, or -1 for unknown BasicBlock*.
 */
int
Cfg::pbbToIndex(BasicBlock *pBB) const
{
	int i = 0;
	for (const auto &bb : m_listBB) {
		if (bb == pBB)
			return i;
		++i;
	}
	return -1;
}

/**
 * Add a call to the set of calls within this procedure.
 *
 * \param call  A call instruction.
 */
void
Cfg::addCall(CallStatement *call)
{
	callSites.insert(call);
}

/**
 * Get the set of calls within this procedure.
 *
 * \returns  The set of calls within this procedure.
 */
std::set<CallStatement *> &
Cfg::getCalls()
{
	return callSites;
}

/**
 * Replace all instances of search with replace.  Can be type sensitive if
 * required.
 *
 * \param search   A location to search for.
 * \param replace  The expression with which to replace it.
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
	bool ch = false;
	for (const auto &bb : m_listBB) {
		for (const auto &rtl : *bb->getRTLs()) {
			ch |= rtl->searchAll(search, result);
		}
	}
	return ch;
}

/**
 * "Deep" delete for a list of pointers to RTLs.
 *
 * \param pLrtl  The list.
 */
static void
delete_lrtls(std::list<RTL *> *pLrtl)
{
	for (const auto &rtl : *pLrtl) {
		delete rtl;
	}
}

/**
 * Sets a flag indicating that this BB has a label, in the sense that a label
 * is required in the translated source code.
 *
 * Add a label for the given basicblock.  The label number must be a non-zero
 * integer.
 *
 * \param pBB  Pointer to the BB whose label will be set.
 */
void
Cfg::setLabel(BasicBlock *pBB)
{
	if (pBB->m_iLabelNum == 0)
		pBB->m_iLabelNum = ++lastLabel;
}

/**
 * \brief Set an additional new out edge to a given value.
 *
 * Append a new out-edge from the given BB to the other given BB.  Needed for
 * example when converting a one-way BB to a two-way BB.
 *
 * \note Use BasicBlock::setOutEdge() for the common case where an existing
 * out edge is merely changed.
 *
 * \note Use Cfg::addOutEdge for ordinary BB creation; this is for unusual cfg
 * manipulation.
 *
 * \param pFromBB      Pointer to the BB getting the new out edge.
 * \param pNewOutEdge  Pointer to BB that will be the new successor.
 *
 * \par Side effects
 * Increments m_iNumOutEdges.
 */
void
Cfg::addNewOutEdge(BasicBlock *pFromBB, BasicBlock *pNewOutEdge)
{
	pFromBB->m_OutEdges.push_back(pNewOutEdge);
	++pFromBB->m_iNumOutEdges;
	// Since this is a new out-edge, set the "jump required" flag
	pFromBB->m_bJumpReqd = true;
	// Make sure that there is a label there
	setLabel(pNewOutEdge);
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

void
Cfg::printToLog() const
{
	std::ostringstream ost;
	print(ost);
	LOG << ost.str();
}

void
Cfg::setTimeStamps()
{
	// set DFS tag
	for (const auto &bb : m_listBB)
		bb->traversed = DFS_TAG;

	// set the parenthesis for the nodes as well as setting the post-order ordering between the nodes
	int time = 1;
	Ordering.clear();
	entryBB->setLoopStamps(time, Ordering);

	// set the reverse parenthesis for the nodes
	time = 1;
	entryBB->setRevLoopStamps(time);

	BasicBlock *retNode = findRetNode();
	assert(retNode);
	revOrdering.clear();
	retNode->setRevOrder(revOrdering);
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
			LOG << "failed to find commonPDom for " << oldCurImmPDom->getLowAddr()
			    << " and " << oldSuccImmPDom->getLowAddr() << "\n";
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
	if (!findRetNode())
		return;
	setTimeStamps();
	findImmedPDom();
	if (!Boomerang::get()->noDecompile) {
		structConds();
		structLoops();
		checkConds();
	}
	structured = true;
}

void
Cfg::addJunctionStatements()
{
	for (const auto &bb : m_listBB) {
		if (bb->getNumInEdges() > 1 && (!bb->getFirstStmt() || !bb->getFirstStmt()->isJunction())) {
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
		if (bb->getFirstStmt() && bb->getFirstStmt()->isJunction()) {
			assert(bb->getRTLs());
			bb->getRTLs()->front()->deleteStmt(0);
		}
	}
}

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
	ADDRESS aret = NO_ADDRESS;

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
		// or a unique string (e.g. bb0x8048c10) if no statements.
		if (auto first = bb->getFirstStmt()) {
#if BBINDEX
			os << ":";
#endif
			os << first->getNumber() << " ";
		} else {
			os << "bb" << (void *)bb << " ";
		}

		switch (bb->getType()) {
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
		case CALL:
			os << "call";
			if (auto dest = bb->getDestProc())
				os << "\\n" << dest->getName();
			break;
		case RET:
			os << "ret\",shape=triangle];\n";
			// Remember the (unique) return BB's address
			aret = bb->getLowAddr();
			continue;
		case ONEWAY:   os << "oneway";   break;
		case FALL:     os << "fall";     break;
		case COMPJUMP: os << "compjump"; break;
		case COMPCALL: os << "compcall"; break;
		case INVALID:  os << "invalid";  break;
		}
		os << "\"];\n";
	}

	// Force the one return node to be at the bottom (max rank).
	// Otherwise, with all its in-edges, it will end up in the middle.
	if (aret)
		os << std::hex
		   << "\t\t{rank=max; bb" << aret << "}\n"
		   << std::dec;

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

	std::list<BasicBlock *> workList;  // List of BBs still to be processed
	// Set of the same; used for quick membership test
	std::set<BasicBlock *> workSet;
	appendBBs(workList, workSet);

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

void
Cfg::appendBBs(std::list<BasicBlock *> &worklist, std::set<BasicBlock *> &workset)
{
	// Append my list of BBs to the worklist
	worklist.insert(worklist.end(), m_listBB.begin(), m_listBB.end());
	// Do the same for the workset
	for (const auto &bb : m_listBB)
		workset.insert(bb);
}

#if DEBUG_SPLIT_FOR_BRANCH
static void
dumpBB(BasicBlock *bb)
{
	std::cerr << "For BB at " << std::hex << bb << ":\nIn edges: ";
	const auto &ins = bb->getInEdges();
	const auto &outs = bb->getOutEdges();
	for (const auto &edge : ins)
		std::cerr << edge << " ";
	std::cerr << "\nOut Edges: ";
	for (const auto &edge : outs)
		std::cerr << edge << " ";
	std::cerr << "\n";
}
#endif

/**
 * Split the given BB at the RTL given, and turn it into the BranchStatement
 * given.  Sort out all the in and out edges.
 *
 *  pBB-> +----+    +----+ <-pBB
 * Change | A  | to | A  | where A and B could be empty. S is the string
 *        |    |    |    | instruction (with will branch to itself and to the
 *        +----+    +----+ start of the next instruction, i.e. the start of B,
 *        | S  |      |    if B is non empty).
 *        +----+      V
 *        | B  |    +----+ <-skipBB
 *        |    |    +-b1-+            b1 is just a branch for the skip part
 *        +----+      |
 *                    V
 *                  +----+ <-rptBB
 *                  | S' |            S' = S less the skip and repeat parts
 *                  +-b2-+            b2 is a branch for the repeat part
 *                    |
 *                    V
 *                  +----+ <-newBb
 *                  | B  |
 *                  |    |
 *                  +----+
 *
 * S is an RTL with 6 statements representing one string instruction (so this
 * function is highly specialised for the job of replacing the %SKIP and %RPT
 * parts of string instructions).
 */
BasicBlock *
Cfg::splitForBranch(BasicBlock *pBB, RTL *rtl, BranchStatement *br1, BranchStatement *br2, iterator &it)
{
#if 0
	std::cerr << "splitForBranch before:\n";
	std::cerr << pBB->prints() << "\n";
#endif

	// First find which RTL has the split address
	auto ri = std::find(pBB->m_pRtls->begin(), pBB->m_pRtls->end(), rtl);
	assert(ri != pBB->m_pRtls->end());

	bool haveA = (ri != pBB->m_pRtls->begin());

	ADDRESS addr = rtl->getAddress();

	// Make a BB for the br1 instruction
	auto pRtls = new std::list<RTL *>;
	auto ls = std::list<Statement *>();
	ls.push_back(br1);
	// Don't give this "instruction" the same address as the rest of the string instruction (causes problems when
	// creating the rptBB). Or if there is no A, temporarily use 0
	ADDRESS a = (haveA) ? addr : 0;
	auto skipRtl = new RTL(a, &ls);
	pRtls->push_back(skipRtl);
	auto skipBB = newBB(pRtls, TWOWAY, 2);
	rtl->updateAddress(addr + 1);
	if (!haveA) {
		skipRtl->updateAddress(addr);
		// Address addr now refers to the splitBB
		m_mapBB[addr] = skipBB;
		// Fix all predecessors of pBB to point to splitBB instead
		for (const auto &pred : pBB->m_InEdges) {
			for (auto &succ : pred->m_OutEdges) {
				if (succ == pBB) {
					succ = skipBB;
					skipBB->addInEdge(pred);
					break;
				}
			}
		}
	}

	// Remove the SKIP from the start of the string instruction RTL
	auto &stmts = rtl->getList();
	assert(stmts.size() >= 4);
	stmts.pop_front();
	// Replace the last statement with br2
	stmts.pop_back();
	stmts.push_back(br2);

	// Move the remainder of the string RTL into a new BB
	pRtls = new std::list<RTL *>;
	pRtls->push_back(*ri);
	auto rptBB = newBB(pRtls, TWOWAY, 2);
	ri = pBB->m_pRtls->erase(ri);

	// Move the remaining RTLs (if any) to a new list of RTLs
	BasicBlock *newBb;
	unsigned oldOutEdges = 0;
	bool haveB = (ri != pBB->m_pRtls->end());
	if (haveB) {
		pRtls = new std::list<RTL *>;
		pRtls->splice(pRtls->end(), *pBB->m_pRtls, ri, pBB->m_pRtls->end());
		oldOutEdges = pBB->getNumOutEdges();
		newBb = newBB(pRtls, pBB->getType(), oldOutEdges);
		// Transfer the out edges from A to B (pBB to newBb)
		for (unsigned i = 0; i < oldOutEdges; ++i)
			// Don't use addOutEdge, since it will also add in-edges back to pBB
			newBb->m_OutEdges.push_back(pBB->getOutEdge(i));
			//addOutEdge(newBb, pBB->getOutEdge(i));
	} else {
		// The "B" part of the above diagram is empty.
		// Don't create a new BB; just point newBB to the successor of pBB
		newBb = pBB->getOutEdge(0);
	}

	// Change pBB to a FALL bb
	pBB->updateType(FALL, 1);
	// Set the first out-edge to be skipBB
	pBB->m_OutEdges.clear();
	addOutEdge(pBB, skipBB);
	// Set the out edges for skipBB. First is the taken (true) leg.
	addOutEdge(skipBB, newBb);
	addOutEdge(skipBB, rptBB);
	// Set the out edges for the rptBB
	addOutEdge(rptBB, skipBB);
	addOutEdge(rptBB, newBb);

	// For each out edge of newBb, change any in-edges from pBB to instead come from newBb
	if (haveB) {
		for (unsigned i = 0; i < oldOutEdges; ++i) {
			BasicBlock *succ = newBb->m_OutEdges[i];
			for (auto &pred : succ->m_InEdges) {
				if (pred == pBB) {
					pred = newBb;
					break;
				}
			}
		}
	} else {
		// There is no "B" bb (newBb is just the successor of pBB) Fix that one out-edge to point to rptBB
		for (auto &pred : newBb->m_InEdges) {
			if (pred == pBB) {
				pred = rptBB;
				break;
			}
		}
	}
	if (!haveA) {
		// There is no A any more. All A's in-edges have been copied to the skipBB. It is possible that the original BB
		// had a self edge (branch to start of self). If so, this edge, now in to skipBB, must now come from newBb (if
		// there is a B) or rptBB if none.  Both of these will already exist, so delete it.
		for (const auto &pred : skipBB->m_InEdges) {
			if (pred == pBB) {
				skipBB->deleteInEdge(pBB);
				break;
			}
		}

#if DEBUG_SPLIT_FOR_BRANCH
		std::cerr << "About to delete pBB: " << std::hex << pBB << "\n";
		dumpBB(pBB);
		dumpBB(skipBB);
		dumpBB(rptBB);
		dumpBB(newBb);
#endif

		// Must delete pBB. Note that this effectively "increments" iterator it
		it = m_listBB.erase(it);
		pBB = nullptr;
	} else
		++it;

#if 0
	std::cerr << "splitForBranch after:\n";
	if (pBB) std::cerr << pBB->prints(); else std::cerr << "<null>\n";
	std::cerr << skipBB->prints();
	std::cerr << rptBB->prints();
	std::cerr << newBb->prints() << "\n";
#endif
	return newBb;
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
	auto it = implicitMap.find(param->getExp());
	if (it == implicitMap.end()) {
		Exp *eParam = Location::param(param->getName());
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
