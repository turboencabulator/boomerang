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

public:
	                    BasicBlock() = default;
	                   ~BasicBlock();
	                    BasicBlock(const BasicBlock &bb);

	        BBTYPE      getType() const;
	        void        updateType(BBTYPE bbType, int iNumOutEdges);

	        int         getLabel() const;

	        bool        isCaseOption() const;

	        bool        isTraversed() const;
	        void        setTraversed(bool bTraversed);

	/*
	 * Don't use = std::cout, because gdb doesn't know about std::
	 */
	        void        print(std::ostream &os, bool html = false) const;
	        std::string prints() const;  // For debugging

	        void        setJumpReqd();
	        bool        isJumpReqd() const;

	        ADDRESS     getLowAddr() const;
	        ADDRESS     getHiAddr() const;

	        std::list<RTL *> *getRTLs() const;

	        RTL        *getRTLWithStatement(Statement *stmt) const;

	        const std::vector<BasicBlock *> &getInEdges() const;
	        const std::vector<BasicBlock *> &getOutEdges() const;

	        int         getNumInEdges() const { return m_InEdges.size(); }
	        int         getNumOutEdges() const { return m_iNumOutEdges; }

	        BasicBlock *getOutEdge(unsigned int i);
	        BasicBlock *getCorrectOutEdge(ADDRESS a) const;

	        void        setOutEdge(int, BasicBlock *);

	        int         whichPred(BasicBlock *pred) const;

	        void        addInEdge(BasicBlock *);

	        void        deleteInEdge(BasicBlock *edge);
	        void        deleteEdge(BasicBlock *edge);

	        ADDRESS     getCallDest() const;
	        Proc       *getCallDestProc() const;

	        unsigned    DFTOrder(int &first, int &last);
	        unsigned    RevDFTOrder(int &first, int &last);

	static  bool        lessAddress(BasicBlock *bb1, BasicBlock *bb2);
	static  bool        lessFirstDFT(BasicBlock *bb1, BasicBlock *bb2);
	static  bool        lessLastDFT(BasicBlock *bb1, BasicBlock *bb2);

	class LastStatementNotABranchError : public std::exception {
	public:
		Statement *stmt;
		LastStatementNotABranchError(Statement *stmt) : stmt(stmt) { }
	};
	        Exp        *getCond() const throw (LastStatementNotABranchError);

	        void        setCond(Exp *e) throw (LastStatementNotABranchError);

	class LastStatementNotAGotoError : public std::exception {
	public:
		Statement *stmt;
		LastStatementNotAGotoError(Statement *stmt) : stmt(stmt) { }
	};
	        Exp        *getDest() const throw (LastStatementNotAGotoError);

	        bool        isJmpZ(BasicBlock *dest) const;

	        BasicBlock *getLoopBody() const;

	        void        simplify();

	/*
	 * Depth first traversal of all bbs, numbering as we go and as we come back, forward and reverse passes.
	 * Use Cfg::establishDFTOrder() and CFG::establishRevDFTOrder to create these values.
	 */
	        int         m_DFTfirst = 0; // depth-first traversal first visit
	        int         m_DFTlast = 0;  // depth-first traversal last visit
	        int         m_DFTrevfirst;  // reverse depth-first traversal first visit
	        int         m_DFTrevlast;   // reverse depth-first traversal last visit

private:
	                    BasicBlock(std::list<RTL *> *pRtls, BBTYPE bbType, int iNumOutEdges);

	        void        setRTLs(std::list<RTL *> *rtls);

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
	        int         m_iLabelNum = 0;        // Nonzero if start of BB needs label
	        bool        m_bIncomplete = true;   // True if not yet complete
	        bool        m_bJumpReqd = false;    // True if jump required for "fall through"

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
	        Statement  *getFirstStmt(rtlit &rit, stlit &sit);
	        Statement  *getNextStmt(rtlit &rit, stlit &sit);
	        Statement  *getLastStmt(rtlrit &rrit, stlrit &srit);
	        Statement  *getPrevStmt(rtlrit &rrit, stlrit &srit);
	        Statement  *getFirstStmt();
	        Statement  *getLastStmt();
	        RTL        *getLastRtl() { return m_pRtls->back(); }

	        void        getStatements(StatementList &stmts);

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

	        void        setLoopStamps(int &time, std::vector<BasicBlock *> &order);
	        void        setRevLoopStamps(int &time);
	        void        setRevOrder(std::vector<BasicBlock *> &order);

	        void        setLoopHead(BasicBlock *head) { loopHead = head; }
	        BasicBlock *getLoopHead() const { return loopHead; }
	        void        setLatchNode(BasicBlock *latch) { latchNode = latch; }
	        bool        isLatchNode() const { return loopHead && loopHead->latchNode == this; }
	        BasicBlock *getLatchNode() const { return latchNode; }
	        BasicBlock *getCaseHead() const { return caseHead; }
	        void        setCaseHead(BasicBlock *head, BasicBlock *follow);

	        structType  getStructType() const { return sType; }
	        void        setStructType(structType s);

	        unstructType getUnstructType() const;
	        void        setUnstructType(unstructType us);

	        loopType    getLoopType() const;
	        void        setLoopType(loopType l);

	        condType    getCondType() const;
	        void        setCondType(condType l);

	        void        setLoopFollow(BasicBlock *other) { loopFollow = other; }
	        BasicBlock *getLoopFollow() const { return loopFollow; }

	        void        setCondFollow(BasicBlock *other) { condFollow = other; }
	        BasicBlock *getCondFollow() const { return condFollow; }

	        bool        hasBackEdgeTo(const BasicBlock *dest) const;

	// establish if this bb has any back edges leading FROM it
	        bool        hasBackEdge() const {
		                    for (const auto &edge : m_OutEdges)
			                    if (hasBackEdgeTo(edge))
				                    return true;
		                    return false;
	                    }

public:
	        bool        isBackEdge(int inEdge) const;

protected:
	        bool        isAncestorOf(const BasicBlock *other) const;

	        bool        inLoop(BasicBlock *header, BasicBlock *latch) const;

	        bool        isIn(std::list<BasicBlock *> &set, BasicBlock *bb) const {
		                    for (const auto &elem : set)
			                    if (elem == bb)
				                    return true;
		                    return false;
	                    }

	        bool        allParentsGenerated() const;
	        void        emitGotoAndLabel(HLLCode *hll, int indLevel, BasicBlock *dest);
	        void        WriteBB(HLLCode *hll, int indLevel);

public:
	        void        generateCode(HLLCode *hll, int indLevel, BasicBlock *latch, std::list<BasicBlock *> &followSet, std::list<BasicBlock *> &gotoSet, UserProc *proc);
	        void        prependStmt(Statement *s, UserProc *proc);

	// Liveness
	        bool        calcLiveness(ConnectionGraph &ig, UserProc *proc);
	        void        getLiveOut(LocationSet &live, LocationSet &phiLocs);

	        bool        decodeIndirectJmp(UserProc *proc);
	        void        processSwitch(UserProc *proc);
	        int         findNumCases();

	        bool        undoComputedBB(Statement *stmt);

	// true if processing for overlapped registers on statements in this BB
	// has been completed.
	        bool        overlappedRegProcessingDone = false;

protected:
	friend class XMLProgParser;
	        void        addOutEdge(BasicBlock *bb) {
		                    m_OutEdges.push_back(bb);
		                    ++m_iNumOutEdges;
	                    }
	        void        addRTL(RTL *rtl) {
		                    if (!m_pRtls)
			                    m_pRtls = new std::list<RTL *>;
		                    m_pRtls->push_back(rtl);
	                    }
	        void        addLiveIn(Exp *e) { liveIn.insert(e); }
};

#endif
