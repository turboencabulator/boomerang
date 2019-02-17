/**
 * \file
 * \brief Interface for the basic block class, which form nodes of the control
 *        flow graph.
 *
 * \authors
 * Copyright (C) 1997-2000, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002-2006, Trent Waddington and Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef BASICBLOCK_H
#define BASICBLOCK_H

#include "managed.h"  // For LocationSet etc
#include "rtl.h"
#include "types.h"

#include <ostream>
#include <exception>
#include <list>
#include <string>
#include <vector>

class BasicBlock;
class Exp;
class HLLCode;
class Proc;
class Statement;
class UserProc;

/*  *   *   *   *   *   *   *   *   *   *   *   *   *   *   *\
*                                                            *
*   e n u m s   u s e d   i n   C f g . h   a n d   h e r e  *
*                                                            *
\*  *   *   *   *   *   *   *   *   *   *   *   *   *   *   */

/**
 * Depth-first traversal constants.
 */
enum travType {
	UNTRAVERSED,  ///< Initial value.
	DFS_TAG,      ///< Remove redundant nodes pass.
	DFS_LNUM,     ///< DFS loop stamping pass.
	DFS_RNUM,     ///< DFS reverse loop stamping pass.
	DFS_CASE,     ///< DFS case head tagging traversal.
	DFS_PDOM,     ///< DFS post dominator ordering.
	DFS_CODEGEN   ///< Code generating pass.
};

/**
 * An enumerated type for the class of stucture determined for a node.
 */
enum structType {
	Loop,      ///< Header of a loop only.
	Cond,      ///< Header of a conditional only (if-then-else or switch).
	LoopCond,  ///< Header of a loop and a conditional.
	Seq        ///< Sequential statement (default).
};

/**
 * A type for the class of unstructured conditional jumps.
 */
enum unstructType {
	Structured,
	JumpInOutLoop,
	JumpIntoCase
};

/**
 * An enumerated type for the type of conditional headers.
 */
enum condType {
	IfThen,      ///< Conditional with only a then clause.
	IfThenElse,  ///< Conditional with a then and an else clause.
	IfElse,      ///< Conditional with only an else clause.
	Case         ///< Nway conditional header (case statement).
};

/**
 * An enumerated type for the type of loop headers.
 */
enum loopType {
	PreTested,   ///< Header of a while loop.
	PostTested,  ///< Header of a repeat loop.
	Endless      ///< Header of an endless loop.
};

/*  *   *   *   *   *   *   *   *   *\
*                                    *
*   B a s i c B l o c k   e n u m s  *
*                                    *
\*  *   *   *   *   *   *   *   *   */

/**
 * Kinds of basic block nodes.
 *
 * Reordering these will break the save files - trent
 */
enum BBTYPE {
	ONEWAY,         ///< Unconditional branch.
	TWOWAY,         ///< Conditional branch.
	NWAY,           ///< Case branch.
	CALL,           ///< Procedure call.
	RET,            ///< Return.
	FALL,           ///< Fall-through node.
	COMPJUMP,       ///< Computed jump.
	COMPCALL,       ///< Computed call.
	INVALID         ///< Invalid instruction.
};

enum SBBTYPE {
	NONE,           ///< Not structured.
	PRETESTLOOP,    ///< Header of a loop.
	POSTTESTLOOP,
	ENDLESSLOOP,
	JUMPINOUTLOOP,  ///< An unstructured jump in or out of a loop.
	JUMPINTOCASE,   ///< An unstructured jump into a case statement.
	IFGOTO,         ///< Unstructured conditional.
	IFTHEN,         ///< Conditional with then clause.
	IFTHENELSE,     ///< Conditional with then and else clauses.
	IFELSE,         ///< Conditional with else clause only.
	CASE            ///< Case statement (switch).
};

/**
 * \brief Basic Block class
 */
class BasicBlock {
	/*
	 * Objects of class Cfg can access the internals of a BasicBlock object.
	 */
	friend class Cfg;
	friend class XMLProgParser;

public:
	                    BasicBlock() = default;
	                   ~BasicBlock();
	                    BasicBlock(const BasicBlock &) = default;

	        BBTYPE      getType() const;
	        void        updateType(BBTYPE, int);

	        bool        isCaseOption() const;

	        bool        isTraversed() const;
	        void        setTraversed(bool);

	/*
	 * Don't use = std::cout, because gdb doesn't know about std::
	 */
	        void        print(std::ostream &, bool = false) const;
	        std::string prints() const;  // For debugging

	        ADDRESS     getLowAddr() const;
	        ADDRESS     getHiAddr() const;

	        std::list<RTL *> *getRTLs() const;

	        RTL        *getRTLWithStatement(Statement *) const;

	        const std::vector<BasicBlock *> &getInEdges() const;
	        const std::vector<BasicBlock *> &getOutEdges() const;

	        int         getNumInEdges() const { return m_InEdges.size(); }
	        int         getNumOutEdges() const { return m_iNumOutEdges; }

	        BasicBlock *getOutEdge(unsigned int);
	        BasicBlock *getCorrectOutEdge(ADDRESS) const;

	        void        setOutEdge(int, BasicBlock *);

	        int         whichPred(BasicBlock *) const;

	        void        addInEdge(BasicBlock *);

	        void        deleteInEdge(BasicBlock *);
	        void        deleteEdge(BasicBlock *);

	        ADDRESS     getCallDest() const;
	        Proc       *getCallDestProc() const;

	//        unsigned    DFTOrder(int &, int &);
	//        unsigned    RevDFTOrder(int &, int &);

	static  bool        lessAddress(BasicBlock *, BasicBlock *);
	//static  bool        lessFirstDFT(BasicBlock *, BasicBlock *);
	//static  bool        lessLastDFT(BasicBlock *, BasicBlock *);

	class LastStatementNotABranchError : public std::exception {
	public:
		Statement *stmt;
		LastStatementNotABranchError(Statement *stmt) : stmt(stmt) { }
	};
	        Exp        *getCond() const throw (LastStatementNotABranchError);

	        void        setCond(Exp *) throw (LastStatementNotABranchError);

	class LastStatementNotAGotoError : public std::exception {
	public:
		Statement *stmt;
		LastStatementNotAGotoError(Statement *stmt) : stmt(stmt) { }
	};
	        Exp        *getDest() const throw (LastStatementNotAGotoError);

	        bool        isJmpZ(BasicBlock *) const;

	        BasicBlock *getLoopBody() const;

	        void        simplify();

	/*
	 * Depth first traversal of all bbs, numbering as we go and as we come back, forward and reverse passes.
	 * Use Cfg::establishDFTOrder() and CFG::establishRevDFTOrder to create these values.
	 */
	//        int         m_DFTfirst = 0; // depth-first traversal first visit
	//        int         m_DFTlast = 0;  // depth-first traversal last visit
	//        int         m_DFTrevfirst;  // reverse depth-first traversal first visit
	//        int         m_DFTrevlast;   // reverse depth-first traversal last visit

private:
	                    BasicBlock(std::list<RTL *> *, BBTYPE, int);

	        void        setRTLs(std::list<RTL *> *);

public:

	/* high level structuring */
	        SBBTYPE     m_structType = NONE;    // structured type of this node
	        SBBTYPE     m_loopCondType = NONE;  // type of conditional to treat this loop header as (if any)
	        BasicBlock *m_loopHead = nullptr;   // head of the most nested enclosing loop
	        BasicBlock *m_caseHead = nullptr;   // head of the most nested enclosing case
	        BasicBlock *m_condFollow = nullptr; // follow of a conditional header
	        BasicBlock *m_loopFollow = nullptr; // follow of a loop header
	        BasicBlock *m_latchNode = nullptr;  // latch node of a loop header

protected:
	/* general basic block information */
	        BBTYPE      m_nodeType = INVALID;   // type of basic block
	        std::list<RTL *> *m_pRtls = nullptr;// Ptr to list of RTLs
	        bool        m_bIncomplete = true;   // True if not yet complete

	/* in-edges and out-edges */
	        std::vector<BasicBlock *> m_InEdges; // Vector of in-edges
	        std::vector<BasicBlock *> m_OutEdges;// Vector of out-edges
	        int         m_iNumOutEdges = 0; // We need this because GCC doesn't support resize() of vectors!

	/* for traversal */
	        bool        m_iTraversed = false;  // traversal marker

	/* Liveness */
	        LocationSet liveIn;         // Set of locations live at BB start

public:
	        Proc       *getDestProc() const;

	/*
	 * Get first/next statement this BB.
	 * Somewhat intricate because of the post call semantics; these funcs
	 * save a lot of duplicated, easily-bugged code.
	 */
	typedef std::list<RTL *>::iterator rtlit;
	typedef std::list<RTL *>::reverse_iterator rtlrit;
	typedef RTL::iterator stlit;
	typedef RTL::reverse_iterator stlrit;
	        Statement  *getFirstStmt(rtlit &, stlit &);
	        Statement  *getNextStmt(rtlit &, stlit &);
	        Statement  *getLastStmt(rtlrit &, stlrit &);
	        Statement  *getPrevStmt(rtlrit &, stlrit &);
	        Statement  *getFirstStmt();
	        Statement  *getLastStmt();
	        RTL        *getLastRtl() { return m_pRtls->back(); }

	        void        getStatements(StatementList &);

protected:
	/* Control flow analysis stuff, lifted from Doug Simon's honours thesis.
	 */
	        int         ord = -1;     // node's position within the ordering structure
	        int         revOrd = -1;  // position within ordering structure for the reverse graph
	        int         inEdgesVisited = 0; // counts the number of in edges visited during a DFS
	        int         numForwardInEdges = -1; // inedges to this node that aren't back edges
	        int         loopStamps[2], revLoopStamps[2]; // used for structuring analysis
	        travType    traversed = UNTRAVERSED; // traversal flag for the numerous DFS's
	        bool        hllLabel = false; // emit a label for this node when generating HL code?
	        int         indentLevel = 0; // the indentation level of this node in the final code

	// analysis information
	        BasicBlock *immPDom = nullptr; // immediate post dominator
	        BasicBlock *loopHead = nullptr; // head of the most nested enclosing loop
	        BasicBlock *caseHead = nullptr; // head of the most nested enclosing case
	        BasicBlock *condFollow = nullptr; // follow of a conditional header
	        BasicBlock *loopFollow = nullptr; // follow of a loop header
	        BasicBlock *latchNode = nullptr; // latching node of a loop header

	// Structured type of the node
	        structType  sType = Seq; // the structuring class (Loop, Cond , etc)
	        unstructType usType = Structured; // the restructured type of a conditional header
	        loopType    lType; // the loop type of a loop header
	        condType    cType; // the conditional type of a conditional header

	        void        setLoopStamps(int &, std::vector<BasicBlock *> &);
	        void        setRevLoopStamps(int &);
	        void        setRevOrder(std::vector<BasicBlock *> &);

	        void        setLoopHead(BasicBlock *head) { loopHead = head; }
	        BasicBlock *getLoopHead() const { return loopHead; }
	        void        setLatchNode(BasicBlock *latch) { latchNode = latch; }
	        bool        isLatchNode() const { return loopHead && loopHead->latchNode == this; }
	        BasicBlock *getLatchNode() const { return latchNode; }
	        BasicBlock *getCaseHead() const { return caseHead; }
	        void        setCaseHead(BasicBlock *, BasicBlock *);

	        structType  getStructType() const { return sType; }
	        void        setStructType(structType);

	        unstructType getUnstructType() const;
	        void        setUnstructType(unstructType);

	        loopType    getLoopType() const;
	        void        setLoopType(loopType);

	        condType    getCondType() const;
	        void        setCondType(condType);

	        void        setLoopFollow(BasicBlock *other) { loopFollow = other; }
	        BasicBlock *getLoopFollow() const { return loopFollow; }

	        void        setCondFollow(BasicBlock *other) { condFollow = other; }
	        BasicBlock *getCondFollow() const { return condFollow; }

	        bool        hasBackEdgeTo(const BasicBlock *) const;

	// establish if this bb has any back edges leading FROM it
	        bool        hasBackEdge() const {
		                    for (const auto &succ : m_OutEdges)
			                    if (hasBackEdgeTo(succ))
				                    return true;
		                    return false;
	                    }

public:
	        bool        isBackEdge(const BasicBlock *) const;

protected:
	        bool        isAncestorOf(const BasicBlock *) const;

	        bool        inLoop(BasicBlock *, BasicBlock *) const;

	        bool        isIn(std::list<BasicBlock *> &set, BasicBlock *bb) const {
		                    for (const auto &elem : set)
			                    if (elem == bb)
				                    return true;
		                    return false;
	                    }

	        bool        allParentsGenerated() const;
	        void        emitGotoAndLabel(HLLCode *, int, BasicBlock *);
	        void        WriteBB(HLLCode *, int);

public:
	        void        generateCode(HLLCode *, int, BasicBlock *, std::list<BasicBlock *> &, std::list<BasicBlock *> &, UserProc *);
	        void        prependStmt(Statement *, UserProc *);

	// Liveness
	        bool        calcLiveness(ConnectionGraph &, UserProc *);
	        void        getLiveOut(LocationSet &, LocationSet &);

	        bool        decodeIndirectJmp(UserProc *);
	        void        processSwitch(UserProc *);
	        int         findNumCases();

	        bool        undoComputedBB(Statement *);

	// true if processing for overlapped registers on statements in this BB
	// has been completed.
	        bool        overlappedRegProcessingDone = false;
};

#endif
