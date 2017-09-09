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

#include <iostream>     // For std::cout, std::dec
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

/*==============================================================================
 * Kinds of Statements, or high-level register transfer lists.
 * changing the order of these will result in save files not working - trent
 *============================================================================*/
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

/*==============================================================================
 * BRANCH_TYPE: These values indicate what kind of conditional jump or
 * conditonal assign is being performed.
 * Changing the order of these will result in save files not working - trent
 *============================================================================*/
enum BRANCH_TYPE {
	BRANCH_JE = 0,          // Jump if equals
	BRANCH_JNE,             // Jump if not equals
	BRANCH_JSL,             // Jump if signed less
	BRANCH_JSLE,            // Jump if signed less or equal
	BRANCH_JSGE,            // Jump if signed greater or equal
	BRANCH_JSG,             // Jump if signed greater
	BRANCH_JUL,             // Jump if unsigned less
	BRANCH_JULE,            // Jump if unsigned less or equal
	BRANCH_JUGE,            // Jump if unsigned greater or equal
	BRANCH_JUG,             // Jump if unsigned greater
	BRANCH_JMI,             // Jump if result is minus
	BRANCH_JPOS,            // Jump if result is positive
	BRANCH_JOF,             // Jump if overflow
	BRANCH_JNOF,            // Jump if no overflow
	BRANCH_JPAR             // Jump if parity even (Intel only)
};

//  //  //  //  //  //  //  //  //  //  //  //  //  //
//
//  A b s t r a c t   C l a s s   S t a t e m e n t //
//
//  //  //  //  //  //  //  //  //  //  //  //  //  //

/* Statements define values that are used in expressions.
 * They are akin to "definition" in the Dragon Book.
 */
class Statement {
protected:
	        BasicBlock *pbb = nullptr;     // contains a pointer to the enclosing BB
	        UserProc   *proc = nullptr;    // procedure containing this statement
	        int         number = 0;        // Statement number for printing
#if USE_DOMINANCE_NUMS
	        int         dominanceNum;      // Like a statement number, but has dominance properties
public:
	        int         getDomNumber() { return dominanceNum; }
	        void        setDomNumber(int dn) { dominanceNum = dn; }
protected:
#endif
	        STMT_KIND   kind;              // Statement kind (e.g. STMT_BRANCH)
	        Statement  *parent = nullptr;  // The statement that contains this one
	        RangeMap    ranges;            // overestimation of ranges of locations
	        RangeMap    savedInputRanges;  // saved overestimation of ranges of locations

	        unsigned int lexBegin, lexEnd;

public:

	                    Statement() { }
	virtual            ~Statement() { }

	// get/set the enclosing BB, etc
	        BasicBlock *getBB() { return pbb; }
	        void        setBB(BasicBlock *bb) { pbb = bb; }

	        //bool        operator ==(Statement &o);
	// Get and set *enclosing* proc (not destination proc)
	        void        setProc(UserProc *p);
	        UserProc   *getProc() { return proc; }

	        int         getNumber() { return number; }
	virtual void        setNumber(int num) { number = num; }  // Overridden for calls (and maybe later returns)

	        STMT_KIND   getKind() { return kind; }
	        void        setKind(STMT_KIND k) { kind = k; }

	        void        setParent(Statement *par) { parent = par; }
	        Statement  *getParent() { return parent; }

	        RangeMap   &getRanges() { return ranges; }
	        void        clearRanges() { ranges.clear(); }

	virtual Statement  *clone() = 0;  // Make copy of self

	// Accept a visitor (of various kinds) to this Statement. Return true to continue visiting
	virtual bool        accept(StmtVisitor *visitor) = 0;
	virtual bool        accept(StmtExpVisitor *visitor) = 0;
	virtual bool        accept(StmtModifier *visitor) = 0;
	virtual bool        accept(StmtPartModifier *visitor) = 0;

	        void        setLexBegin(unsigned int n) { lexBegin = n; }
	        void        setLexEnd(unsigned int n) { lexEnd = n; }
	        unsigned    int getLexBegin() { return lexBegin; }
	        unsigned    int getLexEnd() { return lexEnd; }
	        Exp        *getExpAtLex(unsigned int begin, unsigned int end);


	// returns true if this statement defines anything
	virtual bool        isDefinition() = 0;

	// true if is a null statement
	        bool        isNullStatement();

	virtual bool        isTyping() { return false; }  // Return true if a TypingStatement
	// true if this statement is a standard assign
	        bool        isAssign() { return kind == STMT_ASSIGN; }
	// true if this statement is a any kind of assignment
	        bool        isAssignment() { return kind == STMT_ASSIGN || kind == STMT_PHIASSIGN || kind == STMT_IMPASSIGN || kind == STMT_BOOLASSIGN; }
	// true if this statement is a phi assignment
	        bool        isPhi() { return kind == STMT_PHIASSIGN; }
	// true if this statement is an implicit assignment
	        bool        isImplicit() { return kind == STMT_IMPASSIGN; }
	// true if this statment is a flags assignment
	        bool        isFlagAssgn();
	// true of this statement is an implicit reference
	        bool        isImpRef() { return kind == STMT_IMPREF; }

	virtual bool        isGoto() { return kind == STMT_GOTO; }
	virtual bool        isBranch() { return kind == STMT_BRANCH; }

	// true if this statement is a junction
	        bool        isJunction() { return kind == STMT_JUNCTION; }

	// true if this statement is a call
	        bool        isCall() { return kind == STMT_CALL; }

	// true if this statement is a BoolAssign
	        bool        isBool() { return kind == STMT_BOOLASSIGN; }

	// true if this statement is a ReturnStatement
	        bool        isReturn() { return kind == STMT_RET; }

	// true if this statement is a decoded ICT.
	// NOTE: for now, it only represents decoded indirect jump instructions
	        bool        isHL_ICT() { return kind == STMT_CASE; }

	        bool        isCase() { return kind == STMT_CASE; }

	// true if this is a fpush/fpop
	        bool        isFpush();
	        bool        isFpop();

	// returns a set of locations defined by this statement
	// Classes with no definitions (e.g. GotoStatement and children) don't override this
	virtual void        getDefinitions(LocationSet &def) { }

	// set the left for forExp to newExp
	virtual void        setLeftFor(Exp *forExp, Exp *newExp) { assert(0); }
	virtual bool        definesLoc(Exp *loc) { return false; }  // True if this Statement defines loc

	// returns true if this statement uses the given expression
	virtual bool        usesExp(Exp *e) = 0;

	// statements should be printable (for debugging)
	virtual void        print(std::ostream &os, bool html = false) = 0;
	        void        printAsUse(std::ostream &os)   { os << std::dec << number; }
	        void        printAsUseBy(std::ostream &os) { os << std::dec << number; }
	        void        printNum(std::ostream &os)     { os << std::dec << number; }
	        std::string prints();  // For logging, was also for debugging

	// general search
	virtual bool        search(Exp *search, Exp *&result) = 0;
	virtual bool        searchAll(Exp *search, std::list<Exp *> &result) = 0;

	// general search and replace. Set cc true to change collectors as well. Return true if any change
	virtual bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) = 0;

	// True if can propagate to expression e in this Statement.
	static  bool        canPropagateToExp(Exp *e);
	// Propagate to this statement. Return true if a change
	// destCounts is a map that indicates how may times a statement's definition is used
	// dnp is a StatementSet with statements that should not be propagated
	// Set convert if an indirect call is changed to direct (otherwise, no change)
	// Set force to true to propagate even memofs (for switch analysis)
	        bool        propagateTo(bool &convert, std::map<Exp *, int, lessExpStar> *destCounts = nullptr, LocationSet *usedByDomPhi = nullptr, bool force = false);
	        bool        propagateFlagsTo();

	// code generation
	virtual void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) = 0;

	// simpify internal expressions
	virtual void        simplify() = 0;

	// simplify internal address expressions (a[m[x]] -> x) etc
	// Only Assignments override at present
	virtual void        simplifyAddr() { }

	// map registers and temporaries to local variables
	        void        mapRegistersToLocals();

	// The last part of the fromSSA logic: replace subscripted locations with suitable local variables
	        void        replaceSubscriptsWithLocals();

	// insert casts where needed, since fromSSA will erase type information
	        void        insertCasts();

	// fixSuccessor
	// Only Assign overrides at present
	virtual void        fixSuccessor() { }

	// Generate constraints (for constraint based type analysis)
	virtual void        genConstraints(LocationSet &cons) { }

	// Data flow based type analysis
	virtual void        dfaTypeAnalysis(bool &ch) { }  // Use the type information in this Statement
	        Type       *meetWithFor(Type *ty, Exp *e, bool &ch);// Meet the type associated with e with ty

	// Range analysis
protected:
	        void        updateRanges(RangeMap &output, std::list<Statement *> &execution_paths, bool notTaken = false);
public:
	        RangeMap   &getSavedInputRanges() { return savedInputRanges; }
	        RangeMap    getInputRanges();
	virtual void        rangeAnalysis(std::list<Statement *> &execution_paths);

	// helper functions
	        bool        isFirstStatementInBB();
	        bool        isLastStatementInBB();
	        Statement  *getNextStatementInBB();
	        Statement  *getPreviousStatementInBB();


//  //  //  //  //  //  //  //  //  //
//                                  //
//  Statement visitation functions  //
//                                  //
//  //  //  //  //  //  //  //  //  //

	// Adds (inserts) all locations (registers or memory etc) used by this statement
	// Set cc to true to count the uses in collectors
	        void        addUsedLocs(LocationSet &used, bool cc = false, bool memOnly = false);
	// Special version of the above for finding used locations. Returns true if defineAll was found
	        bool        addUsedLocals(LocationSet &used);
	// Bypass calls for references in this statement
	        void        bypass();


	// replaces a use in this statement with an expression from an ordinary assignment
	// Internal use only
	        bool        replaceRef(Exp *e, Assign *def, bool &convert);

	// Find all constants in this statement
	        void        findConstants(std::list<Const *> &lc);

	// Set or clear the constant subscripts (using a visitor)
	        int         setConscripts(int n);
	        void        clearConscripts();

	// Strip all size casts
	        void        stripSizes();

	// For all expressions in this Statement, replace all e with e{def}
	        void        subscriptVar(Exp *e, Statement *def /*, Cfg *cfg */);

	// Cast the constant num to type ty. If a change was made, return true
	        bool        castConst(int num, Type *ty);

	// Map expressions to locals
	        void        dfaMapLocals();

	// End Statement visitation functions


	// Get the type for the definition, if any, for expression e in this statement
	// Overridden only by Assignment and CallStatement, and ReturnStatement.
	virtual Type       *getTypeFor(Exp *e) { return nullptr; }
	// Set the type for the definition of e in this Statement
	virtual void        setTypeFor(Exp *e, Type *ty) { assert(0); }

	//virtual Type       *getType() { return nullptr; }    // Assignment, ReturnStatement and
	//virtual void        setType(Type *t) { assert(0); }  // CallStatement override

	        bool        doPropagateTo(Exp *e, Assign *def, bool &convert);
	        bool        calcMayAlias(Exp *e1, Exp *e2, int size);
	        bool        mayAlias(Exp *e1, Exp *e2, int size);

	friend class XMLProgParser;
};

// Print the Statement pointed to by p
std::ostream &operator <<(std::ostream &os, Statement *p);

/*==============================================================================
 * TypingStatement is an abstract subclass of Statement. It has a type, representing the type of a reference or an
 * assignment
 *============================================================================*/
class TypingStatement : public Statement {
protected:
	Type       *type;  // The type for this assignment or reference
public:
	            TypingStatement(Type *ty);  // Constructor

	// Get and set the type.
	Type       *getType() /* override */ { return type; }
	void        setType(Type *ty) /* override */ { type = ty; }

	bool        isTyping() override { return true; }
};

/*==========================================================================
 * Assignment is an abstract subclass of TypingStatement, holding a location
 *==========================================================================*/
class Assignment : public TypingStatement {
protected:
	Exp        *lhs;  // The left hand side
public:
	// Constructor, subexpression
	            Assignment(Exp *lhs);
	// Constructor, type, and subexpression
	            Assignment(Type *ty, Exp *lhs);
	// Destructor
	virtual    ~Assignment();

	// Clone
	Statement  *clone() override = 0;

	// We also want operator < for assignments. For example, we want ReturnStatement to contain a set of (pointers
	// to) Assignments, so we can automatically make sure that existing assignments are not duplicated
	// Assume that we won't want sets of assignments differing by anything other than LHSs
	bool        operator <(const Assignment &o) { return lhs < o.lhs; }

	// Accept a visitor to this Statement
	bool        accept(StmtVisitor *visitor) override = 0;
	bool        accept(StmtExpVisitor *visitor) override = 0;
	bool        accept(StmtModifier *visitor) override = 0;
	bool        accept(StmtPartModifier *visitor) override = 0;

	void        print(std::ostream &os, bool html = false) override;
	virtual void printCompact(std::ostream &os, bool html = false) = 0;  // Without statement number

	Type       *getTypeFor(Exp *e) override;            // Get the type for this assignment. It should define e
	void        setTypeFor(Exp *e, Type *ty) override;  // Set the type for this assignment. It should define e

	bool        usesExp(Exp *e) override;  // PhiAssign and ImplicitAssign don't override

	bool        isDefinition() override { return true; }
	void        getDefinitions(LocationSet &defs) override;
	bool        definesLoc(Exp *loc) override;  // True if this Statement defines loc

	// get how to access this lvalue
	virtual Exp *getLeft() { return lhs; }  // Note: now only defined for Assignments, not all Statements
	void        setLeftFor(Exp *forExp, Exp *newExp) override { lhs = newExp; }

	// set the lhs to something new
	void        setLeft(Exp *e) { lhs = e; }

	// memory depth
	int         getMemDepth();

	// general search
	bool        search(Exp *search, Exp *&result) override = 0;
	bool        searchAll(Exp *search, std::list<Exp *> &result) override = 0;

	// general search and replace
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override = 0;

	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override { }

	// simpify internal expressions
	void        simplify() override = 0;

	// simplify address expressions
	void        simplifyAddr() override;

	// generate Constraints
	void        genConstraints(LocationSet &cons) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &ch) override;

	friend class XMLProgParser;
};


// Assign: an ordinary assignment with left and right sides
class Assign : public Assignment {
	Exp        *rhs = nullptr;
	Exp        *guard = nullptr;

public:
	// Constructor, subexpressions
	            Assign(Exp *lhs, Exp *rhs, Exp *guard = nullptr);
	// Constructor, type and subexpressions
	            Assign(Type *ty, Exp *lhs, Exp *rhs, Exp *guard = nullptr);
	// Default constructor, for XML parser
	            Assign() : Assignment(nullptr) { }
	// Copy constructor
	            Assign(Assign &o);
	// Destructor
	           ~Assign() { }

	// Clone
	Statement  *clone() override;

	// get how to replace this statement in a use
	virtual Exp *getRight() { return rhs; }
	Exp       *&getRightRef() { return rhs; }

	// set the rhs to something new
	void        setRight(Exp *e) { rhs = e; }



	// Accept a visitor to this Statement
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	void        printCompact(std::ostream &os, bool html = false) override;  // Without statement number

	// Guard
	void        setGuard(Exp *g) { guard = g; }
	Exp        *getGuard() { return guard; }
	bool        isGuarded() { return !!guard; }

	bool        usesExp(Exp *e) override;
	bool        isDefinition() override { return true; }

	// general search
	bool        search(Exp *search, Exp *&result) override;
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// general search and replace
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	// memory depth
	int         getMemDepth();

	// Generate code
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override;

	// simpify internal expressions
	void        simplify() override;

	// simplify address expressions
	void        simplifyAddr() override;

	// fixSuccessor (succ(r2) -> r3)
	void        fixSuccessor() override;

	// generate Constraints
	void        genConstraints(LocationSet &cons) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &ch) override;

	// Range analysis
	void        rangeAnalysis(std::list<Statement *> &execution_paths) override;

	// FIXME: I suspect that this was only used by adhoc TA, and can be deleted
	bool        match(const char *pattern, std::map<std::string, Exp *> &bindings);

	friend class XMLProgParser;
};

/*==============================================================================
 * PhiAssign is a subclass of Assignment, having a left hand side, and a StatementVec with the references.
 * Example:
 * m[1000] := phi{3 7 10}   m[1000] is defined at statements 3, 7, and 10
 * m[r28{3}+4] := phi{2 8}  the memof is defined at 2 and 8, and
 * the r28 is defined at 3. The integers are really pointers to statements,
 * printed as the statement number for compactness
 * NOTE: Although the left hand side is nearly always redundant, it is essential in at least one circumstance: when
 * finding locations used by some statement, and the reference is to a CallStatement returning multiple locations.
 *============================================================================*/
// The below could almost be a RefExp. But we could not at one stage #include exp.h as part of statement.h; that's since
// changed so it is now possible, and arguably desirable.  However, it's convenient to have these members public
struct PhiInfo {
	// A default constructor is required because CFG changes (?) can cause access to elements of the vector that
	// are beyond the current end, creating gaps which have to be initialised to zeroes so that they can be skipped
	            PhiInfo() { }
	Statement  *def = nullptr;  // The defining statement
	Exp        *e = nullptr;    // The expression for the thing being defined (never subscripted)
};
class PhiAssign : public Assignment {
public:
	typedef std::vector<PhiInfo> Definitions;
	typedef Definitions::iterator iterator;
private:
	Definitions defVec;  // A vector of information about definitions
public:
	// Constructor, subexpression
	            PhiAssign(Exp *lhs) : Assignment(lhs) { kind = STMT_PHIASSIGN; }
	// Constructor, type and subexpression
	            PhiAssign(Type *ty, Exp *lhs) : Assignment(ty, lhs) { kind = STMT_PHIASSIGN; }
	// Copy constructor (not currently used or implemented)
	            PhiAssign(Assign &o);
	// Destructor
	virtual    ~PhiAssign() { }

	// Clone
	Statement  *clone() override;

	// get how to replace this statement in a use
	virtual Exp *getRight() { return nullptr; }

	// Accept a visitor to this Statement
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	void        printCompact(std::ostream &os, bool html = false) override;

	// general search
	bool        search(Exp *search, Exp *&result) override;
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// general search and replace
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	// simplify all the uses/defs in this Statement
	void        simplify() override;

	// Generate constraints
	void        genConstraints(LocationSet &cons) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &ch) override;

//
// Phi specific functions
//

	// Get or put the statement at index idx
	Statement  *getStmtAt(int idx) { return defVec[idx].def; }
	PhiInfo    &getAt(int idx) { return defVec[idx]; }
	void        putAt(int idx, Statement *d, Exp *e);
	void        simplifyRefs();
	virtual int getNumDefs() const { return defVec.size(); }
	Definitions &getDefs() { return defVec; }
	// A hack. Check MVE
	bool        hasGlobalFuncParam();

	iterator    begin() { return defVec.begin(); }
	iterator    end()   { return defVec.end(); }
	iterator    erase(iterator it) { return defVec.erase(it); }

	// Convert this phi assignment to an ordinary assignment
	void        convertToAssign(Exp *rhs);

	// Generate a list of references for the parameters
	void        enumerateParams(std::list<Exp *> &le);

protected:
	friend class XMLProgParser;
};

// An implicit assignment has only a left hand side. It is a placeholder for storing the types of parameters and
// globals.  That way, you can always find the type of a subscripted variable by looking in its defining Assignment
class ImplicitAssign : public Assignment {
public:
	// Constructor, subexpression
	            ImplicitAssign(Exp *lhs);
	// Constructor, type, and subexpression
	            ImplicitAssign(Type *ty, Exp *lhs);
	// Copy constructor
	            ImplicitAssign(ImplicitAssign &o);
	// Destructor
	virtual    ~ImplicitAssign();

	// Clone
	Statement  *clone() override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &ch) override;

	// general search
	bool        search(Exp *search, Exp *&result) override;
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// general search and replace
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	void        printCompact(std::ostream &os, bool html = false) override;

	// Statement and Assignment functions
	virtual Exp *getRight() { return nullptr; }
	void        simplify() override { }

	// Visitation
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;
};

/*==============================================================================
 * BoolAssign represents "setCC" type instructions, where some destination is set (to 1 or 0) depending on the
 * condition codes. It has a condition Exp, similar to the BranchStatement class.
 * *==========================================================================*/
class BoolAssign: public Assignment {
	BRANCH_TYPE jtCond = (BRANCH_TYPE)0;  // the condition for setting true
	Exp        *pCond = nullptr; // Exp representation of the high level
	                             // condition: e.g. r[8] == 5
	bool        bFloat = false;  // True if condition uses floating point CC
	int         size;            // The size of the dest
public:
	            BoolAssign(int size);
	virtual    ~BoolAssign();

	// Make a deep copy, and make the copy a derived object if needed.
	Statement  *clone() override;

	// Accept a visitor to this Statement
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	// Set and return the BRANCH_TYPE of this scond as well as whether the
	// floating point condition codes are used.
	void        setCondType(BRANCH_TYPE cond, bool usesFloat = false);
	BRANCH_TYPE getCond() { return jtCond; }
	bool        isFloat() { return bFloat; }
	void        setFloat(bool b) { bFloat = b; }

	// Set and return the Exp representing the HL condition
	Exp        *getCondExpr();
	void        setCondExpr(Exp *pss);
	// As above, no delete (for subscripting)
	void        setCondExprND(Exp *e) { pCond = e; }

	int         getSize() const { return size; }  // Return the size of the assignment
	void        makeSigned();

	void        printCompact(std::ostream &os = std::cout, bool html = false) override;

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override;

	// simplify all the uses/defs in this Statement
	void        simplify() override;

	// Statement functions
	bool        isDefinition() override { return true; }
	void        getDefinitions(LocationSet &def) override;
	virtual Exp *getRight() { return getCondExpr(); }
	bool        usesExp(Exp *e) override;
	bool        search(Exp *search, Exp *&result) override;
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;
	// a hack for the SETS macro
	void        setLeftFromList(std::list<Statement *> *stmts);

	void        dfaTypeAnalysis(bool &ch) override;

	friend class XMLProgParser;
};

// An implicit reference has only an expression. It holds the type information that results from taking the address
// of a location. Note that dataflow can't decide which local variable (in the decompiled output) is being taken,
// if there is more than one local variable sharing the same memory address (separated then by type).
class ImpRefStatement : public TypingStatement {
	Exp        *addressExp;  // The expression representing the address of the location referenced
public:
	// Constructor, subexpression
	            ImpRefStatement(Type *ty, Exp *a) : TypingStatement(ty), addressExp(a) { kind = STMT_IMPREF; }
	Exp        *getAddressExp() { return addressExp; }
	Type       *getType() /* override */ { return type; }
	void        meetWith(Type *ty, bool &ch);  // Meet the internal type with ty. Set ch if a change

	// Virtuals
	Statement  *clone() override;
	bool        accept(StmtVisitor *) override;
	bool        accept(StmtExpVisitor *) override;
	bool        accept(StmtModifier *) override;
	bool        accept(StmtPartModifier *) override;
	bool        isDefinition() override { return false; }
	bool        usesExp(Exp *) override { return false; }
	bool        search(Exp *, Exp *&) override;
	bool        searchAll(Exp *, std::list<Exp *, std::allocator<Exp *> > &) override;
	bool        searchAndReplace(Exp *, Exp *, bool cc = false) override;
	void        generateCode(HLLCode *, BasicBlock *, int) override { }
	void        simplify() override;
	void        print(std::ostream &os, bool html = false) override;
};


/*=============================================================================
 * GotoStatement has just one member variable, an expression representing the
 * jump's destination (an integer constant for direct jumps; an expression
 * for register jumps). An instance of this class will never represent a
 * return or computed call as these are distinguised by the decoder and are
 * instantiated as CallStatements and ReturnStatements respecitvely.
 * This class also represents unconditional jumps with a fixed offset
 * (e.g BN, Ba on SPARC).
 *===========================================================================*/
class GotoStatement: public Statement {
protected:
	Exp        *pDest = nullptr;       // Destination of a jump or call. This is the absolute destination for both static
	                                   // and dynamic CTIs.
	bool        m_isComputed = false;  // True if this is a CTI with a computed destination address.
	                                   // NOTE: This should be removed, once CaseStatement and HLNwayCall are implemented
	                                   // properly.
public:
	            GotoStatement();
	            GotoStatement(ADDRESS jumpDest);
	virtual    ~GotoStatement();

	// Make a deep copy, and make the copy a derived object if needed.
	Statement  *clone() override;

	// Accept a visitor to this Statement
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	// Set and return the destination of the jump. The destination is either an Exp, or an ADDRESS that is
	// converted to a Exp.
	void        setDest(Exp *pd);
	void        setDest(ADDRESS addr);
	virtual Exp *getDest();

	// Return the fixed destination of this CTI. For dynamic CTIs, returns -1.
	ADDRESS     getFixedDest();

	// Adjust the fixed destination by a given amount. Invalid for dynamic CTIs.
	void        adjustFixedDest(int delta);

	// Set and return whether the destination of this CTI is computed.
	// NOTE: These should really be removed, once CaseStatement and HLNwayCall are implemented properly.
	void        setIsComputed(bool b = true);
	bool        isComputed();

	void        print(std::ostream &os = std::cout, bool html = false) override;

	// general search
	bool        search(Exp *, Exp *&) override;

	// Replace all instances of "search" with "replace".
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	// Searches for all instances of a given subexpression within this
	// expression and adds them to a given list in reverse nesting order.
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override;

	// simplify all the uses/defs in this Statement
	void        simplify() override;

	// Statement virtual functions
	bool        isDefinition() override { return false; }
	bool        usesExp(Exp *) override;

	friend class XMLProgParser;
};

class JunctionStatement: public Statement {
public:
	            JunctionStatement() { kind = STMT_JUNCTION; }

	Statement  *clone() override { return new JunctionStatement(); }

	// Accept a visitor (of various kinds) to this Statement. Return true to continue visiting
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	// returns true if this statement defines anything
	bool        isDefinition() override { return false; }

	bool        usesExp(Exp *e) override { return false; }

	void        print(std::ostream &os, bool html = false) override;

	// general search
	bool        search(Exp *search, Exp *&result) override { return false; }
	bool        searchAll(Exp *search, std::list<Exp *> &result) override { return false; }

	// general search and replace. Set cc true to change collectors as well. Return true if any change
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override { return false; }

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override { }

	// simpify internal expressions
	void        simplify() override { }

	void        rangeAnalysis(std::list<Statement *> &execution_paths) override;
	bool        isLoopJunction();
};

/*================================================================================
 * BranchStatement has a condition Exp in addition to the destination of the jump.
 *==============================================================================*/
class BranchStatement: public GotoStatement {
	BRANCH_TYPE jtCond = (BRANCH_TYPE)0;  // The condition for jumping
	Exp        *pCond = nullptr; // The Exp representation of the high level condition: e.g., r[8] == 5
	bool        bFloat = false;  // True if uses floating point CC
	// jtCond seems to be mainly needed for the Pentium weirdness.
	// Perhaps bFloat, jtCond, and size could one day be merged into a type
	int         size = 0;        // Size of the operands, in bits
	RangeMap    ranges2;         // ranges for the not taken edge

public:
	            BranchStatement();
	virtual    ~BranchStatement();

	// Make a deep copy, and make the copy a derived object if needed.
	Statement  *clone() override;

	// Accept a visitor to this Statement
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	// Set and return the BRANCH_TYPE of this jcond as well as whether the
	// floating point condition codes are used.
	void        setCondType(BRANCH_TYPE cond, bool usesFloat = false);
	BRANCH_TYPE getCond() { return jtCond; }
	bool        isFloat() { return bFloat; }
	void        setFloat(bool b) { bFloat = b; }

	// Set and return the Exp representing the HL condition
	Exp        *getCondExpr();
	void        setCondExpr(Exp *pe);
	// As above, no delete (for subscripting)
	void        setCondExprND(Exp *e) { pCond = e; }

	BasicBlock *getFallBB();
	BasicBlock *getTakenBB();
	void        setFallBB(BasicBlock *bb);
	void        setTakenBB(BasicBlock *bb);

	// Probably only used in front386.cc: convert this from an unsigned to a
	// signed conditional branch
	void        makeSigned();

	void        print(std::ostream &os = std::cout, bool html = false) override;

	// general search
	bool        search(Exp *search, Exp *&result) override;

	// Replace all instances of "search" with "replace".
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	// Searches for all instances of a given subexpression within this
	// expression and adds them to a given list in reverse nesting order.
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override;

	// dataflow analysis
	bool        usesExp(Exp *e) override;

	// Range analysis
	void        rangeAnalysis(std::list<Statement *> &execution_paths) override;
	RangeMap   &getRangesForOutEdgeTo(BasicBlock *out);
	RangeMap   &getRanges2Ref() { return ranges2; }
	void        setRanges2(RangeMap &r) { ranges2 = r; }
	void        limitOutputWithCondition(RangeMap &output, Exp *e);

	// simplify all the uses/defs in this Statememt
	void        simplify() override;

	// Generate constraints
	void        genConstraints(LocationSet &cons) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &ch) override;

	friend class XMLProgParser;
};

/*==============================================================================
 * CaseStatement is derived from GotoStatement. In addition to the destination
 * of the jump, it has a switch variable Exp.
 *============================================================================*/
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

class CaseStatement: public GotoStatement {
	SWITCH_INFO *pSwitchInfo = nullptr;  // Ptr to struct with info about the switch
public:
	            CaseStatement();
	virtual    ~CaseStatement();

	// Make a deep copy, and make the copy a derived object if needed.
	Statement  *clone() override;

	// Accept a visitor to this Statememt
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	// Set and return the Exp representing the switch variable
	SWITCH_INFO *getSwitchInfo();
	void        setSwitchInfo(SWITCH_INFO *pss);

	void        print(std::ostream &os = std::cout, bool html = false) override;

	// Replace all instances of "search" with "replace".
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	// Searches for all instances of a given subexpression within this
	// expression and adds them to a given list in reverse nesting order.
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override;

	// dataflow analysis
	bool        usesExp(Exp *e) override;
public:

	// simplify all the uses/defs in this Statement
	void        simplify() override;

	friend class XMLProgParser;
};

/*==============================================================================
 * CallStatement: represents a high level call. Information about parameters and the like are stored here.
 *============================================================================*/
class CallStatement: public GotoStatement {
	bool        returnAfterCall = false;// True if call is effectively followed by a return.

	// The list of arguments passed by this call, actually a list of Assign statements (location := expr)
	StatementList arguments;

	// The list of defines for this call, a list of ImplicitAssigns (used to be called returns).
	// Essentially a localised copy of the modifies of the callee, so the callee could be deleted. Stores types and
	// locations.  Note that not necessarily all of the defines end up being declared as results.
	StatementList defines;

	// Destination of call. In the case of an analysed indirect call, this will be ONE target's return statement.
	// For an unanalysed indirect call, or a call whose callee is not yet sufficiently decompiled due to recursion,
	// this will be nullptr
	Proc       *procDest = nullptr;

	// The signature for this call. NOTE: this used to be stored in the Proc, but this does not make sense when
	// the proc happens to have varargs
	Signature  *signature = nullptr;

	// A UseCollector object to collect the live variables at this call. Used as part of the calculation of
	// results
	UseCollector useCol;

	// A DefCollector object to collect the reaching definitions; used for bypassAndPropagate/localiseExp etc; also
	// the basis for arguments if this is an unanlysed indirect call
	DefCollector defCol;

	// Pointer to the callee ReturnStatement. If the callee is unanlysed, this will be a special ReturnStatement
	// with ImplicitAssigns. Callee could be unanalysed because of an unanalysed indirect call, or a "recursion
	// break".
	ReturnStatement *calleeReturn = nullptr;

public:
	            CallStatement();
	virtual    ~CallStatement();

	void        setNumber(int num) override;
	// Make a deep copy, and make the copy a derived object if needed.
	Statement  *clone() override;

	// Accept a visitor to this stmt
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	void        setArguments(StatementList &args);
	// Set implicit arguments: so far, for testing only:
	//void        setImpArguments(std::vector<Exp *> &arguments);
	//void        setReturns(std::vector<Exp *> &returns);// Set call's return locs
	void        setSigArguments();  // Set arguments based on signature
	StatementList &getArguments() { return arguments; }  // Return call's arguments
	void        updateArguments();  // Update the arguments based on a callee change
	//Exp        *getDefineExp(int i);
	int         findDefine(Exp *e);  // Still needed temporarily for ad hoc type analysis
	void        removeDefine(Exp *e);
	void        addDefine(ImplicitAssign *as);  // For testing
	//void        ignoreReturn(Exp *e);
	//void        ignoreReturn(int n);
	//void        addReturn(Exp *e, Type *ty = nullptr);
	void        updateDefines();  // Update the defines based on a callee change
	StatementList *calcResults();  // Calculate defines(this) isect live(this)
	ReturnStatement *getCalleeReturn() { return calleeReturn; }
	void        setCalleeReturn(ReturnStatement *ret) { calleeReturn = ret; }
	bool        isChildless();
	Exp        *getProven(Exp *e);
	Signature  *getSignature() { return signature; }
	// Localise the various components of expression e with reaching definitions to this call
	// Note: can change e so usually need to clone the argument
	// Was called substituteParams
	Exp        *localiseExp(Exp *e);
	void        localiseComp(Exp *e);  // Localise only xxx of m[xxx]
	// Do the call bypass logic e.g. r28{20} -> r28{17} + 4 (where 20 is this CallStatement)
	// Set ch if changed (bypassed)
	Exp        *bypassRef(RefExp *r, bool &ch);
	void        clearUseCollector() { useCol.clear(); }
	void        addArgument(Exp *e, UserProc *proc);
	Exp        *findDefFor(Exp *e);  // Find the reaching definition for expression e
	Exp        *getArgumentExp(int i);
	void        setArgumentExp(int i, Exp *e);
	void        setNumArguments(int i);
	int         getNumArguments();
	void        removeArgument(int i);
	Type       *getArgumentType(int i);
	void        truncateArguments();
	void        clearLiveEntry();
	void        eliminateDuplicateArgs();

	// Range analysis
	void        rangeAnalysis(std::list<Statement *> &execution_paths) override;

	void        print(std::ostream &os = std::cout, bool html = false) override;

	// general search
	bool        search(Exp *search, Exp *&result) override;

	// Replace all instances of "search" with "replace".
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	// Searches for all instances of a given subexpression within this
	// expression and adds them to a given list in reverse nesting order.
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// Set and return whether the call is effectively followed by a return.
	// E.g. on Sparc, whether there is a restore in the delay slot.
	void        setReturnAfterCall(bool b);
	bool        isReturnAfterCall();

	// Set and return the list of Exps that occur *after* the call (the
	// list of exps in the RTL occur before the call). Useful for odd patterns.
	void        setPostCallExpList(std::list<Exp *> *le);
	std::list<Exp *> *getPostCallExpList();

	// Set and return the destination proc.
	void        setDestProc(Proc *dest);
	Proc       *getDestProc();

	// Generate constraints
	void        genConstraints(LocationSet &cons) override;

	// Data flow based type analysis
	void        dfaTypeAnalysis(bool &ch) override;

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override;

	// dataflow analysis
	bool        usesExp(Exp *e) override;

	// dataflow related functions
	bool        isDefinition() override;
	void        getDefinitions(LocationSet &defs) override;

	bool        definesLoc(Exp *loc) override;  // True if this Statement defines loc
	void        setLeftFor(Exp *forExp, Exp *newExp) override;
	// get how to replace this statement in a use
	//virtual Exp *getRight() { return nullptr; }

	// simplify all the uses/defs in this Statement
	void        simplify() override;

	//void        setIgnoreReturnLoc(bool b);

	void        decompile();

	// Insert actual arguments to match formal parameters
	//void        insertArguments(StatementSet &rs);

	Type       *getTypeFor(Exp *e) override;                 // Get the type defined by this Statement for this location
	void        setTypeFor(Exp *e, Type *ty) override;       // Set the type for this location, defined in this statement
	DefCollector *getDefCollector() { return &defCol; }         // Return pointer to the def collector object
	UseCollector *getUseCollector() { return &useCol; }         // Return pointer to the use collector object
	void        useBeforeDefine(Exp *x) { useCol.insert(x); }   // Add x to the UseCollector for this call
	void        removeLiveness(Exp *e) { useCol.remove(e); }    // Remove e from the UseCollector
	void        removeAllLive() { useCol.clear(); }             // Remove all livenesses
	//Exp        *fromCalleeContext(Exp *e);          // Convert e from callee to caller (this) context
	StatementList &getDefines() { return defines; } // Get list of locations defined by this call
	// Process this call for ellipsis parameters. If found, in a printf/scanf call, truncate the number of
	// parameters if needed, and return true if any signature parameters added
	bool        ellipsisProcessing(Prog *prog);
	bool        convertToDirect();  // Internal function: attempt to convert an indirect to a direct call
	void        useColFromSsaForm(Statement *s) { useCol.fromSSAform(proc, s); }
private:
	// Private helper functions for the above
	void        addSigParam(Type *ty, bool isScanf);
	Assign     *makeArgAssign(Type *ty, Exp *e);

protected:

	void        updateDefineWithType(int n);
	void        appendArgument(Assignment *as) { arguments.append(as); }
	friend class XMLProgParser;
};


/*===========================================================
 * ReturnStatement: represents an ordinary high level return.
 *==========================================================*/
class ReturnStatement : public Statement {
protected:
	// Native address of the (only) return instruction. Needed for branching to this only return statement
	ADDRESS     retAddr = NO_ADDRESS;

	/**
	 * The progression of return information is as follows:
	 * First, reaching definitions are collected in the DefCollector col. These are not sorted or filtered.
	 * Second, some of those definitions make it to the modifieds list, which is sorted and filtered. These are
	 * the locations that are modified by the enclosing procedure. As locations are proved to be preserved (with NO
	 * modification, not even sp = sp+4), they are removed from this list. Defines in calls to the enclosing
	 * procedure are based on this list.
	 * Third, the modifications are initially copied to the returns list (also sorted and filtered, but the returns
	 * have RHS where the modifieds don't). Locations not live at any caller are removed from the returns, but not
	 * from the modifieds.
	 */
	/// A DefCollector object to collect the reaching definitions
	DefCollector col;

	/// A list of assignments that represents the locations modified by the enclosing procedure. These assignments
	/// have no RHS?
	/// These transmit type information to callers
	/// Note that these include preserved locations early on (?)
	StatementList modifieds;

	/// A list of assignments of locations to expressions.
	/// Initially definitions reaching the exit less preserveds; later has locations unused by any callers removed.
	/// A list is used to facilitate ordering. (A set would be ideal, but the ordering depends at runtime on the
	/// signature)
	StatementList returns;

public:
	            ReturnStatement();
	virtual    ~ReturnStatement();

	typedef StatementList::iterator iterator;
	iterator    begin()             { return returns.begin(); }
	iterator    end()               { return returns.end(); }
	iterator    erase(iterator it)  { return returns.erase(it); }
	StatementList &getModifieds()   { return modifieds; }
	StatementList &getReturns()     { return returns; }
	unsigned    getNumReturns() const { return returns.size(); }
	void        updateModifieds();  // Update modifieds from the collector
	void        updateReturns();    // Update returns from the modifieds

	void        print(std::ostream &os = std::cout, bool html = false) override;

	// general search
	bool        search(Exp *, Exp *&) override;

	// Replace all instances of "search" with "replace".
	bool        searchAndReplace(Exp *search, Exp *replace, bool cc = false) override;

	// Searches for all instances of a given subexpression within this statement and adds them to a given list
	bool        searchAll(Exp *search, std::list<Exp *> &result) override;

	// returns true if this statement uses the given expression
	bool        usesExp(Exp *e) override;

	void        getDefinitions(LocationSet &defs) override;

	void        removeModified(Exp *loc);  // Remove from modifieds AND from returns
	void        removeReturn(Exp *loc);    // Remove from returns only
	void        addReturn(Assignment *a);

	Type       *getTypeFor(Exp *e) override;
	void        setTypeFor(Exp *e, Type *ty) override;

	// simplify all the uses/defs in this Statement
	void        simplify() override;

	bool        isDefinition() override { return true; }

	// Get a subscripted version of e from the collector
	Exp        *subscriptWithDef(Exp *e);

	// Make a deep copy, and make the copy a derived object if needed.
	Statement  *clone() override;

	// Accept a visitor to this Statement
	bool        accept(StmtVisitor *visitor) override;
	bool        accept(StmtExpVisitor *visitor) override;
	bool        accept(StmtModifier *visitor) override;
	bool        accept(StmtPartModifier *visitor) override;

	bool        definesLoc(Exp *loc) override;  // True if this Statement defines loc

	// code generation
	void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) override;

	//Exp        *getReturnExp(int n) { return returns[n]; }
	//void        setReturnExp(int n, Exp *e) { returns[n] = e; }
	//void        setSigArguments();  // Set returns based on signature
	DefCollector *getCollector() { return &col; }  // Return pointer to the collector object

	// Get and set the native address for the first and only return statement
	ADDRESS     getRetAddr() { return retAddr; }
	void        setRetAddr(ADDRESS r) { retAddr = r; }

	// Find definition for e (in the collector)
	Exp        *findDefFor(Exp *e) { return col.findDefFor(e); }

	void        dfaTypeAnalysis(bool &ch) override;

	// Remove the stack pointer and return a statement list
	StatementList *getCleanReturns();

	// Temporary hack (not neccesary anymore)
	//void        specialProcessing();

	friend class XMLProgParser;
};

#endif
