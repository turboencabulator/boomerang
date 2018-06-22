/**
 * \file
 * \brief Provides the definition for the various visitor and modifier
 *        classes.
 *
 * These classes sometimes are associated with Statement and Exp classes, so
 * they are here to avoid #include problems, to make exp.cpp and statement.cpp
 * a little less huge.  The main advantage is that they are quick and easy to
 * implement (once you get used to them), and it avoids having to declare
 * methods in every Statement or Exp subclass.
 *
 * Top level classes:
 * - ExpVisitor       (visit expressions)
 * - StmtVisitor      (visit statements)
 * - StmtExpVisitor   (visit expressions in statements)
 * - ExpModifier      (modify expressions)
 * - SimpExpModifier  (simplifying expression modifier)
 * - StmtModifier     (modify expressions in statements; not abstract)
 * - StmtPartModifier (as above with special case for whole of LHS)
 *
 * \authors
 * Copyright (C) 2004-2006, Mike Van Emmerik and Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef VISITOR_H
#define VISITOR_H

#include "exp.h"  // Needs to know class hierarchy, e.g. so that can convert Unary* to Exp* in return of ExpModifier::preVisit()

#include <list>
#include <map>

#include <cstddef>

class Statement;
class Assignment;
class Assign;
class ImplicitAssign;
class PhiAssign;
class BoolAssign;
class CaseStatement;
class CallStatement;
class ReturnStatement;
class GotoStatement;
class BranchStatement;
class ImpRefStatement;

class Cfg;
class LocationSet;
class Prog;
class RTL;
class Signature;
class Type;
class UserProc;
class lessExpStar;

/**
 * The ExpVisitor class is used to iterate over all subexpressions in an
 * expression.
 */
class ExpVisitor {
public:
	                    ExpVisitor() { }
	virtual            ~ExpVisitor() { }

	// visitor functions return false to abandon iterating through the expression (terminate the search)
	// Set recurse to false to not do the usual recursion into children
	virtual bool        visit(   Unary *, bool &) { return true; }
	virtual bool        visit(  Binary *, bool &) { return true; }
	virtual bool        visit( Ternary *, bool &) { return true; }
	virtual bool        visit(TypedExp *, bool &) { return true; }
	virtual bool        visit( FlagDef *, bool &) { return true; }
	virtual bool        visit(  RefExp *, bool &) { return true; }
	virtual bool        visit(Location *, bool &) { return true; }
	// These three have zero arity, so there is nothing to recurse
	virtual bool        visit(   Const *)         { return true; }
	virtual bool        visit(Terminal *)         { return true; }
	virtual bool        visit( TypeVal *)         { return true; }
};

/**
 * This class visits subexpressions, and if a location, sets the UserProc.
 */
class FixProcVisitor : public ExpVisitor {
	// the enclosing UserProc (if a Location)
	UserProc   *proc;

public:
	void        setProc(UserProc *p) { proc = p; }
	bool        visit(Location *, bool &) override;
	// All other virtual functions inherit from ExpVisitor, i.e. they just visit their children recursively
};

/**
 * This class is more or less the opposite of the above.  It finds a proc by
 * visiting the whole expression if necessary.
 */
class GetProcVisitor : public ExpVisitor {
	UserProc   *proc = nullptr;  // The result (or nullptr)

public:
	            GetProcVisitor() { }  // Constructor
	UserProc   *getProc() const { return proc; }
	bool        visit(Location *, bool &) override;
	// All others inherit and visit their children
};

/**
 * This class visits subexpressions, and if a Const, sets or clears a new
 * conscript.
 */
class SetConscripts : public ExpVisitor {
	int         curConscript;
	bool        bInLocalGlobal = false;  // True when inside a local or global
	bool        bClear;                  // True when clearing, not setting
public:
	            SetConscripts(int n, bool bClear) : curConscript(n), bClear(bClear) { }
	int         getLast() const { return curConscript; }
	bool        visit(   Const *) override;
	bool        visit(Location *, bool &) override;
	bool        visit(  Binary *, bool &) override;
	// All other virtual functions inherit from ExpVisitor: return true
};

/**
 * The ExpModifier class is used to iterate over all subexpressions in an
 * expression.  It contains methods for each kind of subexpression found in an
 * and can be used to eliminate switch statements.  It is a little more
 * expensive to use than ExpVisitor, but can make changes to the expression.
 */
class ExpModifier {
protected:
	        bool        mod = false;  // Set if there is any change. Don't have to implement
public:
	                    ExpModifier() { }
	virtual            ~ExpModifier() { }
	        bool        isMod() const { return mod; }
	        void        clearMod() { mod = false; }

	// visitor functions
	// Most times these won't be needed. You only need to override the ones that make a change.
	// preVisit comes before modifications to the children (if any)
	virtual Exp        *preVisit(    Unary *e, bool &) { return e; }
	virtual Exp        *preVisit(   Binary *e, bool &) { return e; }
	virtual Exp        *preVisit(  Ternary *e, bool &) { return e; }
	virtual Exp        *preVisit( TypedExp *e, bool &) { return e; }
	virtual Exp        *preVisit(  FlagDef *e, bool &) { return e; }
	virtual Exp        *preVisit(   RefExp *e, bool &) { return e; }
	virtual Exp        *preVisit( Location *e, bool &) { return e; }
	virtual Exp        *preVisit(    Const *e)         { return e; }
	virtual Exp        *preVisit( Terminal *e)         { return e; }
	virtual Exp        *preVisit(  TypeVal *e)         { return e; }

	// postVisit comes after modifications to the children (if any)
	virtual Exp        *postVisit(   Unary *e)         { return e; }
	virtual Exp        *postVisit(  Binary *e)         { return e; }
	virtual Exp        *postVisit( Ternary *e)         { return e; }
	virtual Exp        *postVisit(TypedExp *e)         { return e; }
	virtual Exp        *postVisit( FlagDef *e)         { return e; }
	virtual Exp        *postVisit(  RefExp *e)         { return e; }
	virtual Exp        *postVisit(Location *e)         { return e; }
	virtual Exp        *postVisit(   Const *e)         { return e; }
	virtual Exp        *postVisit(Terminal *e)         { return e; }
	virtual Exp        *postVisit( TypeVal *e)         { return e; }
};

/**
 * The StmtVisitor class is used for code that has to work with all the
 * Statement classes.  One advantage is that you don't need to declare a
 * function in every class derived from Statement:  The accept methods already
 * do that for you.  It does not automatically visit the expressions in the
 * statement.
 */
class StmtVisitor {
public:
	                    StmtVisitor() { }
	virtual            ~StmtVisitor() { }

	// visitor functions,
	// returns true to continue iterating the container
	virtual bool        visit(            RTL *) { return true; }  // By default, visits all statements.  Mostly, don't do anything at the RTL level.
	virtual bool        visit(         Assign *) { return true; }
	virtual bool        visit(      PhiAssign *) { return true; }
	virtual bool        visit( ImplicitAssign *) { return true; }
	virtual bool        visit(     BoolAssign *) { return true; }
	virtual bool        visit(  GotoStatement *) { return true; }
	virtual bool        visit(BranchStatement *) { return true; }
	virtual bool        visit(  CaseStatement *) { return true; }
	virtual bool        visit(  CallStatement *) { return true; }
	virtual bool        visit(ReturnStatement *) { return true; }
	virtual bool        visit(ImpRefStatement *) { return true; }
};

class StmtConscriptSetter : public StmtVisitor {
	int         curConscript;
	bool        bClear;
public:
	            StmtConscriptSetter(int n, bool bClear) : curConscript(n), bClear(bClear) { }
	int         getLast() const { return curConscript; }

	bool        visit(         Assign *) override;
	bool        visit(      PhiAssign *) override;
	bool        visit( ImplicitAssign *) override;
	bool        visit(     BoolAssign *) override;
	bool        visit(BranchStatement *) override;
	bool        visit(  CaseStatement *) override;
	bool        visit(  CallStatement *) override;
	bool        visit(ReturnStatement *) override;
	bool        visit(ImpRefStatement *) override;
};

/**
 * StmtExpVisitor is a visitor of statements, and of expressions within those
 * expressions.  The visiting of expressions (after the current node) is done
 * by an ExpVisitor (i.e. this is a preorder traversal).
 */
class StmtExpVisitor {
	        bool        ignoreCol;  // True if ignoring collectors
public:
	        ExpVisitor &ev;
	                    StmtExpVisitor(ExpVisitor &v, bool ignoreCol = true) : ignoreCol(ignoreCol), ev(v) { }
	virtual            ~StmtExpVisitor() { }
	virtual bool        visit(         Assign *, bool &) { return true; }
	virtual bool        visit(      PhiAssign *, bool &) { return true; }
	virtual bool        visit( ImplicitAssign *, bool &) { return true; }
	virtual bool        visit(     BoolAssign *, bool &) { return true; }
	virtual bool        visit(  GotoStatement *, bool &) { return true; }
	virtual bool        visit(BranchStatement *, bool &) { return true; }
	virtual bool        visit(  CaseStatement *, bool &) { return true; }
	virtual bool        visit(  CallStatement *, bool &) { return true; }
	virtual bool        visit(ReturnStatement *, bool &) { return true; }
	virtual bool        visit(ImpRefStatement *, bool &) { return true; }

	        bool        isIgnoreCol() const { return ignoreCol; }
};

/**
 * StmtModifier is a class that for all expressions in this statement, makes a
 * modification.  The modification is as a result of an ExpModifier; there is
 * a pointer to such an ExpModifier in a StmtModifier.  Even the top level of
 * the LHS of assignments are changed.  This is useful e.g. when modifiying
 * locations to locals as a result of converting from SSA form,
 * e.g. eax := ebx -> local1 := local2
 *
 * Classes that derive from StmtModifier inherit the code (in the accept
 * member functions) to modify all the expressions in the various types of
 * statement.
 *
 * Because there is nothing specialised about a StmtModifier, it is not an
 * abstract class (can be instantiated).
 */
class StmtModifier {
protected:
	        bool        ignoreCol;
public:
	        ExpModifier &mod;  // The expression modifier object
	                    StmtModifier(ExpModifier &em, bool ic = false) : ignoreCol(ic), mod(em) { }// Constructor
	virtual            ~StmtModifier() { }
	        bool        ignoreCollector() const { return ignoreCol; }
	// This class' visitor functions don't return anything. Maybe we'll need return values at a later stage.
	virtual void        visit(         Assign *, bool &) { }
	virtual void        visit(      PhiAssign *, bool &) { }
	virtual void        visit( ImplicitAssign *, bool &) { }
	virtual void        visit(     BoolAssign *, bool &) { }
	virtual void        visit(  GotoStatement *, bool &) { }
	virtual void        visit(BranchStatement *, bool &) { }
	virtual void        visit(  CaseStatement *, bool &) { }
	virtual void        visit(  CallStatement *, bool &) { }
	virtual void        visit(ReturnStatement *, bool &) { }
	virtual void        visit(ImpRefStatement *, bool &) { }
};

/**
 * As above, but specialised for propagating to.  The top level of the lhs of
 * assignment-like statements (including arguments in calls) is not modified.
 * So for example eax := ebx -> eax := local2, but in m[xxx] := rhs, the rhs
 * and xxx are modified, but not the m[xxx].
 */
class StmtPartModifier {
	        bool        ignoreCol;
public:
	        ExpModifier &mod;  // The expression modifier object
	                    StmtPartModifier(ExpModifier &em, bool ic = false) : ignoreCol(ic), mod(em) { }  // Constructor
	virtual            ~StmtPartModifier() { }
	        bool        ignoreCollector() const { return ignoreCol; }
	// This class' visitor functions don't return anything. Maybe we'll need return values at a later stage.
	virtual void        visit(         Assign *, bool &) { }
	virtual void        visit(      PhiAssign *, bool &) { }
	virtual void        visit( ImplicitAssign *, bool &) { }
	virtual void        visit(     BoolAssign *, bool &) { }
	virtual void        visit(  GotoStatement *, bool &) { }
	virtual void        visit(BranchStatement *, bool &) { }
	virtual void        visit(  CaseStatement *, bool &) { }
	virtual void        visit(  CallStatement *, bool &) { }
	virtual void        visit(ReturnStatement *, bool &) { }
	virtual void        visit(ImpRefStatement *, bool &) { }
};

class PhiStripper : public StmtModifier {
	bool        del = false;  // Set true if this statment is to be deleted
public:
	            PhiStripper(ExpModifier &em) : StmtModifier(em) { }
	void        visit(PhiAssign *, bool &) override;
	bool        getDelete() const { return del; }
};

/**
 * A simplifying expression modifier.  It does a simplification on the parent
 * after a child has been modified.
 */
class SimpExpModifier : public ExpModifier {
protected:
	// These two provide 31 bits (or sizeof (int) - 1) of information about whether the child is unchanged.
	// If the mask overflows, it goes to zero, and from then on the child is reported as always changing.
	// (That's why it's an "unchanged" set of flags, instead of a "changed" set).
	// This is used to avoid calling simplify in most cases where it is not necessary.
	unsigned    mask = 1;
	unsigned    unchanged = (unsigned)-1;
public:
	            SimpExpModifier() { }
	unsigned    getUnchanged() const { return unchanged; }
	bool        isTopChanged() const { return !(unchanged & mask); }
	Exp        *preVisit(    Unary *e, bool &) override { mask <<= 1; return e; }
	Exp        *preVisit(   Binary *e, bool &) override { mask <<= 1; return e; }
	Exp        *preVisit(  Ternary *e, bool &) override { mask <<= 1; return e; }
	Exp        *preVisit( TypedExp *e, bool &) override { mask <<= 1; return e; }
	Exp        *preVisit(  FlagDef *e, bool &) override { mask <<= 1; return e; }
	Exp        *preVisit(   RefExp *e, bool &) override { mask <<= 1; return e; }
	Exp        *preVisit( Location *e, bool &) override { mask <<= 1; return e; }
	Exp        *preVisit(    Const *e)         override { mask <<= 1; return e; }
	Exp        *preVisit( Terminal *e)         override { mask <<= 1; return e; }
	Exp        *preVisit(  TypeVal *e)         override { mask <<= 1; return e; }

	Exp        *postVisit(   Unary *) override;
	Exp        *postVisit(  Binary *) override;
	Exp        *postVisit( Ternary *) override;
	Exp        *postVisit(TypedExp *) override;
	Exp        *postVisit( FlagDef *) override;
	Exp        *postVisit(  RefExp *) override;
	Exp        *postVisit(Location *) override;
	Exp        *postVisit(   Const *) override;
	Exp        *postVisit(Terminal *) override;
	Exp        *postVisit( TypeVal *) override;
};

/**
 * A modifying visitor to process all references in an expression, bypassing
 * calls (and phi statements if they have been replaced by copy assignments),
 * and performing simplification on the direct parent of the expression that
 * is modified.
 *
 * \note This is sometimes not enough!  Consider changing (r+x)+K2 where x
 * gets changed to K1. Now you have (r+K1)+K2, but simplifying only the parent
 * doesn't simplify the K1+K2.
 *
 * Used to also propagate, but this became unwieldy with -l propagation
 * limiting.
 */
class CallBypasser : public SimpExpModifier {
	Statement  *enclosingStmt;  // Statement that is being modified at present, for debugging only
public:
	            CallBypasser(Statement *enclosing) : enclosingStmt(enclosing) { }
	Exp        *postVisit(  RefExp *) override;
	Exp        *postVisit(Location *) override;
};

class UsedLocsFinder : public ExpVisitor {
	LocationSet *used;    // Set of Exps
	bool        memOnly;  // If true, only look inside m[...]
public:
	            UsedLocsFinder(LocationSet &used, bool memOnly) : used(&used), memOnly(memOnly) { }
	           ~UsedLocsFinder() { }

	LocationSet *getLocSet() const { return used; }
	void        setMemOnly(bool b) { memOnly = b; }
	bool        isMemOnly() const { return memOnly; }

	bool        visit(  RefExp *, bool &) override;
	bool        visit(Location *, bool &) override;
	bool        visit(Terminal *) override;
};

/**
 * This class differs from the above in these ways:
 * 1. It counts locals implicitly referred to with (cast to pointer)(sp-K).
 * 2. It does not recurse inside the memof (thus finding the stack pointer as a local).
 * 3. Only used after fromSSA, so no RefExps to visit.
 */
class UsedLocalFinder : public ExpVisitor {
	LocationSet *used;        // Set of used locals' names
	UserProc   *proc;         // Enclosing proc
	bool        all = false;  // True if see opDefineAll
public:
	            UsedLocalFinder(LocationSet &used, UserProc *proc) : used(&used), proc(proc) { }
	           ~UsedLocalFinder() { }

	LocationSet *getLocSet() const { return used; }
	bool        wasAllFound() const { return all; }

	bool        visit(TypedExp *, bool &) override;
	bool        visit(Location *, bool &) override;
	bool        visit(Terminal *) override;
};

class UsedLocsVisitor : public StmtExpVisitor {
	bool        countCol;  // True to count uses in collectors
public:
	            UsedLocsVisitor(ExpVisitor &v, bool cc) : StmtExpVisitor(v), countCol(cc) { }
	virtual    ~UsedLocsVisitor() { }
	// Needs special attention because the lhs of an assignment isn't used (except where it's m[blah], when blah is
	// used)
	bool        visit(         Assign *, bool &) override;
	bool        visit(      PhiAssign *, bool &) override;
	bool        visit( ImplicitAssign *, bool &) override;
	// A BoolAssign uses its condition expression, but not its destination (unless it's an m[x], in which case x is
	// used and not m[x])
	bool        visit(     BoolAssign *, bool &) override;
	// Returns aren't used (again, except where m[blah] where blah is used), and there is special logic for when the
	// pass is final
	bool        visit(  CallStatement *, bool &) override;
	// Only consider the first return when final
	bool        visit(ReturnStatement *, bool &) override;
};

class ExpSubscripter : public ExpModifier {
	Exp        *search;
	Statement  *def;
public:
	            ExpSubscripter(Exp *s, Statement *d) : search(s), def(d) { }
	Exp        *preVisit(  Binary *, bool &) override;
	Exp        *preVisit(  RefExp *, bool &) override;
	Exp        *preVisit(Location *, bool &) override;
	Exp        *preVisit(Terminal *) override;
};

class StmtSubscripter : public StmtModifier {
public:
	            StmtSubscripter(ExpSubscripter &es) : StmtModifier(es) { }
	virtual    ~StmtSubscripter() { }

	void        visit(        Assign *, bool &) override;
	void        visit(     PhiAssign *, bool &) override;
	void        visit(ImplicitAssign *, bool &) override;
	void        visit(    BoolAssign *, bool &) override;
	void        visit( CallStatement *, bool &) override;
};

class SizeStripper : public ExpModifier {
public:
	            SizeStripper() { }
	virtual    ~SizeStripper() { }

	Exp        *preVisit(Binary *, bool &) override;
};

class ExpConstCaster: public ExpModifier {
	int         num;
	Type       *ty;
	bool        changed = false;
public:
	            ExpConstCaster(int num, Type *ty) : num(num), ty(ty) { }
	virtual    ~ExpConstCaster() { }
	bool        isChanged() const { return changed; }

	Exp        *preVisit(Const *) override;
};

class ConstFinder : public ExpVisitor {
	std::list<Const *> &lc;
public:
	            ConstFinder(std::list<Const *> &lc) : lc(lc) { }
	virtual    ~ConstFinder() { }

	bool        visit(Location *, bool &) override;
	bool        visit(   Const *) override;
};

/**
 * This class is an ExpModifier because although most of the time it merely
 * maps expressions to locals, in one case, where sp-K is found, we replace it
 * with a[m[sp-K]] so the back end emits it as &localX.
 *
 * FIXME:  This is probably no longer necessary, since the back end no longer
 * maps anything!
 */
class DfaLocalMapper : public ExpModifier {
	UserProc   *proc;
	Prog       *prog;
	Signature  *sig;  // Look up once (from proc) for speed
	bool        processExp(Exp *);  // Common processing here
public:
	bool        change = false;  // True if changed this statement

	            DfaLocalMapper(UserProc *);

	//Exp        *preVisit(   Unary *, bool &) override;  // To process a[X]
	Exp        *preVisit(  Binary *, bool &) override;  // To look for sp -+ K
	Exp        *preVisit(TypedExp *, bool &) override;  // To prevent processing TypedExps more than once
	Exp        *preVisit(Location *, bool &) override;  // To process m[X]
};

#if 0  // FIXME: deleteme
class StmtDfaLocalMapper : public StmtModifier {
public:
	            StmtDfaLocalMapper(ExpModifier &em, bool ic = false) : StmtModifier(em, ic) { }

	void        visit(         Assign *, bool &) override;
	void        visit(      PhiAssign *, bool &) override;
	void        visit( ImplicitAssign *, bool &) override;
	void        visit(     BoolAssign *, bool &) override;
	void        visit(BranchStatement *, bool &) override;
	void        visit(  CaseStatement *, bool &) override;
	void        visit(  CallStatement *, bool &) override;
	void        visit(ReturnStatement *, bool &) override;
	void        visit(ImpRefStatement *, bool &) override;
};
#endif

/**
 * Convert any exp{-} (with null definition) so that the definition points
 * instead to an implicit assignment (exp{0}).
 *
 * \note It is important to process refs in a depth first manner, so that e.g.
 * m[sp{-}-8]{-} -> m[sp{0}-8]{-} first, so that there is never an implicit
 * definition for m[sp{-}-8], only ever for m[sp{0}-8].
 */
class ImplicitConverter : public ExpModifier {
	Cfg        *cfg;
public:
	            ImplicitConverter(Cfg *cfg) : cfg(cfg) { }
	Exp        *postVisit(RefExp *) override;
};

class StmtImplicitConverter : public StmtModifier {
	Cfg        *cfg;
public:
	// False to not ignore collectors (want to make sure that collectors have valid expressions so you can ascendType)
	            StmtImplicitConverter(ImplicitConverter &ic, Cfg *cfg) : StmtModifier(ic, false), cfg(cfg) { }
	void        visit(PhiAssign *, bool &) override;
};

class Localiser : public SimpExpModifier {
	CallStatement *call;  // The call to localise to
public:
	            Localiser(CallStatement *call) : call(call) { }
	Exp        *preVisit(   RefExp *, bool &) override;
	Exp        *postVisit(Location *) override;
	Exp        *postVisit(Terminal *) override;
};


class ComplexityFinder : public ExpVisitor {
	int         count = 0;
	UserProc   *proc;
public:
	            ComplexityFinder(UserProc *p) : proc(p) { }
	int         getDepth() const { return count; }

	bool        visit(   Unary *, bool &) override;
	bool        visit(  Binary *, bool &) override;
	bool        visit( Ternary *, bool &) override;
	bool        visit(Location *, bool &) override;
};

/**
 * Used by range analysis.
 */
class MemDepthFinder : public ExpVisitor {
	int         depth = 0;
public:
	            MemDepthFinder() { }
	bool        visit(Location *, bool &) override;
	int         getDepth() const { return depth; }
};

/**
 * A class to propagate everything, regardless, to this expression.  Does not
 * consider memory expressions and whether the address expression is
 * primitive.  Use with caution; mostly Statement::propagateTo() should be
 * used.
 */
class ExpPropagator : public SimpExpModifier {
	bool        change = false;
public:
	            ExpPropagator() { }
	bool        isChanged() const { return change; }
	void        clearChanged() { change = false; }
	Exp        *postVisit(RefExp *) override;
};

/**
 * Test an address expression (operand of a memOf) for primitiveness (i.e. if
 * it is possible to SSA rename the memOf without problems).  Note that the
 * PrimitiveTester is not used with the memOf expression, only its address
 * expression.
 */
class PrimitiveTester : public ExpVisitor {
	bool        result = true;
public:
	            PrimitiveTester() { }  // Initialise result true: need AND of all components
	bool        getResult() const { return result; }
	bool        visit(  RefExp *, bool &) override;
	bool        visit(Location *, bool &) override;
};

/**
 * Test if an expression (usually the RHS on an assignment) contains memory
 * expressions.  If so, it may not be safe to propagate the assignment.
 *
 * NO LONGER USED.
 */
class ExpHasMemofTester : public ExpVisitor {
	bool        result = false;
	UserProc   *proc;
public:
	            ExpHasMemofTester(UserProc *proc) : proc(proc) { }
	bool        getResult() const { return result; }
	bool        visit(Location *, bool &) override;
};

class TempToLocalMapper : public ExpVisitor {
	UserProc   *proc;  // Proc object for storing the symbols
public:
	            TempToLocalMapper(UserProc *p) : proc(p) { }
	bool        visit(Location *, bool &) override;
};

/**
 * Name registers and temporaries.
 */
class ExpRegMapper : public ExpVisitor {
	UserProc   *proc;  // Proc object for storing the symbols
	Prog       *prog;
public:
	            ExpRegMapper(UserProc *proc);
	bool        visit(RefExp *, bool &) override;
};

class StmtRegMapper : public StmtExpVisitor {
public:
	            StmtRegMapper(ExpRegMapper &erm) : StmtExpVisitor(erm) { }
	virtual bool common(  Assignment *, bool &);
	bool        visit(        Assign *, bool &) override;
	bool        visit(     PhiAssign *, bool &) override;
	bool        visit(ImplicitAssign *, bool &) override;
	bool        visit(    BoolAssign *, bool &) override;
};

class ConstGlobalConverter : public ExpModifier {
	Prog       *prog;  // Pointer to the Prog object, for reading memory
public:
	            ConstGlobalConverter(Prog *pg) : prog(pg) { }
	Exp        *preVisit(RefExp *, bool &) override;
};

/**
 * Count the number of times a reference expression is used.  Increments the
 * count multiple times if the same reference expression appears multiple
 * times (so can't use UsedLocsFinder for this).
 */
class ExpDestCounter : public ExpVisitor {
	std::map<Exp *, int, lessExpStar> &destCounts;
public:
	            ExpDestCounter(std::map<Exp *, int, lessExpStar> &dc) : destCounts(dc) { }
	bool        visit(RefExp *, bool &) override;
};

/**
 * FIXME:  Do I need to count collectors?  All the visitors and modifiers
 * should be refactored to conditionally visit or modify collectors, or not.
 */
class StmtDestCounter : public StmtExpVisitor {
public:
	            StmtDestCounter(ExpDestCounter &edc) : StmtExpVisitor(edc) { }
};

/**
 * Search an expression for flags calls, e.g. SETFFLAGS(...) & 0x45.
 */
class FlagsFinder : public ExpVisitor {
	bool        found = false;
public:
	            FlagsFinder() { }
	bool        isFound() const { return found; }
private:
	bool        visit(Binary *, bool &) override;
};

/**
 * Search an expression for a bad memof (non subscripted or not linked with a
 * symbol, i.e. local or parameter).
 */
class BadMemofFinder : public ExpVisitor {
	bool        found = false;
	UserProc   *proc;
public:
	            BadMemofFinder(UserProc *proc) : proc(proc) { }
	bool        isFound() const { return found; }
private:
	bool        visit(  RefExp *, bool &) override;
	bool        visit(Location *, bool &) override;
};

class ExpCastInserter : public ExpModifier {
	UserProc   *proc;  // The enclising UserProc
public:
	            ExpCastInserter(UserProc *proc) : proc(proc) { }
	static void checkMemofType(Exp *, Type *);
	Exp        *preVisit(TypedExp *e, bool &recurse) override { recurse = false; return e; }  // Don't consider if already cast
	Exp        *postVisit( Binary *) override;
	Exp        *postVisit( RefExp *) override;
	Exp        *postVisit(  Const *) override;
};

class StmtCastInserter : public StmtVisitor {
	ExpCastInserter *ema;
public:
	            StmtCastInserter() { }
	bool        common(   Assignment *);
	bool        visit(        Assign *) override;
	bool        visit(     PhiAssign *) override;
	bool        visit(ImplicitAssign *) override;
	bool        visit(    BoolAssign *) override;
};

/**
 * Transform an exp by applying mappings to the subscripts.  This used to be
 * done by many Exp::fromSSAform() functions.
 *
 * \note Mappings have to be done depth first, so e.g. m[r28{0}-8]{22} ->
 * m[esp-8]{22} first, otherwise there wil be a second implicit definition for
 * m[esp{0}-8] (original should be b[esp+8] by now).
 */
class ExpSsaXformer : public ExpModifier {
	UserProc   *proc;
public:
	            ExpSsaXformer(UserProc *proc) : proc(proc) { }
	UserProc   *getProc() const { return proc; }

	Exp        *postVisit(RefExp *) override;
};

class StmtSsaXformer : public StmtModifier {
	UserProc   *proc;
public:
	            StmtSsaXformer(ExpSsaXformer &esx, UserProc *proc) : StmtModifier(esx), proc(proc) { }
	//virtual    ~StmtSsaXformer() { }
	void        commonLhs(Assignment *);

	void        visit(        Assign *, bool &) override;
	void        visit(     PhiAssign *, bool &) override;
	void        visit(ImplicitAssign *, bool &) override;
	void        visit(    BoolAssign *, bool &) override;
	void        visit( CallStatement *, bool &) override;
};

#endif
