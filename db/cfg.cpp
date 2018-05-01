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
static void erase_lrtls(std::list<RTL *> *pLrtl, std::list<RTL *>::iterator begin, std::list<RTL *>::iterator end);

/*==============================================================================
 * FUNCTION:        Cfg::Cfg
 * OVERVIEW:
 *============================================================================*/
Cfg::Cfg()
{
}

/*==============================================================================
 * FUNCTION:        Cfg::~Cfg
 * OVERVIEW:        Destructor. Note: destructs the component BBs as well
 *============================================================================*/
Cfg::~Cfg()
{
	// Delete the BBs
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		delete *it;
	}
}

/*==============================================================================
 * FUNCTION:        Cfg::setProc
 * OVERVIEW:        Set the pointer to the owning UserProc object
 * PARAMETERS:      proc - pointer to the owning UserProc object
 *============================================================================*/
void
Cfg::setProc(UserProc *proc)
{
	myProc = proc;
}

/*==============================================================================
 * FUNCTION:        Cfg::clear
 * OVERVIEW:        Clear the CFG of all basic blocks, ready for decode
 *============================================================================*/
void
Cfg::clear()
{
	// Don't delete the BBs; this will delete any CaseStatements we want to save for the re-decode. Just let the garbage
	// collection take care of it.
#if 0
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it)
		delete *it;
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

/*==============================================================================
 * FUNCTION:        Cfg::operator =
 * OVERVIEW:
 *============================================================================*/
const Cfg &
Cfg::operator =(const Cfg &other)
{
	m_listBB = other.m_listBB;
	m_mapBB = other.m_mapBB;
	m_bWellFormed = other.m_bWellFormed;
	return *this;
}

/*==============================================================================
 * FUNCTION:        setEntryBB
 * OVERVIEW:        Set the entry and exut BB pointers
 * NOTE:            Each cfg should have only one exit node now
 * PARAMETERS:      bb: pointer to the entry BB
 *============================================================================*/
void
Cfg::setEntryBB(BasicBlock *bb)
{
	entryBB = bb;
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		if ((*it)->getType() == RET) {
			exitBB = *it;
			return;
		}
	}
	// It is possible that there is no exit BB
}

void
Cfg::setExitBB(BasicBlock *bb)
{
	exitBB = bb;
}

/*==============================================================================
 * FUNCTION:        checkEntryBB
 * OVERVIEW:        Check the entry BB pointer; if zero, emit error message
 *                    and return true
 * RETURNS:         true if was null
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::newBB
 * OVERVIEW:        Add a new basic block to this cfg
 * PARAMETERS:      pRtls: list of pointers to RTLs to initialise the BB with bbType: the type of the BB (e.g. TWOWAY)
 *                  iNumOutEdges: number of out edges this BB will eventually have
 * RETURNS:         Pointer to the newly created BB, or 0 if there is already an incomplete BB with the same address
 *============================================================================*/
BasicBlock *
Cfg::newBB(std::list<RTL *> *pRtls, BBTYPE bbType, int iNumOutEdges) throw (BBAlreadyExistsError)
{
	MAPBB::iterator mi;
	BasicBlock *pBB;

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

// Use this function when there are outedges to BBs that are not created yet. Usually used via addOutEdge()
/*==============================================================================
 * FUNCTION:        Cfg::newIncompleteBB
 * OVERVIEW:
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::addOutEdge
 * OVERVIEW:        Add an out edge to this BB (and the in-edge to the dest BB)
 *                  May also set a label
 * NOTE:            Overloaded with address as 2nd argument (calls this proc in the end)
 * NOTE ALSO:       Does not increment m_iNumOutEdges; this is supposed to be constant for a BB.
 *                    (But see BasicBlock::addNewOutEdge())
 * PARAMETERS:      pBB: source BB (to have the out edge added to)
 *                  pDestBB: destination BB (to have the out edge point to)
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::addOutEdge
 * OVERVIEW:        Add an out edge to this BB (and the in-edge to the dest BB)
 *                  May also set a label
 * NOTE:            Calls the above
 * PARAMETERS:      pBB: source BB (to have the out edge added to)
 *                  addr: source address of destination (the out edge is to point to the BB whose lowest address is
 *                    addr)
 *                  bSetLabel: if true, set a label at the destination address.  Set true on "true" branches of labels
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::existsBB
 * OVERVIEW:        Return true if the given address is the start of a basic block, complete or not
 * PARAMETERS:      uNativeAddr: native address to look up
 * RETURNS:         True if uNativeAddr starts a BB
 *============================================================================*/
// Note: must ignore entries with a null pBB, since these are caused by
// calls to Label that failed, i.e. the instruction is not decoded yet.
bool
Cfg::existsBB(ADDRESS uNativeAddr) const
{
	auto mi = m_mapBB.find(uNativeAddr);
	return (mi != m_mapBB.end() && mi->second);
}

/*==============================================================================
 * FUNCTION:    Cfg::splitBB (private)
 * OVERVIEW:    Split the given basic block at the RTL associated with uNativeAddr. The first node's type becomes
 *              fall-through and ends at the RTL prior to that associated with uNativeAddr.  The second node's type
 *              becomes the type of the original basic block (pBB), and its out-edges are those of the original basic
 *              block. In edges of the new BB's descendants are changed.
 * PRECONDITION: assumes uNativeAddr is an address within the boundaries of the given basic block.
 * PARAMETERS:  pBB -  pointer to the BB to be split
 *              uNativeAddr - address of RTL to become the start of the new BB
 *              pNewBB -  if non zero, it remains as the "bottom" part of the BB, and splitBB only modifies the top part
 *              to not overlap.
 *              bDelRtls - if true, deletes the RTLs removed from the existing BB after the split point. Only used if
 *              there is an overlap with existing instructions
 * RETURNS:     Returns a pointer to the "bottom" (new) part of the split BB.
 *============================================================================*/
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
		// But we don't want the top BB's in edges; our only in-edge should be the out edge from the top BB
		pNewBB->m_InEdges.clear();
		// The "bottom" BB now starts at the implicit label, so we create a new list that starts at ri. We need a new
		// list, since it is different from the original BB's list. We don't have to "deep copy" the RTLs themselves,
		// since they will never overlap
		pNewBB->setRTLs(new std::list<RTL *>(ri, pBB->m_pRtls->end()));
		// Put it in the graph
		m_listBB.push_back(pNewBB);
		// Put the implicit label into the map. Need to do this before the addOutEdge() below
		m_mapBB[uNativeAddr] = pNewBB;
		// There must be a label here; else would not be splitting.  Give it a new label
		pNewBB->m_iLabelNum = ++lastLabel;
	} else if (pNewBB->m_bIncomplete) {
		// We have an existing BB and a map entry, but no details except for in-edges and m_bHasLabel.
		// First save the in-edges and m_iLabelNum
		std::vector<BasicBlock *> ins(pNewBB->m_InEdges);
		int label = pNewBB->m_iLabelNum;
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

	// Update original ("top") basic block's info and make it a fall-through
	pBB->m_nodeType = FALL;
	// Fix the in-edges of pBB's descendants. They are now pNewBB
	// Note: you can't believe m_iNumOutEdges at the time that this function may get called
	for (unsigned j = 0; j < pBB->m_OutEdges.size(); ++j) {
		BasicBlock *pDescendant = pBB->m_OutEdges[j];
		// Search through the in edges for pBB (old ancestor)
		unsigned k;
		for (k = 0; k < pDescendant->m_InEdges.size(); ++k) {
			if (pDescendant->m_InEdges[k] == pBB) {
				// Replace with a pointer to the new ancestor
				pDescendant->m_InEdges[k] = pNewBB;
				break;
			}
		}
		// That pointer should have been found!
		assert(k < pDescendant->m_InEdges.size());
	}
	// The old BB needs to have part of its list of RTLs erased, since the instructions overlap
	if (bDelRtls) {
		// Delete the list of pointers, and also the RTLs they point to
		erase_lrtls(pBB->m_pRtls, ri, pBB->m_pRtls->end());
	} else {
		// Delete the list of pointers, but not the RTLs they point to
		pBB->m_pRtls->erase(ri, pBB->m_pRtls->end());
	}
	// Erase any existing out edges
	pBB->m_OutEdges.clear();
	pBB->m_iNumOutEdges = 1;
	addOutEdge(pBB, uNativeAddr);
	return pNewBB;
}

/*==============================================================================
 * FUNCTION:        Cfg::getFirstBB
 * OVERVIEW:        Get the first BB of this cfg
 * PARAMETERS:      it: set to an value that must be passed to getNextBB
 * RETURNS:         Pointer to the first BB this cfg, or nullptr if none
 *============================================================================*/
BasicBlock *
Cfg::getFirstBB(BB_IT &it)
{
	if ((it = m_listBB.begin()) == m_listBB.end()) return nullptr;
	return *it;
}

/*==============================================================================
 * FUNCTION:        Cfg::getNextBB
 * OVERVIEW:        Get the next BB this cfg. Basically increments the given iterator and returns it
 * PARAMETERS:      iterator from a call to getFirstBB or getNextBB
 * RETURNS:         pointer to the BB, or nullptr if no more
 *============================================================================*/
BasicBlock *
Cfg::getNextBB(BB_IT &it)
{
	if (++it == m_listBB.end()) return nullptr;
	return *it;
}

/*==============================================================================
 * FUNCTION:    Cfg::label
 * OVERVIEW:    Checks whether the given native address is a label (explicit or non explicit) or not. Returns false for
 *              incomplete BBs.  So it returns true iff the address has already been decoded in some BB. If it was not
 *              already a label (i.e. the first instruction of some BB), the BB is split so that it becomes a label.
 *              Explicit labels are addresses that have already been tagged as being labels due to transfers of control
 *              to that address, and are therefore the start of some BB.     Non explicit labels are those that belong
 *              to basic blocks that have already been constructed (i.e. have previously been parsed) and now need to
 *              be made explicit labels. In the case of non explicit labels, the basic block is split into two and types
 *              and edges are adjusted accordingly. If pCurBB is the BB that gets split, it is changed to point to the
 *              address of the new (lower) part of the split BB.
 *              If there is an incomplete entry in the table for this address which overlaps with a completed address,
 *              the completed BB is split and the BB for this address is completed.
 * PARAMETERS:  uNativeAddress - native (source) address to check
 *              pCurBB - See above
 * RETURNS:     True if uNativeAddr is a label, i.e. (now) the start of a BB
 *              Note: pCurBB may be modified (as above)
 *============================================================================*/
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

// Return true if there is an incomplete BB already at this address
/*==============================================================================
 * FUNCTION:        Cfg::isIncomplete
 * OVERVIEW:        Return true if given address is the start of an incomplete basic block
 * PARAMETERS:      uAddr: Address to look up
 * RETURNS:         True if uAddr starts an incomplete BB
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::sortByAddress
 * OVERVIEW:        Sorts the BBs in a cfg by first address. Just makes it more convenient to read when BBs are
 *                  iterated.
 *============================================================================*/
void
Cfg::sortByAddress()
{
	m_listBB.sort(BasicBlock::lessAddress);
}

/*==============================================================================
 * FUNCTION:        Cfg::sortByFirstDFT
 * OVERVIEW:        Sorts the BBs in a cfg by their first DFT numbers.
 *============================================================================*/
void
Cfg::sortByFirstDFT()
{
	m_listBB.sort(BasicBlock::lessFirstDFT);
}

/*==============================================================================
 * FUNCTION:        Cfg::sortByLastDFT
 * OVERVIEW:        Sorts the BBs in a cfg by their last DFT numbers.
 *============================================================================*/
void
Cfg::sortByLastDFT()
{
	m_listBB.sort(BasicBlock::lessLastDFT);
}

/*==============================================================================
 * FUNCTION:        Cfg::wellFormCfg
 * OVERVIEW:        Checks that all BBs are complete, and all out edges are valid. However, ADDRESSes that are
 *                  interprocedural out edges are not checked or changed.
 * RETURNS:         transformation was successful
 *============================================================================*/
bool
Cfg::wellFormCfg()
{
	m_bWellFormed = true;
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		// it iterates through all BBs in the list
		// Check that it's complete
		if ((*it)->m_bIncomplete) {
			m_bWellFormed = false;
			auto itm = m_mapBB.begin();
			for (; itm != m_mapBB.end(); ++itm)
				if (itm->second == *it) break;
			if (itm == m_mapBB.end())
				std::cerr << "WellFormCfg: incomplete BB not even in map!\n";
			else {
				std::cerr << "WellFormCfg: BB with native address " << std::hex << itm->first
				          << " is incomplete\n";
			}
		} else {
			// Complete. Test the out edges
			assert((int)(*it)->m_OutEdges.size() == (*it)->m_iNumOutEdges);
			for (int i = 0; i < (*it)->m_iNumOutEdges; ++i) {
				// check if address is interprocedural
				//if ((*it)->m_OutEdgeInterProc[i] == false)
				{
					// i iterates through the outedges in the BB *it
					BasicBlock *pBB = (*it)->m_OutEdges[i];

					// Check that the out edge has been written (i.e. nonzero)
					if (!pBB) {
						m_bWellFormed = false;  // At least one problem
						ADDRESS addr = (*it)->getLowAddr();
						std::cerr << "WellFormCfg: BB with native address " << std::hex << addr
						          << " is missing outedge " << i << std::endl;
					} else {
						// Check that there is a corresponding in edge from the
						// child to here
						auto ii = pBB->m_InEdges.begin();
						for (; ii != pBB->m_InEdges.end(); ++ii)
							if (*ii == *it) break;
						if (ii == pBB->m_InEdges.end()) {
							std::cerr << "WellFormCfg: No in edge to BB at " << std::hex << (*it)->getLowAddr()
							          << " from successor BB at " << pBB->getLowAddr() << std::endl;
							m_bWellFormed = false;  // At least one problem
						}
					}
				}
			}
			// Also check that each in edge has a corresponding out edge to here (could have an extra in-edge, for
			// example)
			for (auto ii = (*it)->m_InEdges.begin(); ii != (*it)->m_InEdges.end(); ++ii) {
				auto oo = (*ii)->m_OutEdges.begin();
				for (; oo != (*ii)->m_OutEdges.end(); ++oo)
					if (*oo == *it) break;
				if (oo == (*ii)->m_OutEdges.end()) {
					std::cerr << "WellFormCfg: No out edge to BB at " << std::hex << (*it)->getLowAddr()
					          << " from predecessor BB at " << (*ii)->getLowAddr() << std::endl;
					m_bWellFormed = false;  // At least one problem
				}
			}
		}
	}
	return m_bWellFormed;
}

/*==============================================================================
 * FUNCTION:        Cfg::mergeBBs
 * OVERVIEW:
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::completeMerge
 * OVERVIEW:        Complete the merge of two BBs by adjusting in and out edges.  If bDelete is true, delete pb1
 * PARAMETERS:      pb1, pb2: pointers to the two BBs to merge
 *                  bDelete: if true, pb1 is deleted as well
 *============================================================================*/
void
Cfg::completeMerge(BasicBlock *pb1, BasicBlock *pb2, bool bDelete = false)
{
	// First we replace all of pb1's predecessors' out edges that used to point to pb1 (usually only one of these) with
	// pb2
	for (int i = 0; i < pb1->m_InEdges.size(); ++i) {
		BasicBlock *pPred = pb1->m_InEdges[i];
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

/*==============================================================================
 * FUNCTION:        Cfg::joinBB
 * OVERVIEW:        Amalgamate the RTLs for pb1 and pb2, and place the result into pb2
 * PARAMETERS:      pb1, pb2: pointers to the BBs to join
 * ASSUMES:         Fallthrough of *pb1 is *pb2
 * RETURNS:         True if successful
 *============================================================================*/
bool
Cfg::joinBB(BasicBlock *pb1, BasicBlock *pb2)
{
	// Ensure that the fallthrough case for pb1 is pb2
	std::vector<BasicBlock *> &v = pb1->getOutEdges();
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

void
Cfg::removeBB(BasicBlock *bb)
{
	auto bbit = std::find(m_listBB.begin(), m_listBB.end(), bb);
	m_listBB.erase(bbit);
}

/*==============================================================================
 * FUNCTION:        Cfg::compressCfg
 * OVERVIEW:        Compress the CFG. For now, it only removes BBs that are
 *                    just branches
 * RETURNS:         False if not well formed; true otherwise
 *============================================================================*/
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
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		for (auto it1 = (*it)->m_OutEdges.begin(); it1 != (*it)->m_OutEdges.end(); ++it1) {
			BasicBlock *pSucc = (*it1);  // Pointer to J
			BasicBlock *bb = (*it);      // Pointer to A
			if (pSucc->m_InEdges.size() == 1
			 && pSucc->m_OutEdges.size() == 1
			 && pSucc->m_pRtls->size() == 1
			 && pSucc->m_pRtls->front()->getNumStmt() == 1
			 && pSucc->m_pRtls->front()->elementAt(0)->isGoto()) {
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
				for (auto it2 = (*it1)->m_InEdges.begin(); it2 != (*it1)->m_InEdges.end(); ++it2) {
					if (*it2 == pSucc)
						*it2 = bb;  // Point to A
				}
				// Remove the in-edge from J to A. First find the in-edge
				auto it2 = pSucc->m_InEdges.begin();
				for (; it2 != pSucc->m_InEdges.end(); ++it2) {
					if (*it2 == bb)
						break;
				}
				assert(it2 != pSucc->m_InEdges.end());
				pSucc->deleteInEdge(it2);
				// If nothing else uses this BB (J), remove it from the CFG
				if (pSucc->m_InEdges.size() == 0) {
					for (auto it3 = m_listBB.begin(); it3 != m_listBB.end(); ++it3) {
						if (*it3 == pSucc) {
							m_listBB.erase(it3);
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

/*==============================================================================
 * FUNCTION:        Cfg::unTraverse
 * OVERVIEW:        Reset all the traversed flags.
 *============================================================================*/
void
Cfg::unTraverse()
{
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		(*it)->m_iTraversed = false;
		(*it)->traversed = UNTRAVERSED;
	}
}

/*==============================================================================
 * FUNCTION:        Cfg::establishDFTOrder
 * OVERVIEW:        Given a well-formed cfg graph, a partial ordering is established between the nodes. The ordering is
 *                  based on the final visit to each node during a depth first traversal such that if node n1 was
 *                  visited for the last time before node n2 was visited for the last time, n1 will be less than n2.
 *                  The return value indicates if all nodes where ordered. This will not be the case for incomplete CFGs
 *                  (e.g. switch table not completely recognised) or where there are nodes unreachable from the entry
 *                  node.
 * RETURNS:         all nodes where ordered
 *============================================================================*/
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
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		if ((*it)->getType() == RET) {
			retNode = *it;
			break;
		} else if ((*it)->getType() == CALL) {
			Proc *p = (*it)->getCallDestProc();
			if (p && !strcmp(p->getName(), "exit"))
				retNode = *it;
		}
	}
	return retNode;
}

/*==============================================================================
 * FUNCTION:        Cfg::establishRevDFTOrder
 * OVERVIEW:        Performs establishDFTOrder on the reverse (flip) of the graph, assumes: establishDFTOrder has
 *                  already been called
 * RETURNS:         all nodes where ordered
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::isWellFormed
 * OVERVIEW:
 *============================================================================*/
bool
Cfg::isWellFormed() const
{
	return m_bWellFormed;
}

/*==============================================================================
 * FUNCTION:        Cfg::isOrphan
 * OVERVIEW:
 *============================================================================*/
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

/*==============================================================================
 * FUNCTION:        Cfg::pbbToIndex
 * OVERVIEW:        Return an index for the given BasicBlock*
 * NOTE:            Linear search: O(N) complexity
 * RETURNS:         Index, or -1 for unknown BasicBlock*
 *============================================================================*/
int
Cfg::pbbToIndex(BasicBlock *pBB) const
{
	int i = 0;
	auto it = m_listBB.begin();
	while (it != m_listBB.end()) {
		if (*it++ == pBB) return i;
		++i;
	}
	return -1;
}

/*==============================================================================
 * FUNCTION:        Cfg::addCall
 * OVERVIEW:        Add a call to the set of calls within this procedure.
 * PARAMETERS:      call - a call instruction
 *============================================================================*/
void
Cfg::addCall(CallStatement *call)
{
	callSites.insert(call);
}

/*==============================================================================
 * FUNCTION:        Cfg::getCalls
 * OVERVIEW:        Get the set of calls within this procedure.
 * RETURNS:         the set of calls within this procedure
 *============================================================================*/
std::set<CallStatement *> &
Cfg::getCalls()
{
	return callSites;
}

/*==============================================================================
 * FUNCTION:        Cfg::searchAndReplace
 * OVERVIEW:        Replace all instances of search with replace.
 * PARAMETERS:      search - a location to search for
 *                  replace - the expression with which to replace it
 *============================================================================*/
void
Cfg::searchAndReplace(Exp *search, Exp *replace)
{
	for (auto bb_it = m_listBB.begin(); bb_it != m_listBB.end(); ++bb_it) {
		std::list<RTL *> &rtls = *((*bb_it)->getRTLs());
		for (auto rtl_it = rtls.begin(); rtl_it != rtls.end(); ++rtl_it) {
			RTL &rtl = **rtl_it;
			rtl.searchAndReplace(search, replace);
		}
	}
}

bool
Cfg::searchAll(Exp *search, std::list<Exp *> &result)
{
	bool ch = false;
	for (auto bb_it = m_listBB.begin(); bb_it != m_listBB.end(); ++bb_it) {
		std::list<RTL *> &rtls = *((*bb_it)->getRTLs());
		for (auto rtl_it = rtls.begin(); rtl_it != rtls.end(); ++rtl_it) {
			RTL &rtl = **rtl_it;
			ch |= rtl.searchAll(search, result);
		}
	}
	return ch;
}

/*==============================================================================
 * FUNCTION:    delete_lrtls
 * OVERVIEW:    "deep" delete for a list of pointers to RTLs
 * PARAMETERS:  pLrtl - the list
 *============================================================================*/
static void
delete_lrtls(std::list<RTL *> *pLrtl)
{
	for (auto it = pLrtl->begin(); it != pLrtl->end(); ++it) {
		delete *it;
	}
}

/*==============================================================================
 * FUNCTION:    erase_lrtls
 * OVERVIEW:    "deep" erase for a list of pointers to RTLs
 * PARAMETERS:  pLrtls - the list
 *              begin - iterator to first (inclusive) item to delete
 *              end - iterator to last (exclusive) item to delete
 *============================================================================*/
static void
erase_lrtls(std::list<RTL *> *pLrtl, std::list<RTL *>::iterator begin, std::list<RTL *>::iterator end)
{
	for (auto it = begin; it != end; ++it) {
		delete *it;
	}
	pLrtl->erase(begin, end);
}

/*==============================================================================
 * FUNCTION:        Cfg::setLabel
 * OVERVIEW:        Sets a flag indicating that this BB has a label, in the sense that a label is required in the
 *                  translated source code
 * PARAMETERS:      pBB: Pointer to the BB whose label will be set
 *============================================================================*/
void
Cfg::setLabel(BasicBlock *pBB)
{
	if (pBB->m_iLabelNum == 0)
		pBB->m_iLabelNum = ++lastLabel;
}

/*==============================================================================
 * FUNCTION:        Cfg::addNewOutEdge
 * OVERVIEW:        Append a new out-edge from the given BB to the other given BB
 *                  Needed for example when converting a one-way BB to a two-way BB
 * NOTE:            Use BasicBlock::setOutEdge() for the common case where an existing out edge is merely changed
 * NOTE ALSO:       Use Cfg::addOutEdge for ordinary BB creation; this is for unusual cfg manipulation
 * PARAMETERS:      pFromBB: pointer to the BB getting the new out edge
 *                  pNewOutEdge: pointer to BB that will be the new successor
 * SIDE EFFECTS:    Increments m_iNumOutEdges
 *============================================================================*/
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

void
Cfg::simplify()
{
	if (VERBOSE)
		LOG << "simplifying...\n";
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it)
		(*it)->simplify();
}

// print this cfg, mainly for debugging
void
Cfg::print(std::ostream &out, bool html) const
{
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it)
		(*it)->print(out, html);
	out << std::endl;
}

void
Cfg::printToLog() const
{
	std::ostringstream ost;
	print(ost);
	LOG << ost.str().c_str();
}

void
Cfg::setTimeStamps()
{
	// set DFS tag
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it)
		(*it)->traversed = DFS_TAG;

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

// Finds the common post dominator of the current immediate post dominator and its successor's immediate post dominator
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

/* Finds the immediate post dominator of each node in the graph PROC->cfg.  Adapted version of the dominators algorithm
 * by Hecht and Ullman; finds immediate post dominators only.  Note: graph should be reducible
 */
void
Cfg::findImmedPDom()
{
	BasicBlock *curNode, *succNode;  // the current Node and its successor

	// traverse the nodes in order (i.e from the bottom up)
	int i;
	for (i = revOrdering.size() - 1; i >= 0; --i) {
		curNode = revOrdering[i];
		std::vector<BasicBlock *> &oEdges = curNode->getOutEdges();
		for (unsigned int j = 0; j < oEdges.size(); ++j) {
			succNode = oEdges[j];
			if (succNode->revOrd > curNode->revOrd)
				curNode->immPDom = commonPDom(curNode->immPDom, succNode);
		}
	}

	// make a second pass but consider the original CFG ordering this time
	unsigned u;
	for (u = 0; u < Ordering.size(); ++u) {
		curNode = Ordering[u];
		std::vector<BasicBlock *> &oEdges = curNode->getOutEdges();
		if (oEdges.size() > 1)
			for (unsigned int j = 0; j < oEdges.size(); ++j) {
				succNode = oEdges[j];
				curNode->immPDom = commonPDom(curNode->immPDom, succNode);
			}
	}

	// one final pass to fix up nodes involved in a loop
	for (u = 0; u < Ordering.size(); ++u) {
		curNode = Ordering[u];
		std::vector<BasicBlock *> &oEdges = curNode->getOutEdges();
		if (oEdges.size() > 1)
			for (unsigned int j = 0; j < oEdges.size(); ++j) {
				succNode = oEdges[j];
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

// Structures all conditional headers (i.e. nodes with more than one outedge)
void
Cfg::structConds()
{
	// Process the nodes in order
	for (unsigned int i = 0; i < Ordering.size(); ++i) {
		BasicBlock *curNode = Ordering[i];

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

// Pre: The loop induced by (head,latch) has already had all its member nodes tagged
// Post: The type of loop has been deduced
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

// Pre: The loop headed by header has been induced and all it's member nodes have been tagged
// Post: The follow of the loop has been determined.
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

// Pre: header has been detected as a loop header and has the details of the
//      latching node
// Post: the nodes within the loop have been tagged
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
	for (int i = header->ord - 1; i >= latch->ord; --i)
		if (Ordering[i]->inLoop(header, latch)) {
			// update the membership map to reflect that this node is within the loop
			loopNodes[i] = true;

			Ordering[i]->setLoopHead(header);
		}
}

// Pre: The graph for curProc has been built.
// Post: Each node is tagged with the header of the most nested loop of which it is a member (possibly none).
// The header of each loop stores information on the latching node as well as the type of loop it heads.
void
Cfg::structLoops()
{
	for (int i = Ordering.size() - 1; i >= 0; --i) {
		BasicBlock *curNode = Ordering[i];  // the current node under investigation
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

		std::vector<BasicBlock *> &iEdges = curNode->getInEdges();
		for (unsigned int j = 0; j < iEdges.size(); ++j) {
			BasicBlock *pred = iEdges[j];
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

// This routine is called after all the other structuring has been done. It detects conditionals that are in fact the
// head of a jump into/outof a loop or into a case body. Only forward jumps are considered as unstructured backward
//jumps will always be generated nicely.
void
Cfg::checkConds()
{
	for (unsigned int i = 0; i < Ordering.size(); ++i) {
		BasicBlock *curNode = Ordering[i];
		std::vector<BasicBlock *> &oEdges = curNode->getOutEdges();

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
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		BasicBlock *pbb = *it;
		if (pbb->getNumInEdges() > 1 && (!pbb->getFirstStmt() || !pbb->getFirstStmt()->isJunction())) {
			assert(pbb->getRTLs());
			auto j = new JunctionStatement();
			j->setBB(pbb);
			pbb->getRTLs()->front()->prependStmt(j);
		}
	}
}

void
Cfg::removeJunctionStatements()
{
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		BasicBlock *pbb = *it;
		if (pbb->getFirstStmt() && pbb->getFirstStmt()->isJunction()) {
			assert(pbb->getRTLs());
			pbb->getRTLs()->front()->deleteStmt(0);
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
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		os << "\t\tbb" << std::hex << (*it)->getLowAddr()
		   << " [label=\"";
		const char *p = (*it)->getStmtNumber();
#if BBINDEX
		os << std::dec << indices[*it];
		if (p[0] != 'b')
			// If starts with 'b', no statements (something like bb8101c3c).
			os << ":";
#endif
		os << p << " ";
		switch ((*it)->getType()) {
		case TWOWAY:
			os << "twoway";
			if ((*it)->getCond()) {
				os << "\\n";
				(*it)->getCond()->print(os);
				os << "\",shape=diamond];\n";
				continue;
			}
			break;
		case NWAY:
			os << "nway";
			{
				Exp *de = (*it)->getDest();
				if (de) os << "\\n" << de;
				os << "\",shape=trapezium];\n";
			}
			continue;
		case CALL:
			os << "call";
			{
				Proc *dest = (*it)->getDestProc();
				if (dest) os << "\\n" << dest->getName();
			}
			break;
		case RET:
			os << "ret\",shape=triangle];\n";
			// Remember the (unbique) return BB's address
			aret = (*it)->getLowAddr();
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
		os << "\t\t{rank=max; bb" << std::hex << aret << "}\n";

	// Close the subgraph
	os << "\t}\n";

	// Now the edges
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		std::vector<BasicBlock *> &outEdges = (*it)->getOutEdges();
		for (unsigned int j = 0; j < outEdges.size(); ++j) {
			os << "\tbb" << std::hex << (*it)->getLowAddr()
			   << " -> bb" << std::hex << outEdges[j]->getLowAddr()
			   << " [color=blue";
			if ((*it)->getType() == TWOWAY) {
				if (j == 0)
					os << ",label=\"true\"";
				else
					os << ",label=\"false\"";
			}
			os << "];\n";
		}
	}
#if BACK_EDGES
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		std::vector<BasicBlock *> &inEdges = (*it)->getInEdges();
		for (unsigned int j = 0; j < inEdges.size(); ++j) {
			os << "\tbb" << std::hex << (*it)->getLowAddr()
			   << " -> bb" << std::hex << inEdges[j]->getLowAddr()
			   << " [color=green];\n";
		}
	}
#endif
}



////////////////////////////////////
//           Liveness             //
////////////////////////////////////

void
updateWorkListRev(BasicBlock *currBB, std::list<BasicBlock *> &workList, std::set<BasicBlock *> &workSet)
{
	// Insert inedges of currBB into the worklist, unless already there
	std::vector<BasicBlock *> &ins = currBB->getInEdges();
	int n = ins.size();
	for (int i = 0; i < n; ++i) {
		BasicBlock *currIn = ins[i];
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
					RTL *lastRtl = currBB->m_pRtls->back();
					std::list<Statement *> &lst = lastRtl->getList();
					if (!lst.empty()) last = lst.back();
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
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it)
		workset.insert(*it);
}

void
dumpBB(BasicBlock *bb)
{
	std::cerr << "For BB at " << std::hex << bb << ":\nIn edges: ";
	int i, n;
	std::vector<BasicBlock *> ins = bb->getInEdges();
	std::vector<BasicBlock *> outs = bb->getOutEdges();
	n = ins.size();
	for (i = 0; i < n; ++i)
		std::cerr << ins[i] << " ";
	std::cerr << "\nOut Edges: ";
	n = outs.size();
	for (i = 0; i < n; ++i)
		std::cerr << outs[i] << " ";
	std::cerr << "\n";
}

/*  pBB-> +----+    +----+ <-pBB
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
 * S is an RTL with 6 statements representing one string instruction (so this function is highly specialised for the job
 * of replacing the %SKIP and %RPT parts of string instructions)
 */

BasicBlock *
Cfg::splitForBranch(BasicBlock *pBB, RTL *rtl, BranchStatement *br1, BranchStatement *br2, BB_IT &it)
{
#if 0
	std::cerr << "splitForBranch before:\n";
	std::cerr << pBB->prints() << "\n";
#endif

	unsigned i, j;
	// First find which RTL has the split address
	auto ri = pBB->m_pRtls->begin();
	for (; ri != pBB->m_pRtls->end(); ++ri) {
		if ((*ri) == rtl)
			break;
	}
	assert(ri != pBB->m_pRtls->end());

	bool haveA = (ri != pBB->m_pRtls->begin());

	ADDRESS addr = rtl->getAddress();

	// Make a BB for the br1 instruction
	auto pRtls = new std::list<RTL *>;
	auto ls = new std::list<Statement *>;
	ls->push_back(br1);
	// Don't give this "instruction" the same address as the rest of the string instruction (causes problems when
	// creating the rptBB). Or if there is no A, temporarily use 0
	ADDRESS a = (haveA) ? addr : 0;
	auto skipRtl = new RTL(a, ls);
	pRtls->push_back(skipRtl);
	auto skipBB = newBB(pRtls, TWOWAY, 2);
	rtl->updateAddress(addr + 1);
	if (!haveA) {
		skipRtl->updateAddress(addr);
		// Address addr now refers to the splitBB
		m_mapBB[addr] = skipBB;
		// Fix all predecessors of pBB to point to splitBB instead
		for (unsigned i = 0; i < pBB->m_InEdges.size(); ++i) {
			BasicBlock *pred = pBB->m_InEdges[i];
			for (unsigned j = 0; j < pred->m_OutEdges.size(); ++j) {
				BasicBlock *succ = pred->m_OutEdges[j];
				if (succ == pBB) {
					pred->m_OutEdges[j] = skipBB;
					skipBB->addInEdge(pred);
					break;
				}
			}
		}
	}

	// Remove the SKIP from the start of the string instruction RTL
	std::list<Statement *> &li = rtl->getList();
	assert(li.size() >= 4);
	li.pop_front();
	// Replace the last statement with br2
	li.pop_back();
	li.push_back(br2);

	// Move the remainder of the string RTL into a new BB
	pRtls = new std::list<RTL *>;
	pRtls->push_back(*ri);
	auto rptBB = newBB(pRtls, TWOWAY, 2);
	ri = pBB->m_pRtls->erase(ri);

	// Move the remaining RTLs (if any) to a new list of RTLs
	BasicBlock *newBb;
	unsigned oldOutEdges = 0;
	bool haveB = true;
	if (ri != pBB->m_pRtls->end()) {
		pRtls = new std::list<RTL *>;
		while (ri != pBB->m_pRtls->end()) {
			pRtls->push_back(*ri);
			ri = pBB->m_pRtls->erase(ri);
		}
		oldOutEdges = pBB->getNumOutEdges();
		newBb = newBB(pRtls, pBB->getType(), oldOutEdges);
		// Transfer the out edges from A to B (pBB to newBb)
		for (i = 0; i < oldOutEdges; ++i)
			// Don't use addOutEdge, since it will also add in-edges back to pBB
			newBb->m_OutEdges.push_back(pBB->getOutEdge(i));
			//addOutEdge(newBb, pBB->getOutEdge(i));
	} else {
		// The "B" part of the above diagram is empty.
		// Don't create a new BB; just point newBB to the successor of pBB
		haveB = false;
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
		for (i = 0; i < oldOutEdges; ++i) {
			BasicBlock *succ = newBb->m_OutEdges[i];
			for (j = 0; j < succ->m_InEdges.size(); ++j) {
				BasicBlock *pred = succ->m_InEdges[j];
				if (pred == pBB) {
					succ->m_InEdges[j] = newBb;
					break;
				}
			}
		}
	} else {
		// There is no "B" bb (newBb is just the successor of pBB) Fix that one out-edge to point to rptBB
		for (j = 0; j < newBb->m_InEdges.size(); ++j) {
			BasicBlock *pred = newBb->m_InEdges[j];
			if (pred == pBB) {
				newBb->m_InEdges[j] = rptBB;
				break;
			}
		}
	}
	if (!haveA) {
		// There is no A any more. All A's in-edges have been copied to the skipBB. It is possible that the original BB
		// had a self edge (branch to start of self). If so, this edge, now in to skipBB, must now come from newBb (if
		// there is a B) or rptBB if none.  Both of these will already exist, so delete it.
		for (j = 0; j < skipBB->m_InEdges.size(); ++j) {
			BasicBlock *pred = skipBB->m_InEdges[j];
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

// Check for indirect jumps and calls in all my BBs; decode any new code
bool
Cfg::decodeIndirectJmp(UserProc *proc)
{
	bool res = false;
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		res |= (*it)->decodeIndirectJmp(proc);
	}
	return res;
}

void
Cfg::undoComputedBB(Statement *stmt)
{
	for (auto it = m_listBB.begin(); it != m_listBB.end(); ++it) {
		if ((*it)->undoComputedBB(stmt))
			break;
	}
}

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

Statement *
Cfg::findTheImplicitAssign(Exp *x)
{
	// As per the above, but don't create an implicit if it doesn't already exist
	auto it = implicitMap.find(x);
	if (it == implicitMap.end())
		return nullptr;
	return it->second;
}

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

void
Cfg::removeImplicitAssign(Exp *x)
{
	auto it = implicitMap.find(x);
	assert(it != implicitMap.end());
	Statement *ia = it->second;
	implicitMap.erase(it);        // Delete the mapping
	myProc->removeStatement(ia);  // Remove the actual implicit assignment statement as well
}
