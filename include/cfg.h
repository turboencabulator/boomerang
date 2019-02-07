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

/**
 * Control Flow Graph class.  Contains all the BasicBlock objects for a
 * procedure.  These BBs contain all the RTLs for the procedure, so by
 * traversing the Cfg, one traverses the whole procedure.
 */
class Cfg {
	friend class XMLProgParser;

	/**
	 * Pointer to the UserProc object that contains this CFG object.
	 */
	UserProc   *myProc;

	/**
	 * The list of pointers to BBs.
	 */
	std::list<BasicBlock *> m_listBB;

	/**
	 * Ordering of BBs for control flow structuring.
	 */
	std::vector<BasicBlock *> Ordering;
	std::vector<BasicBlock *> revOrdering;

	/**
	 * The ADDRESS to BasicBlock* map.
	 */
	std::map<ADDRESS, BasicBlock *> m_mapBB;

	/**
	 * The entry and exit BBs.
	 */
	BasicBlock *entryBB = nullptr;
	BasicBlock *exitBB = nullptr;

	/**
	 * True if well formed.
	 */
	bool        m_bWellFormed = false, structured = false;

	/**
	 * Set of the call instructions in this procedure.
	 */
	std::set<CallStatement *> callSites;

	/**
	 * Last label (positive integer) used by any BB this Cfg.
	 */
	int         lastLabel = 0;

	/**
	 * Map from expression to implicit assignment.  The purpose is to
	 * prevent multiple implicit assignments for the same location.
	 */
	std::map<Exp *, Statement *, lessExpStar> implicitMap;

	bool        bImplicitsDone = false;  // True when the implicits are done; they can cause problems (e.g. with
	                                     // ad-hoc global assignment)

public:
	           ~Cfg();

	void        setProc(UserProc *proc);

	void        clear();

	/*
	 * Get the number of BBs
	 */
	unsigned    getNumBBs() const { return m_listBB.size(); }

	Cfg        &operator =(const Cfg &);

	class BBAlreadyExistsError : public std::exception {
	public:
		BasicBlock *pBB;
		BBAlreadyExistsError(BasicBlock *pBB) : pBB(pBB) { }
	};

	BasicBlock *newBB(std::list<RTL *> *pRtls, BBTYPE bbType, int iNumOutEdges) throw (BBAlreadyExistsError);

	BasicBlock *newIncompleteBB(ADDRESS addr);

	void        addOutEdge(BasicBlock *pBB, BasicBlock *pDestBB, bool bSetLabel = false);
	void        addOutEdge(BasicBlock *pBB, ADDRESS adr, bool bSetLabel = false);

	void        setLabel(BasicBlock *pBB);

	/*
	 * Allow iteration over the list of BBs.
	 */
	typedef std::list<BasicBlock *>::iterator iterator;
	iterator    begin() { return m_listBB.begin(); }
	iterator    end()   { return m_listBB.end(); }

	bool        label(ADDRESS uNativeAddr, BasicBlock *&pNewBB);

	bool        isIncomplete(ADDRESS uNativeAddr) const;

	bool        existsBB(ADDRESS uNativeAddr) const;

	void        sortByAddress();
	void        sortByFirstDFT();
	void        sortByLastDFT();

	bool        wellFormCfg();

	bool        mergeBBs(BasicBlock *pb1, BasicBlock *pb2);

	bool        compressCfg();

	bool        establishDFTOrder();
	bool        establishRevDFTOrder();

	int         pbbToIndex(BasicBlock *pBB) const;

	void        unTraverse();

	bool        isWellFormed() const;

	bool        isOrphan(ADDRESS uAddr) const;

	bool        joinBB(BasicBlock *pb1, BasicBlock *pb2);

	void        removeBB(const BasicBlock *);

	void        addCall(CallStatement *call);

	std::set<CallStatement *> &getCalls();

	void        searchAndReplace(Exp *search, Exp *replace);
	bool        searchAll(Exp *search, std::list<Exp *> &result);

	void        structure();

	void        addJunctionStatements();
	void        removeJunctionStatements();

	void        simplify();

	void        undoComputedBB(Statement *stmt);

private:

	BasicBlock *splitBB(BasicBlock *pBB, ADDRESS uNativeAddr);

	void        completeMerge(const BasicBlock *, BasicBlock *);

	bool        checkEntryBB();

public:
	BasicBlock *splitForBranch(iterator &, RTL *);

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

	void        setEntryBB(BasicBlock *bb);
	void        setExitBB(BasicBlock *bb);

	BasicBlock *findRetNode() const;

	void        print(std::ostream &out, bool html = false) const;

	bool        decodeIndirectJmp(UserProc *proc);

	/*
	 * Implicit assignments
	 */
	Statement  *findImplicitAssign(Exp *x);
	Statement  *findTheImplicitAssign(Exp *x);
	Statement  *findImplicitParamAssign(Parameter *p);
	void        removeImplicitAssign(Exp *x);
	bool        implicitsDone() const { return bImplicitsDone; } // True if implicits have been created
	void        setImplicitsDone() { bImplicitsDone = true; }    // Call when implicits have been created

	void        findInterferences(ConnectionGraph &ig);
};

#endif
