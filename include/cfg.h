/**
 * \file
 * \brief Interface for a control flow graph, based on basic block nodes.
 *
 * \authors
 * Copyright (C) 1997-2005, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef CFG_H
#define CFG_H

#include "basicblock.h"  // For the BB nodes
#include "dataflow.h"    // For embedded class DataFlow
#include "exphelp.h"     // For lessExpStar
#include "types.h"

#include <ostream>
#include <exception>
#include <list>
#include <map>
#include <set>
#include <vector>

class BranchStatement;
class CallStatement;
class ConnectionGraph;
class Exp;
class Global;
class HLLCode;
class Parameter;
class Prog;
class RTL;
class Statement;
class UserProc;

#define BTHEN 0
#define BELSE 1



// A type for the ADDRESS to BB map
typedef std::map<ADDRESS, BasicBlock *> MAPBB;

/*==============================================================================
 * Control Flow Graph class. Contains all the BasicBlock objects for a procedure.  These BBs contain all the RTLs for
 * the procedure, so by traversing the Cfg, one traverses the whole procedure.
 *============================================================================*/
class Cfg {
	/*
	 * Pointer to the UserProc object that contains this CFG object
	 */
	UserProc   *myProc;

	/*
	 * The list of pointers to BBs.
	 */
	std::list<BasicBlock *> m_listBB;

	/*
	 * Ordering of BBs for control flow structuring
	 */
	std::vector<BasicBlock *> Ordering;
	std::vector<BasicBlock *> revOrdering;

	/*
	 * The ADDRESS to BasicBlock* map.
	 */
	MAPBB       m_mapBB;

	/*
	 * The entry and exit BBs.
	 */
	BasicBlock *entryBB = nullptr;
	BasicBlock *exitBB = nullptr;

	/*
	 * True if well formed.
	 */
	bool        m_bWellFormed = false, structured = false;

	/*
	 * Set of the call instructions in this procedure.
	 */
	std::set<CallStatement *> callSites;

	/*
	 * Last label (positive integer) used by any BB this Cfg
	 */
	int         lastLabel = 0;

	/*
	 * Map from expression to implicit assignment. The purpose is to prevent multiple implicit assignments for
	 * the same location.
	 */
	std::map<Exp *, Statement *, lessExpStar> implicitMap;

	bool        bImplicitsDone = false;  // True when the implicits are done; they can cause problems (e.g. with
	                                     // ad-hoc global assignment)

public:
	/*
	 * Constructor.
	 */
	            Cfg();

	/*
	 * Destructor.
	 */
	           ~Cfg();

	/*
	 * Set the pointer to the owning UserProc object
	 */
	void        setProc(UserProc *proc);

	/*
	 * clear this CFG of all basic blocks, ready for decode
	 */
	void        clear();

	/*
	 * Get the number of BBs
	 */
	unsigned    getNumBBs() const { return m_listBB.size(); }

	/*
	 * Equality operator.
	 */
	const Cfg  &operator =(const Cfg &other); /* Copy constructor */

	class BBAlreadyExistsError : public std::exception {
	public:
		BasicBlock *pBB;
		BBAlreadyExistsError(BasicBlock *pBB) : pBB(pBB) { }
	};

	/*
	 * Checks to see if the address associated with pRtls is already in the map as an incomplete BB; if so, it is
	 * completed now and a pointer to that BB is returned. Otherwise, allocates memory for a new basic block node,
	 * initializes its list of RTLs with pRtls, its type to the given type, and allocates enough space to hold
	 * pointers to the out-edges (based on given numOutEdges).
	 * The native address associated with the start of the BB is taken from pRtls, and added to the map (unless 0).
	 * NOTE: You cannot assume that the returned BB will have the RTL associated with pStart as its first RTL, since
	 * the BB could be split. You can however assume that the returned BB is suitable for adding out edges (i.e. if
	 * the BB is split, you get the "bottom" part of the BB, not the "top" (with lower addresses at the "top").
	 * Returns nullptr if not successful, or if there already exists a completed BB at this address (this can happen
	 * with certain kinds of forward branches).
	 */
	BasicBlock *newBB(std::list<RTL *> *pRtls, BBTYPE bbType, int iNumOutEdges) throw (BBAlreadyExistsError);

	/*
	 * Allocates space for a new, incomplete BB, and the given address is added to the map. This BB will have to be
	 * completed before calling WellFormCfg. This function will commonly be called via AddOutEdge()
	 */
	BasicBlock *newIncompleteBB(ADDRESS addr);

	/*
	 * Adds an out-edge to the basic block pBB by filling in the first slot that is empty.  Note: an address is
	 * given here; the out edge will be filled in as a pointer to a BB. An incomplete BB will be created if
	 * required. If bSetLabel is true, the destination BB will have its "label required" bit set.
	 */
	void        addOutEdge(BasicBlock *pBB, ADDRESS adr, bool bSetLabel = false);

	/*
	 * Adds an out-edge to the basic block pBB by filling in the first slot that is empty.  Note: a pointer to a BB
	 * is given here.
	 */
	void        addOutEdge(BasicBlock *pBB, BasicBlock *pDestBB, bool bSetLabel = false);

	/*
	 * Add a label for the given basicblock. The label number must be a non-zero integer
	 */
	void        setLabel(BasicBlock *pBB);

	/*
	 * Gets a pointer to the first BB this cfg. Also initialises `it' so that calling GetNextBB will return the
	 * second BB, etc.  Also, *it is the first BB.  Returns 0 if there are no BBs this CFG.
	 */
	BasicBlock *getFirstBB(BB_IT &it);

	/*
	 * Gets a pointer to the next BB this cfg. `it' must be from a call to GetFirstBB(), or from a subsequent call
	 * to GetNextBB().  Also, *it is the current BB.  Returns 0 if there are no more BBs this CFG.
	 */
	BasicBlock *getNextBB(BB_IT &it);

	/*
	 * An alternative to the above is to use begin() and end():
	 */
	typedef BB_IT iterator;
	iterator    begin() { return m_listBB.begin(); }
	iterator    end()   { return m_listBB.end(); }


	/*
	 * Checks whether the given native address is a label (explicit or non explicit) or not.  Explicit labels are
	 * addresses that have already been tagged as being labels due to transfers of control to that address.
	 * Non explicit labels are those that belong to basic blocks that have already been constructed (i.e. have
	 * previously been parsed) and now need to be made explicit labels.  In the case of non explicit labels, the
	 * basic block is split into two and types and edges are adjusted accordingly. pNewBB is set to the lower part
	 * of the split BB.
	 * Returns true if the native address is that of an explicit or non explicit label, false otherwise.
	 */
	bool        label(ADDRESS uNativeAddr, BasicBlock *&pNewBB);

	/*
	 * Checks whether the given native address is in the map. If not, returns false. If so, returns true if it is
	 * incomplete.  Otherwise, returns false.
	 */
	bool        isIncomplete(ADDRESS uNativeAddr) const;

	/*
	 * Just checks to see if there exists a BB starting with this native address. If not, the address is NOT added
	 * to the map of labels to BBs.
	 */
	bool        existsBB(ADDRESS uNativeAddr) const;

	/*
	 * Sorts the BBs in the CFG according to the low address of each BB.  Useful because it makes printouts easier,
	 * if they used iterators to traverse the list of BBs.
	 */
	void        sortByAddress();

	/*
	 * Sorts the BBs in the CFG by their first DFT numbers.
	 */
	void        sortByFirstDFT();

	/*
	 * Sorts the BBs in the CFG by their last DFT numbers.
	 */
	void        sortByLastDFT();

	/*
	 * Transforms the input machine-dependent cfg, which has ADDRESS labels for each out-edge, into a machine-
	 * independent cfg graph (i.e. a well-formed graph) which has references to basic blocks for each out-edge.
	 * Returns false if not successful.
	 */
	bool        wellFormCfg();

	/*
	 * Given two basic blocks that belong to a well-formed graph, merges the second block onto the first one and
	 * returns the new block.  The in and out edges links are updated accordingly.
	 * Note that two basic blocks can only be merged if each has a unique out-edge and in-edge respectively, and
	 * these edges correspond to each other.
	 * Returns true if the blocks are merged.
	 */
	bool        mergeBBs(BasicBlock *pb1, BasicBlock *pb2);


	/*
	 * Given a well-formed cfg graph, optimizations are performed on the graph to reduce the number of basic blocks
	 * and edges.
	 * Optimizations performed are: removal of branch chains (i.e. jumps to jumps), removal of redundant jumps (i.e.
	 *  jump to the next instruction), merge basic blocks where possible, and remove redundant basic blocks created
	 *  by the previous optimizations.
	 * Returns false if not successful.
	 */
	bool        compressCfg();


	/*
	 * Given a well-formed cfg graph, a partial ordering is established between the nodes. The ordering is based on
	 * the final visit to each node during a depth first traversal such that if node n1 was visited for the last
	 * time before node n2 was visited for the last time, n1 will be less than n2.
	 * The return value indicates if all nodes where ordered. This will not be the case for incomplete CFGs (e.g.
	 * switch table not completely recognised) or where there are nodes unreachable from the entry node.
	 */
	bool        establishDFTOrder();
	/*
	 * Performs establishDFTOrder on the inverse of the graph (ie, flips the graph)
	 */
	bool        establishRevDFTOrder();

	/*
	 * Given a pointer to a basic block, return an index (e.g. 0 for the first basic block, 1 for the next, ... n-1
	 * for the last BB.
	 */
	int         pbbToIndex(BasicBlock *pBB) const;

	/*
	 * Reset all the traversed flags.
	 * To make this a useful public function, we need access to the traversed flag with other public functions.
	 */
	void        unTraverse();

	/*
	 * Return true if the CFG is well formed.
	 */
	bool        isWellFormed() const;

	/*
	 * Return true if there is a BB at the address given whose first RTL is an orphan, i.e. GetAddress() returns 0.
	 */
	bool        isOrphan(ADDRESS uAddr) const;

	/*
	 * This is called where a two-way branch is deleted, thereby joining a two-way BB with it's successor.
	 * This happens for example when transforming Intel floating point branches, and a branch on parity is deleted.
	 * The joined BB becomes the type of the successor.
	 * Returns true if succeeds.
	 */
	bool        joinBB(BasicBlock *pb1, BasicBlock *pb2);

	/*
	 * Completely remove a BB from the CFG.
	 */
	void        removeBB(BasicBlock *bb);

	/*
	 * Add a call to the set of calls within this procedure.
	 */
	void        addCall(CallStatement *call);

	/*
	 * Get the set of calls within this procedure.
	 */
	std::set<CallStatement *> &getCalls();

	/*
	 * Replace all instances of search with replace.  Can be type sensitive if reqd
	 */
	void        searchAndReplace(Exp *search, Exp *replace);
	bool        searchAll(Exp *search, std::list<Exp *> &result);

	/*
	 * Structures the control flow graph
	 */
	void        structure();

	/*
	 * Add/Remove Junction statements
	 */
	void        addJunctionStatements();
	void        removeJunctionStatements();

	/* Simplify all the expressions in the CFG
	 */
	void        simplify();

	/*
	 * Change the BB enclosing stmt to be CALL, not COMPCALL
	 */
	void        undoComputedBB(Statement *stmt);

private:

	/*
	 * Split the given basic block at the RTL associated with uNativeAddr. The first node's type becomes
	 * fall-through and ends at the RTL prior to that associated with uNativeAddr. The second node's type becomes
	 * the type of the original basic block (pBB), and its out-edges are those of the original basic block.
	 * Precondition: assumes uNativeAddr is an address within the boundaries of the given basic block.
	 * If pNewBB is non zero, it is retained as the "bottom" part of the split, i.e. splitBB just changes the "top"
	 * BB to not overlap the existing one.
	 * Returns a pointer to the "bottom" (new) part of the BB.
	 */
	BasicBlock *splitBB(BasicBlock *pBB, ADDRESS uNativeAddr, BasicBlock *pNewBB = nullptr, bool bDelRtls = false);

	/*
	 * Completes the merge of pb1 and pb2 by adjusting out edges. No checks are made that the merge is valid
	 * (hence this is a private function) Deletes pb1 if bDelete is true
	 */
	void        completeMerge(BasicBlock *pb1, BasicBlock *pb2, bool bDelete);

	/*
	 * checkEntryBB: emit error message if this pointer is null
	 */
	bool        checkEntryBB();

public:
	/*
	 * Split the given BB at the RTL given, and turn it into the BranchStatement given. Sort out all the in and out
	 * edges.
	 */
	BasicBlock *splitForBranch(BasicBlock *pBB, RTL *rtl, BranchStatement *br1, BranchStatement *br2, BB_IT &it);

	/*
	 * Control flow analysis stuff, lifted from Doug Simon's honours thesis.
	 */
	void        setTimeStamps();
	BasicBlock *commonPDom(BasicBlock *curImmPDom, BasicBlock *succImmPDom);
	void        findImmedPDom();
	void        structConds();
	void        structLoops();
	void        checkConds();
	void        determineLoopType(BasicBlock *header, bool *&loopNodes);
	void        findLoopFollow(BasicBlock *header, bool *&loopNodes);
	void        tagNodesInLoop(BasicBlock *header, bool *&loopNodes);

	void        removeUnneededLabels(HLLCode *hll);
	void        generateDot(std::ostream &os) const;


	/*
	 * Get the entry-point or exit BB
	 */
	BasicBlock *getEntryBB() const { return entryBB; }
	BasicBlock *getExitBB() const  { return exitBB; }

	/*
	 * Set the entry-point BB (and exit BB as well)
	 */
	void        setEntryBB(BasicBlock *bb);
	void        setExitBB(BasicBlock *bb);

	BasicBlock *findRetNode() const;

	/*
	 * Set an additional new out edge to a given value
	 */
	void        addNewOutEdge(BasicBlock *fromBB, BasicBlock *newOutEdge);

	/*
	 * print this cfg, mainly for debugging
	 */
	void        print(std::ostream &out, bool html = false) const;
	void        printToLog() const;

	/*
	 * Check for indirect jumps and calls. If any found, decode the extra code and return true
	 */
	bool        decodeIndirectJmp(UserProc *proc);

	/*
	 * Implicit assignments
	 */
	Statement  *findImplicitAssign(Exp *x);                      // Find or create an implicit assign for x
	Statement  *findTheImplicitAssign(Exp *x);                   // Find the existing implicit assign for x (if any)
	Statement  *findImplicitParamAssign(Parameter *p);           // Find exiting implicit assign for parameter p
	void        removeImplicitAssign(Exp *x);                    // Remove an existing implicit assignment for x
	bool        implicitsDone() const { return bImplicitsDone; } // True if implicits have been created
	void        setImplicitsDone() { bImplicitsDone = true; }    // Call when implicits have been created

	void        findInterferences(ConnectionGraph &ig);
	void        appendBBs(std::list<BasicBlock *> &worklist, std::set<BasicBlock *> &workset);

protected:
	void        addBB(BasicBlock *bb) { m_listBB.push_back(bb); }
	friend class XMLProgParser;
};

#endif
