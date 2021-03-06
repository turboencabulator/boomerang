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
#include <queue>
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
	 * The queue of addresses still to be processed.
	 */
	std::queue<ADDRESS> targets;

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
	 * Map from expression to implicit assignment.  The purpose is to
	 * prevent multiple implicit assignments for the same location.
	 */
	std::map<Exp *, Statement *, lessExpStar> implicitMap;

	bool        bImplicitsDone = false;  // True when the implicits are done; they can cause problems (e.g. with
	                                     // ad-hoc global assignment)

public:
	           ~Cfg();

	void        setProc(UserProc *);

	void        clear();

	/*
	 * Get the number of BBs
	 */
	unsigned    getNumBBs() const { return m_listBB.size(); }

	Cfg        &operator =(const Cfg &);

	class BBAlreadyExistsError : public std::exception {
	public:
		BasicBlock *pBB;  ///< The existing BB
		BBAlreadyExistsError(BasicBlock *pBB) : pBB(pBB) { }
	};

	BasicBlock *newBB(std::list<RTL *> *, BBTYPE) throw (BBAlreadyExistsError);
	void        addOutEdge(BasicBlock *&, ADDRESS);
	void        label(BasicBlock *&, ADDRESS);

	/*
	 * Allow iteration over the list of BBs.
	 */
	typedef std::list<BasicBlock *>::iterator iterator;
	iterator    begin() { return m_listBB.begin(); }
	iterator    end()   { return m_listBB.end(); }

	/*
	 * Target queue logic
	 */
	void        enqueue(ADDRESS);
	ADDRESS     dequeue();

	bool        existsBB(ADDRESS) const;
	bool        isComplete(ADDRESS) const;

	void        sortByAddress();
	//void        sortByFirstDFT();
	//void        sortByLastDFT();

	bool        wellFormCfg();

	bool        compressCfg();

	//bool        establishDFTOrder();
	//bool        establishRevDFTOrder();

	void        unTraverse();

	bool        isWellFormed() const;

	bool        isOrphan(ADDRESS) const;

	bool        joinBB(BasicBlock *, BasicBlock *);

	void        removeBB(BasicBlock *);

	void        searchAndReplace(Exp *, Exp *);
	bool        searchAll(Exp *, std::list<Exp *> &);

	void        structure();

	//void        addJunctionStatements();
	//void        removeJunctionStatements();

	void        simplify();

	void        undoComputedBB(Statement *);

private:

	BasicBlock *splitBB(BasicBlock *, ADDRESS);
	BasicBlock *splitBB(BasicBlock *, std::list<RTL *>::iterator);

	bool        checkEntryBB();

public:
	void        splitForBranch(BasicBlock *, std::list<RTL *>::iterator);

	/*
	 * Control flow analysis stuff, lifted from Doug Simon's honours thesis.
	 */
	bool        setTimeStamps();
	BasicBlock *commonPDom(BasicBlock *, BasicBlock *);
	void        findImmedPDom();
	void        structConds();
	void        structLoops();
	void        checkConds();
	void        determineLoopType(BasicBlock *, bool *&);
	void        findLoopFollow(BasicBlock *, bool *&);
	void        tagNodesInLoop(BasicBlock *, bool *&);

	void        removeUnneededLabels(HLLCode *);
	void        generateDot(std::ostream &) const;


	/*
	 * Get the entry-point or exit BB
	 */
	BasicBlock *getEntryBB() const { return entryBB; }
	BasicBlock *getExitBB() const  { return exitBB; }

	void        setEntryBB(BasicBlock *);
	void        setExitBB(BasicBlock *);

	BasicBlock *findRetNode() const;

	void        print(std::ostream &, bool = false) const;

	bool        decodeIndirectJmp(UserProc *);

	/*
	 * Implicit assignments
	 */
	Statement  *findImplicitAssign(Exp *);
	Statement  *findTheImplicitAssign(Exp *);
	Statement  *findImplicitParamAssign(Parameter *);
	void        removeImplicitAssign(Exp *);
	bool        implicitsDone() const { return bImplicitsDone; } // True if implicits have been created
	void        setImplicitsDone() { bImplicitsDone = true; }    // Call when implicits have been created

	void        findInterferences(ConnectionGraph &);
};

#endif
