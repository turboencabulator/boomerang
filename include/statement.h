/**
 * \file
 * \brief The Statement and related classes (was dataflow.h)
 *
 *     Class hierarchy:     Statement@            (@ = abstract)
 *                        __/   |   \________________________
 *                       /      |            \               \
 *           GotoStatement  TypingStatement@  ReturnStatement JunctionStatement
 *     BranchStatement_/     /          \
 *     CaseStatement__/  Assignment@   ImpRefStatement
 *     CallStatement_/  /   /    \ \________
 *           PhiAssign_/ Assign  BoolAssign \_ImplicitAssign
 *
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef STATEMENT_H
#define STATEMENT_H

#include "boomerang.h"  // For USE_DOMINANCE_NUMS etc
#include "dataflow.h"   // For embedded objects DefCollector and UseCollector
#include "managed.h"
#include "types.h"

#include <iostream>     // For std::cout
#include <ostream>
#include <memory>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <cassert>

class Assign;
class BasicBlock;
//class Cfg;
class Const;
class Exp;
class HLLCode;
class Proc;
class Prog;
class RefExp;
class ReturnStatement;
class Signature;
class StmtExpVisitor;
class StmtModifier;
class StmtPartModifier;
class StmtVisitor;
class Type;
class UserProc;
class lessExpStar;

typedef std::set<UserProc *> CycleSet;

/**
 * Kinds of Statements, or high-level register transfer lists.
 */
enum STMT_KIND {
	STMT_ASSIGN = 0,
	STMT_PHIASSIGN,
	STMT_IMPASSIGN,
	STMT_BOOLASSIGN,        // For "setCC" instructions that set destination
	                        // to 1 or 0 depending on the condition codes.
	STMT_CALL,
	STMT_RET,
	STMT_BRANCH,
	STMT_GOTO,
	STMT_CASE,              // Represent  a switch statement
	STMT_IMPREF,
	STMT_JUNCTION
};

/**
 * These values indicate what kind of conditional jump or conditional assign
 * is being performed.
 *
 * Changing the order of these will result in save files not working - trent
 */
enum BRANCH_TYPE {
	BRANCH_JE = 0,          ///< Jump if equals.
	BRANCH_JNE,             ///< Jump if not equal.
	BRANCH_JSL,             ///< Jump if signed less.
	BRANCH_JSLE,            ///< Jump if signed less or equal.
	BRANCH_JSGE,            ///< Jump if signed greater or equal.
	BRANCH_JSG,             ///< Jump if signed greater.
	BRANCH_JUL,             ///< Jump if unsigned less.
	BRANCH_JULE,            ///< Jump if unsigned less or equal.
	BRANCH_JUGE,            ///< Jump if unsigned greater or equal.
	BRANCH_JUG,             ///< Jump if unsigned greater.
	BRANCH_JMI,             ///< Jump if result is minus.
	BRANCH_JPOS,            ///< Jump if result is positive.
	BRANCH_JOF,             ///< Jump if overflow.
	BRANCH_JNOF,            ///< Jump if no overflow.
	BRANCH_JPAR             ///< Jump if parity even (Intel only).
};

/**
 * Statements define values that are used in expressions.  They are akin to
 * "definition" in the Dragon Book.
 */
class Statement {
	friend class XMLProgParser;

protected:
	        BasicBlock *pbb = nullptr;     // contains a pointer to the enclosing BB
	        UserProc   *proc = nullptr;    // procedure containing this statement
	        int         number = 0;        // Statement number for printing
#if USE_DOMINANCE_NUMS
	        int         dominanceNum;      // Like a statement number, but has dominance properties
public:
	        int         getDomNumber() const { return dominanceNum; }
	        void        setDomNumber(int dn) { dominanceNum = dn; }
protected:
#endif
	        Statement  *parent = nullptr;  // The statement that contains this one
	//        RangeMap    ranges;            // overestimation of ranges of locations
	//        RangeMap    savedInputRanges;  // saved overestimation of ranges of locations

	//        unsigned int lexBegin, lexEnd;

public:
	virtual            ~Statement() = default;

	// get/set the enclosing BB, etc
	        BasicBlock *getBB() const { return pbb; }
	        void        setBB(BasicBlock *bb) { pbb = bb; }

	        //bool        operator ==(Statement &o);
	// Get and set *enclosing* proc (not destination proc)
	        void        setProc(UserProc *p);
	        UserProc   *getProc() const { return proc; }

	        int         getNumber() const { return number; }
	virtual void        setNumber(int num) { number = num; }  // Overridden for calls (and maybe later returns)

	virtual STMT_KIND   getKind() const = 0;

	        void        setParent(Statement *par) { parent = par; }
	        Statement  *getParent() const { return parent; }

	//        RangeMap   &getRanges() { return ranges; }
	//        void        clearRanges() { ranges.clear(); }

	virtual Statement  *clone() const = 0;  // Make copy of self

	virtual bool        accept(StmtVisitor &) = 0;
	virtual bool        accept(StmtExpVisitor &) = 0;
	virtual bool        accept(StmtModifier &) = 0;
	virtual bool        accept(StmtPartModifier &) = 0;

	//        void        setLexBegin(unsigned int n) { lexBegin = n; }
	//        void        setLexEnd(unsigned int n) { lexEnd = n; }
	//        unsigned    int getLexBegin() const { return lexBegin; }
	//        unsigned    int getLexEnd() const { return lexEnd; }
	//        Exp        *getExpAtLex(unsigned int, unsigned int) const;


	// returns true if this statement defines anything
	virtual bool        isDefinition() const = 0;

	// true if is a null statement
	virtual bool        isNullStatement() const { return false; }

	// true if this statment is a flags assignment
	virtual bool        isFlagAssgn() const { return false; }

	// true if this statement is a decoded ICT.
	// NOTE: for now, it only represents decoded indirect jump instructions
	        bool        isHL_ICT() const { return getKind() == STMT_CASE; }

	// true if this is a fpush/fpop
	virtual bool        isFpush() const { return false; }
	virtual bool        isFpop() const { return false; }

	// returns a set of locations defined by this statement
	// Classes with no definitions (e.g. GotoStatement and children) don't override this
	virtual void        getDefinitions(LocationSet &def) const { }

	// set the left for forExp to newExp
	virtual void        setLeftFor(Exp *forExp, Exp *newExp) { assert(0); }
	virtual bool        definesLoc(Exp *loc) const { return false; }  // True if this Statement defines loc

	// returns true if this statement uses the given expression
	virtual bool        usesExp(Exp *e) const = 0;

	// statements should be printable (for debugging)
	virtual void        print(std::ostream &os, bool html = false) const = 0;
	        void        printAsUse(std::ostream &os) const   { os << number; }
	        void        printAsUseBy(std::ostream &os) const { os << number; }
	        void        printNum(std::ostream &os) const     { os << number; }
	        std::string prints() const;

	// general search
	virtual bool        search(Exp *search, Exp *&result) = 0;
	virtual bool        searchAll(Exp *search, std::list<Exp *> &result) = 0;

	// general search and replace. Set cc true to change collectors as well. Return true if any change
	virtual bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) = 0;

	static  bool        canPropagateToExp(Exp *);
	        bool        propagateTo(bool &, std::map<Exp *, int, lessExpStar> * = nullptr, LocationSet * = nullptr, bool = false);
	        bool        propagateFlagsTo();

	// code generation
	virtual void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) = 0;

	// simpify internal expressions
	virtual void        simplify() = 0;

	// simplify internal address expressions (a[m[x]] -> x) etc
	// Only Assignments override at present
	virtual void        simplifyAddr() { }

	        void        mapRegistersToLocals();

	        void        replaceSubscriptsWithLocals();

	        void        insertCasts();

	// fixSuccessor
	// Only Assign overrides at present
	virtual void        fixSuccessor() { }

	// Generate constraints (for constraint based type analysis)
	virtual void        genConstraints(LocationSet &cons) { }

	// Data flow based type analysis
	virtual void        dfaTypeAnalysis(bool &) { }
	        Type       *meetWithFor(Type *, Exp *, bool &);

	// Range analysis
protected:
	//        void        updateRanges(RangeMap &, std::list<Statement *> &, bool = false);
public:
	//        RangeMap   &getSavedInputRanges() { return savedInputRanges; }
	//        RangeMap    getInputRanges();
	//virtual void        rangeAnalysis(std::list<Statement *> &);

	// helper functions
	        bool        isFirstStatementInBB() const;
	        bool        isLastStatementInBB() const;
	        Statement  *getNextStatementInBB() const;
	        Statement  *getPreviousStatementInBB() const;


//  //  //  //  //  //  //  //  //  //
//                                  //
//  Statement visitation functions  //
//                                  //
//  //  //  //  //  //  //  //  //  //

	        void        addUsedLocs(LocationSet &, bool = false, bool = false);
	        bool        addUsedLocals(LocationSet &);
	        void        bypass();

	        bool        replaceRef(Exp *, Assign *, bool &);

	        void        findConstants(std::list<Const *> &);

	        int         setConscripts(int);
	        void        clearConscripts();

	        void        stripSizes();

	        void        subscriptVar(Exp *, Statement * /*, Cfg * */);

	        bool        castConst(int, Type *);

	        void        dfaMapLocals();

	// End Statement visitation functions


	// Get the type for the definition, if any, for expression e in this statement
	// Overridden only by Assignment and CallStatement, and ReturnStatement.
	virtual Type       *getTypeFor(Exp *e) const { return nullptr; }
	// Set the type for the definition of e in this Statement
	virtual void        setTypeFor(Exp *e, Type *ty) { assert(0); }

	        bool        doPropagateTo(Exp *, Assign *, bool &);
	static  bool        calcMayAlias(Exp *, Exp *, int);
	static  bool        mayAlias(Exp *, Exp *, int);
};

std::ostream &operator <<(std::ostream &, const Statement *);
std::ostream &operator <<(std::ostream &, const Statement &);

/**
 * TypingStatement is an abstract subclass of Statement.  It has a type,
 * representing the type of a reference or an assignment.
 */
class TypingStatement : public Statement {
protected:
	Type       *type;  // The type for this assignment or reference

public:
	            TypingStatement(Type *);

	// Get and set the type.
	Type       *getType() const { return type; }
	void        setType(Type *ty) { type = ty; }
};

/**
 * Assignment is an abstract subclass of TypingStatement, holding a location.
 */
class Assignment : public TypingStatement {
	friend class XMLProgParser;

protected:
	Exp        *lhs;  // The left hand side

public:
	            Assignment(Exp *);
	            Assignment(Type *, Exp *);

	// We also want operator < for assignments. For example, we want ReturnStatement to contain a set of (pointers
	// to) Assignments, so we can automatically make sure that existing assignments are not duplicated
	// Assume that we won't want sets of assignments differing by anything other than LHSs
	bool        operator <(const Assignment &o) const { return lhs < o.lhs; }

	void        print(std::ostream &, bool = false) const override;
	virtual void printCompact(std::ostream &os, bool html = false) const = 0;  // Without statement number

	Type       *getTypeFor(Exp *) const override;
	void        setTypeFor(Exp *, Type *) override;

	bool        usesExp(Exp *) const override;

	bool        isDefinition() const override { return true; }
	void        getDefinitions(LocationSet &) const override;
	bool        definesLoc(Exp *) const override;

	// get how to access this lvalue
	virtual Exp *getLeft() const { return lhs; }  // Note: now only defined for Assignments, not all Statements
	void        setLeftFor(Exp *forExp, Exp *newExp) override { lhs = newExp; }

	// set the lhs to something new
	void        setLeft(Exp *e) { lhs = e; }

	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override { }

	void        simplifyAddr() override;

	// generate Constraints
	void        genConstraints(LocationSet &) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &) override;
};


/**
 * An ordinary assignment with left and right sides.
 */
class Assign : public Assignment {
	friend class XMLProgParser;

	Exp        *rhs = nullptr;
	Exp        *guard = nullptr;

public:
	            Assign(Exp *, Exp *, Exp * = nullptr);
	            Assign(Type *, Exp *, Exp *, Exp * = nullptr);
	// Default constructor, for XML parser
	            Assign() : Assignment(nullptr) { }
	            Assign(const Assign &);

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_ASSIGN; }
	bool        isFlagAssgn() const override;
	bool        isNullStatement() const override;
	bool        isFpush() const override;
	bool        isFpop() const override;

	// get how to replace this statement in a use
	virtual Exp *getRight() const { return rhs; }

	// set the rhs to something new
	void        setRight(Exp *e) { rhs = e; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	void        printCompact(std::ostream &, bool = false) const override;

	// Guard
	void        setGuard(Exp *g) { guard = g; }
	Exp        *getGuard() const { return guard; }
	bool        isGuarded() const { return !!guard; }

	bool        usesExp(Exp *) const override;
	bool        isDefinition() const override { return true; }

	bool        search(Exp *, Exp *&) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;

	// memory depth
	int         getMemDepth() const;

	// Generate code
	void        generateCode(HLLCode *, BasicBlock *, int) override;

	void        simplify() override;
	void        simplifyAddr() override;
	void        fixSuccessor() override;

	// generate Constraints
	void        genConstraints(LocationSet &) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &) override;

	// Range analysis
	//void        rangeAnalysis(std::list<Statement *> &) override;

#if 0
	// FIXME: I suspect that this was only used by adhoc TA, and can be deleted
	bool        match(const char *, std::map<std::string, Exp *> &);
#endif
};

/**
 * The below could almost be a RefExp.  But we could not at one stage #include
 * exp.h as part of statement.h; that's since changed so it is now possible,
 * and arguably desirable.  However, it's convenient to have these members
 * public.
 */
struct PhiInfo {
	Statement  *def = nullptr;  // The defining statement
	Exp        *e = nullptr;    // The expression for the thing being defined (never subscripted)
};

/**
 * PhiAssign is a subclass of Assignment, having a left hand side, and a
 * vector of PhiInfo with the references.
 *
 * \par Example
 * `m[1000] := phi{3 7 10}`\n
 * m[1000] is defined at statements 3, 7, and 10
 *
 * \par
 * `m[r28{3}+4] := phi{2 8}`\n
 * The memof is defined at 2 and 8, and the r28 is defined at 3.  The integers
 * are really pointers to statements, printed as the statement number for
 * compactness.
 *
 * \note Although the left hand side is nearly always redundant, it is
 * essential in at least one circumstance:  When finding locations used by
 * some statement, and the reference is to a CallStatement returning multiple
 * locations.
 */
class PhiAssign : public Assignment {
	friend class XMLProgParser;

public:
	typedef std::vector<PhiInfo> Definitions;
	typedef Definitions::iterator iterator;
	typedef Definitions::const_iterator const_iterator;

private:
	Definitions defVec;  // A vector of information about definitions

public:
	// Constructor, subexpression
	            PhiAssign(Exp *lhs) : Assignment(lhs) { }
	// Constructor, type and subexpression
	            PhiAssign(Type *ty, Exp *lhs) : Assignment(ty, lhs) { }

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_PHIASSIGN; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	void        printCompact(std::ostream &, bool = false) const override;

	bool        search(Exp *, Exp *&) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;

	void        simplify() override;

	// Generate constraints
	void        genConstraints(LocationSet &) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &) override;

//
// Phi specific functions
//

	// Get or put the statement at index idx
	const PhiInfo &getAt(int idx) const { return defVec[idx]; }
	void        putAt(int, Statement *, Exp *);
	void        simplifyRefs();
	const Definitions &getDefs() const { return defVec; }

	iterator    begin() { return defVec.begin(); }
	iterator    end()   { return defVec.end(); }
	iterator    erase(const_iterator it) { return defVec.erase(it); }

	void        convertToAssign(Exp *);

	void        enumerateParams(std::list<Exp *> &);
};

/**
 * An implicit assignment has only a left hand side.  It is a placeholder for
 * storing the types of parameters and globals.  That way, you can always find
 * the type of a subscripted variable by looking in its defining Assignment.
 */
class ImplicitAssign : public Assignment {
public:
	            ImplicitAssign(Exp *);
	            ImplicitAssign(Type *, Exp *);
	            ImplicitAssign(const ImplicitAssign &);

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_IMPASSIGN; }

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &) override;

	bool        search(Exp *, Exp *&) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;

	void        printCompact(std::ostream &, bool = false) const override;

	// Statement and Assignment functions
	void        simplify() override { }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;
};

/**
 * BoolAssign represents "setCC" type instructions, where some destination is
 * set (to 1 or 0) depending on the condition codes.  It has a condition Exp,
 * similar to the BranchStatement class.
 */
class BoolAssign: public Assignment {
	friend class XMLProgParser;

	BRANCH_TYPE jtCond = (BRANCH_TYPE)0;  // the condition for setting true
	Exp        *pCond = nullptr; // Exp representation of the high level
	                             // condition: e.g. r[8] == 5
	bool        bFloat = false;  // True if condition uses floating point CC
	int         size;            // The size of the dest

public:
	            BoolAssign(int size);
	           ~BoolAssign() override;

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_BOOLASSIGN; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	// Set and return the BRANCH_TYPE of this scond as well as whether the
	// floating point condition codes are used.
	void        setCondType(BRANCH_TYPE, bool = false);
	BRANCH_TYPE getCond() const { return jtCond; }
	bool        isFloat() const { return bFloat; }
	void        setFloat(bool b) { bFloat = b; }

	Exp        *getCondExpr() const;
	void        setCondExpr(Exp *);
	// As above, no delete (for subscripting)
	void        setCondExprND(Exp *e) { pCond = e; }

	int         getSize() const { return size; }  // Return the size of the assignment

	void        printCompact(std::ostream & = std::cout, bool = false) const override;

	// code generation
	void        generateCode(HLLCode *, BasicBlock *, int) override;

	void        simplify() override;

	// Statement functions
	bool        isDefinition() const override { return true; }
	void        getDefinitions(LocationSet &) const override;
	bool        usesExp(Exp *) const override;
	bool        search(Exp *, Exp *&) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;
	void        setLeftFromList(const std::list<Statement *> &);

	void        dfaTypeAnalysis(bool &) override;
};

/**
 * \note ImpRefStatement not yet used.
 *
 * An implicit reference has only an expression.  It holds the type
 * information that results from taking the address of a location.  Note that
 * dataflow can't decide which local variable (in the decompiled output) is
 * being taken, if there is more than one local variable sharing the same
 * memory address (separated then by type).
 */
class ImpRefStatement : public TypingStatement {
	Exp        *addressExp;  // The expression representing the address of the location referenced

public:
	// Constructor, subexpression
	            ImpRefStatement(Type *ty, Exp *a) : TypingStatement(ty), addressExp(a) { }
	Exp        *getAddressExp() const { return addressExp; }
	void        meetWith(Type *, bool &);

	// Virtuals
	Statement  *clone() const override;
	STMT_KIND   getKind() const override { return STMT_IMPREF; }
	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;
	bool        isDefinition() const override { return false; }
	bool        usesExp(Exp *) const override { return false; }
	bool        search(Exp *, Exp *&) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;
	void        generateCode(HLLCode *, BasicBlock *, int) override { }
	void        simplify() override;
	void        print(std::ostream &os, bool html = false) const override;
};

/**
 * GotoStatement has just one member variable, an expression representing the
 * jump's destination (an integer constant for direct jumps; an expression for
 * register jumps).  An instance of this class will never represent a return
 * or computed call as these are distinguished by the decoder and are
 * instantiated as ReturnStatements and CallStatements respectively.  This
 * class also represents unconditional jumps with a fixed offset (e.g BN, Ba
 * on SPARC).
 */
class GotoStatement: public Statement {
	friend class XMLProgParser;

protected:
	/**
	 * Destination of a jump or call.  This is the absolute destination
	 * for both static and dynamic CTIs.
	 */
	Exp        *pDest = nullptr;

	/**
	 * True if this is a CTI with a computed destination address.
	 *
	 * \note This should be removed, once CaseStatement and HLNwayCall are
	 * implemented properly.
	 */
	bool        m_isComputed = false;

public:
	            GotoStatement() = default;
	            GotoStatement(Exp *);
	            GotoStatement(ADDRESS);
	           ~GotoStatement() override;

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_GOTO; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	void        setDest(Exp *);
	void        setDest(ADDRESS);
	virtual Exp *getDest() const;

	ADDRESS     getFixedDest() const;

	void        adjustFixedDest(int);

	void        setIsComputed(bool = true);
	bool        isComputed() const;

	void        print(std::ostream & = std::cout, bool = false) const override;

	bool        search(Exp *, Exp *&) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;

	// code generation
	void        generateCode(HLLCode *, BasicBlock *, int) override;

	void        simplify() override;

	// Statement virtual functions
	bool        isDefinition() const override { return false; }
	bool        usesExp(Exp *) const override;
};

#if 0 // Cruft?
class JunctionStatement: public Statement {
public:
	Statement  *clone() const override { return new JunctionStatement(); }

	STMT_KIND   getKind() const override { return STMT_JUNCTION; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	// returns true if this statement defines anything
	bool        isDefinition() const override { return false; }

	bool        usesExp(Exp *e) const override { return false; }

	void        print(std::ostream &, bool = false) const override;

	// general search
	bool        search(Exp *search, Exp *&result) override { return false; }
	bool        searchAll(Exp *search, std::list<Exp *> &result) override { return false; }

	// general search and replace. Set cc true to change collectors as well. Return true if any change
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override { return false; }

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override { }

	// simpify internal expressions
	void        simplify() override { }

	//void        rangeAnalysis(std::list<Statement *> &) override;
	bool        isLoopJunction() const;
};
#endif

/**
 * BranchStatement has a condition Exp in addition to the destination of the
 * jump.
 */
class BranchStatement: public GotoStatement {
	friend class XMLProgParser;

	BRANCH_TYPE jtCond = (BRANCH_TYPE)0;  // The condition for jumping
	Exp        *pCond = nullptr; // The Exp representation of the high level condition: e.g., r[8] == 5
	bool        bFloat = false;  // True if uses floating point CC
	// jtCond seems to be mainly needed for the Pentium weirdness.
	// Perhaps bFloat, jtCond, and size could one day be merged into a type
	int         size = 0;        // Size of the operands, in bits
	//RangeMap    ranges2;         // ranges for the not taken edge

public:
	            BranchStatement() = default;
	            BranchStatement(Exp *);
	            BranchStatement(ADDRESS);
	           ~BranchStatement() override;

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_BRANCH; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	// Set and return the BRANCH_TYPE of this jcond as well as whether the
	// floating point condition codes are used.
	void        setCondType(BRANCH_TYPE, bool = false);
	BRANCH_TYPE getCond() const { return jtCond; }
	bool        isFloat() const { return bFloat; }
	void        setFloat(bool b) { bFloat = b; }

	Exp        *getCondExpr() const;
	void        setCondExpr(Exp *);
	// As above, no delete (for subscripting)
	void        setCondExprND(Exp *e) { pCond = e; }

	BasicBlock *getFallBB() const;
	BasicBlock *getTakenBB() const;
	void        setFallBB(BasicBlock *);
	void        setTakenBB(BasicBlock *);

	void        print(std::ostream & = std::cout, bool = false) const override;

	bool        search(Exp *, Exp *&) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;

	// code generation
	void        generateCode(HLLCode *, BasicBlock *, int) override;

	// dataflow analysis
	bool        usesExp(Exp *) const override;

	// Range analysis
	//void        rangeAnalysis(std::list<Statement *> &) override;
	//RangeMap   &getRangesForOutEdgeTo(BasicBlock *);
	//RangeMap   &getRanges2Ref() { return ranges2; }
	//void        setRanges2(RangeMap &r) { ranges2 = r; }
	//void        limitOutputWithCondition(RangeMap &, Exp *);

	void        simplify() override;

	// Generate constraints
	void        genConstraints(LocationSet &) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &) override;
};

struct SWITCH_INFO {
	Exp        *pSwitchVar;     // Ptr to Exp repres switch var, e.g. v[7]
	char        chForm;         // Switch form: 'A', 'O', 'R', 'H', or 'F' etc
	int         iLower;         // Lower bound of the switch variable
	int         iUpper;         // Upper bound for the switch variable
	ADDRESS     uTable;         // Native address of the table, or ptr to array of values for form F
	int         iNumTable;      // Number of entries in the table (form H only)
	int         iOffset;        // Distance from jump to table (form R only)
	//int         delta;          // Host address - Native address
};

/**
 * CaseStatement is derived from GotoStatement.  In addition to the
 * destination of the jump, it has a switch variable Exp.
 */
class CaseStatement: public GotoStatement {
	friend class XMLProgParser;

	/**
	 * \brief Struct with info about the switch.
	 */
	SWITCH_INFO *pSwitchInfo = nullptr;

public:
	            CaseStatement() = default;
	            CaseStatement(Exp *);
	           ~CaseStatement() override;

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_CASE; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	SWITCH_INFO *getSwitchInfo() const;
	void        setSwitchInfo(SWITCH_INFO *);

	void        print(std::ostream & = std::cout, bool = false) const override;

	bool        searchAndReplace(Exp *, Exp *, bool = false) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;

	// code generation
	void        generateCode(HLLCode *, BasicBlock *, int) override;

	// dataflow analysis
	bool        usesExp(Exp *) const override;

	void        simplify() override;
};

/**
 * Represents a high level call.  Information about parameters and the like
 * are stored here.
 */
class CallStatement: public GotoStatement {
	friend class XMLProgParser;

	/**
	 * True if call is effectively followed by a return.
	 */
	bool        returnAfterCall = false;

	/**
	 * The list of arguments passed by this call, actually a list of
	 * Assign statements (location := expr).
	 */
	StatementList arguments;

	/**
	 * The list of defines for this call, a list of ImplicitAssigns (used
	 * to be called returns).  Essentially a localised copy of the
	 * modifies of the callee, so the callee could be deleted.  Stores
	 * types and locations.  Note that not necessarily all of the defines
	 * end up being declared as results.
	 */
	StatementList defines;

	/**
	 * Destination of call.  In the case of an analysed indirect call,
	 * this will be ONE target's return statement.  For an unanalysed
	 * indirect call, or a call whose callee is not yet sufficiently
	 * decompiled due to recursion, this will be null.
	 */
	Proc       *procDest = nullptr;

	/**
	 * The signature for this call.
	 * \note This used to be stored in the Proc, but this does not make
	 * sense when the proc happens to have varargs.
	 */
	Signature  *signature = nullptr;

	/**
	 * A UseCollector object to collect the live variables at this call.
	 * Used as part of the calculation of results.
	 */
	UseCollector useCol;

	/**
	 * A DefCollector object to collect the reaching definitions; used for
	 * bypassAndPropagate/localiseExp, etc.; also the basis for arguments
	 * if this is an unanalysed indirect call.
	 */
	DefCollector defCol;

	/**
	 * Pointer to the callee ReturnStatement.  If the callee is
	 * unanalysed, this will be a special ReturnStatement with
	 * ImplicitAssigns.  Callee could be unanalysed because of an
	 * unanalysed indirect call, or a "recursion break".
	 */
	ReturnStatement *calleeReturn = nullptr;

public:
	            CallStatement() = default;
	            CallStatement(Exp *);
	            CallStatement(ADDRESS);

	void        setNumber(int) override;
	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_CALL; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	void        setArguments(StatementList &);
	// Set implicit arguments: so far, for testing only:
	//void        setImpArguments(std::vector<Exp *> &arguments);
	//void        setReturns(std::vector<Exp *> &returns);// Set call's return locs
	void        setSigArguments();
	StatementList &getArguments() { return arguments; }  // Return call's arguments
	void        updateArguments();
	int         findDefine(Exp *);
	void        removeDefine(Exp *);
	void        addDefine(ImplicitAssign *);
	//void        addReturn(Exp *e, Type *ty = nullptr);
	void        updateDefines();
	StatementList *calcResults();
	ReturnStatement *getCalleeReturn() const { return calleeReturn; }
	void        setCalleeReturn(ReturnStatement *ret) { calleeReturn = ret; }
	bool        isChildless() const;
	Exp        *getProven(Exp *) const;
	Signature  *getSignature() const { return signature; }
	Exp        *localiseExp(Exp *);
	void        localiseComp(Exp *);
	Exp        *bypassRef(RefExp *, bool &);
	void        clearUseCollector() { useCol.clear(); }
	Exp        *findDefFor(Exp *) const;
	Exp        *getArgumentExp(int) const;
	void        setArgumentExp(int, Exp *);
	void        setNumArguments(int);
	int         getNumArguments() const;
	void        removeArgument(int);
	Type       *getArgumentType(int) const;
	void        eliminateDuplicateArgs();

	// Range analysis
	//void        rangeAnalysis(std::list<Statement *> &) override;

	void        print(std::ostream & = std::cout, bool = false) const override;

	bool        search(Exp *, Exp *&) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;

	void        setReturnAfterCall(bool);
	bool        isReturnAfterCall() const;

	// Set and return the list of Exps that occur *after* the call (the
	// list of exps in the RTL occur before the call). Useful for odd patterns.
	void        setPostCallExpList(std::list<Exp *> *le);
	std::list<Exp *> *getPostCallExpList();

	void        setDestProc(Proc *);
	Proc       *getDestProc() const;

	// Generate constraints
	void        genConstraints(LocationSet &) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &) override;

	// code generation
	void        generateCode(HLLCode *, BasicBlock *, int) override;

	// dataflow analysis
	bool        usesExp(Exp *) const override;

	// dataflow related functions
	bool        isDefinition() const override;
	void        getDefinitions(LocationSet &) const override;

	bool        definesLoc(Exp *) const override;
	void        setLeftFor(Exp *, Exp *) override;

	void        simplify() override;

	//void        setIgnoreReturnLoc(bool b);

	void        decompile();

	// Insert actual arguments to match formal parameters
	//void        insertArguments(StatementSet &rs);

	Type       *getTypeFor(Exp *) const override;
	void        setTypeFor(Exp *, Type *) override;
	DefCollector *getDefCollector() { return &defCol; }         // Return pointer to the def collector object
	UseCollector *getUseCollector() { return &useCol; }         // Return pointer to the use collector object
	void        useBeforeDefine(Exp *x) { useCol.insert(x); }   // Add x to the UseCollector for this call
	void        removeLiveness(Exp *e) { useCol.remove(e); }    // Remove e from the UseCollector
	void        removeAllLive() { useCol.clear(); }             // Remove all livenesses
	//Exp        *fromCalleeContext(Exp *e);          // Convert e from callee to caller (this) context
	StatementList &getDefines() { return defines; } // Get list of locations defined by this call
	bool        ellipsisProcessing(Prog *);
	bool        convertToDirect();
	void        useColFromSsaForm(Statement *s) { useCol.fromSSAform(proc, s); }
private:
	// Private helper functions for the above
	void        addSigParam(Type *, bool);
	Assign     *makeArgAssign(Type *, Exp *);

protected:
	void        appendArgument(Assignment *as) { arguments.append(as); }
};

/**
 * Represents an ordinary high level return.
 */
class ReturnStatement : public Statement {
	friend class XMLProgParser;

protected:
	/**
	 * \brief A DefCollector object to collect the reaching definitions.
	 *
	 * The progression of return information is as follows:
	 *
	 * First, reaching definitions are collected in the DefCollector col.
	 * These are not sorted or filtered.
	 *
	 * Second, some of those definitions make it to the modifieds list,
	 * which is sorted and filtered.  These are the locations that are
	 * modified by the enclosing procedure.  As locations are proved to be
	 * preserved (with NO modification, not even sp = sp+4), they are
	 * removed from this list.  Defines in calls to the enclosing
	 * procedure are based on this list.
	 *
	 * Third, the modifications are initially copied to the returns list
	 * (also sorted and filtered, but the returns have RHS where the
	 * modifieds don't).  Locations not live at any caller are removed
	 * from the returns, but not from the modifieds.
	 */
	DefCollector col;

	/**
	 * \brief A list of assignments that represents the locations modified
	 * by the enclosing procedure.  These assignments have no RHS?
	 *
	 * These transmit type information to callers.
	 *
	 * Note that these include preserved locations early on (?)
	 */
	StatementList modifieds;

	/**
	 * \brief A list of assignments of locations to expressions.
	 *
	 * Initially definitions reaching the exit less preserveds; later has
	 * locations unused by any callers removed.  A list is used to
	 * facilitate ordering.  (A set would be ideal, but the ordering
	 * depends at runtime on the signature.)
	 */
	StatementList returns;

public:
	typedef StatementList::iterator iterator;
	typedef StatementList::const_iterator const_iterator;
	const_iterator begin() const    { return returns.begin(); }
	const_iterator end() const      { return returns.end(); }
	iterator    begin()             { return returns.begin(); }
	iterator    end()               { return returns.end(); }
	iterator    erase(const_iterator it) { return returns.erase(it); }
	StatementList &getModifieds()   { return modifieds; }
	StatementList &getReturns()     { return returns; }
	unsigned    getNumReturns() const { return returns.size(); }
	void        updateModifieds();
	void        updateReturns();

	void        print(std::ostream & = std::cout, bool = false) const override;

	bool        search(Exp *, Exp *&) override;
	bool        searchAndReplace(Exp *, Exp *, bool = false) override;
	bool        searchAll(Exp *, std::list<Exp *> &) override;

	bool        usesExp(Exp *) const override;

	void        getDefinitions(LocationSet &) const override;

	void        removeModified(Exp *);
	void        removeReturn(Exp *);
	void        addReturn(Assignment *);

	Type       *getTypeFor(Exp *) const override;
	void        setTypeFor(Exp *, Type *) override;

	void        simplify() override;

	bool        isDefinition() const override { return true; }

	Statement  *clone() const override;

	STMT_KIND   getKind() const override { return STMT_RET; }

	bool        accept(StmtVisitor &) override;
	bool        accept(StmtExpVisitor &) override;
	bool        accept(StmtModifier &) override;
	bool        accept(StmtPartModifier &) override;

	bool        definesLoc(Exp *) const override;

	// code generation
	void        generateCode(HLLCode *, BasicBlock *, int) override;

	//Exp        *getReturnExp(int n) { return returns[n]; }
	//void        setReturnExp(int n, Exp *e) { returns[n] = e; }
	//void        setSigArguments();  // Set returns based on signature
	DefCollector *getCollector() { return &col; }  // Return pointer to the collector object

	// Find definition for e (in the collector)
	Exp        *findDefFor(Exp *e) const { return col.findDefFor(e); }

	void        dfaTypeAnalysis(bool &) override;

	// Temporary hack (not neccesary anymore)
	//void        specialProcessing();
};

#endif
