/**
 * \file
 * \brief Provides the definition for the Exp class and its subclasses.
 *
 *     Main class hierarchy:    Exp (abstract)
 *                        _____/ | \
 *                       /       |  \
 *                    Unary    Const Terminal
 *       TypedExp____/  |   \         \
 *        FlagDef___/ Binary Location  TypeVal
 *         RefExp__/    |
 *                   Ternary
 *
 * \authors
 * Copyright (C) 2002-2006, Trent Waddington and Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef EXP_H
#define EXP_H

#include "operator.h"   // Declares the OPER enum
#include "types.h"      // For ADDRESS, etc

#include <iostream>     // For std::cout
#include <ostream>      // For std::ostream
#include <list>
#include <map>
#include <string>
#include <vector>

#include <cassert>
#include <cstdint>

class BasicBlock;
class ExpModifier;
class ExpVisitor;
class LocationSet;
class Proc;
class RTL;
class Statement;
class Type;
class TypeVal;
class UserProc;

/**
 * Exp is an expression class, though it will probably be used to hold many
 * other things (e.g. perhaps transformations).  It is a standard tree
 * representation.  Exp itself is abstract.  A special class Const is used for
 * constants.  Unary, Binary, and Ternary hold 1, 2, and 3 subexpressions
 * respectively.  For efficiency of representation, these have to be separate
 * classes, derived from Exp.
 *
 * Class Exp is abstract.  However, the constructor can be called from the
 * constructors of derived classes, and virtual functions not overridden by
 * derived classes can be called.
 */
class Exp {
	friend class XMLProgParser;

protected:
	        OPER        op;  // The operator (e.g. opPlus)

	// Constructor, with ID
	                    Exp(OPER op) : op(op) { }
public:
	// Virtual destructor
	virtual            ~Exp() { }

	// Clone
	virtual Exp        *clone() const = 0;

	// Return the operator. Note: I'd like to make this protected, but then subclasses don't seem to be able to use
	// it (at least, for subexpressions)
	        OPER        getOper() const { return op; }
	        void        setOper(OPER x) { op = x; }  // A few simplifications use this

	/**
	 * \name Printing
	 * \{
	 */
	virtual void        print(std::ostream &os, bool html = false) const = 0;
	virtual void        printr(std::ostream &os, bool html = false) const { print(os, html); }
	        void        printt(std::ostream &os = std::cout) const;
	        void        printAsHL(std::ostream &os = std::cout) const;
	        std::string prints() const;
	virtual void        printx(int ind) const = 0;

	        void        createDot(std::ostream &os) const;
	virtual void        appendDot(std::ostream &os) const = 0;
	/** \} */

	/**
	 * \name Comparison
	 * \{
	 */
	virtual bool        operator ==(const Exp &) const = 0;
	virtual bool        operator < (const Exp &) const = 0;
	virtual bool        operator <<(const Exp &e) const { return (*this < e); }
	virtual bool        operator *=(const Exp &) const = 0;
	/** \} */

	/**
	 * \name Sub expressions
	 * \{
	 */
	// Return the number of subexpressions. This is only needed in rare cases.
	// Could use polymorphism for all those cases, but this is easier
	virtual int         getArity() const { return 0; }  // Overridden for Unary, Binary, etc

	// These are here so we can (optionally) prevent code clutter.
	// Using a *Exp (that is known to be a Binary* say), you can just directly call getSubExp2.
	// However, you can still choose to cast from Exp* to Binary* etc. and avoid the virtual call
	virtual Exp        *getSubExp1() const { return nullptr; }
	virtual Exp        *getSubExp2() const { return nullptr; }
	virtual Exp        *getSubExp3() const { return nullptr; }
	virtual void        setSubExp1(Exp *e) { }
	virtual void        setSubExp2(Exp *e) { }
	virtual void        setSubExp3(Exp *e) { }
	/** \} */

	/**
	 * \name Enquiry functions
	 * \{
	 */
	// True if this is a call to a flag function
	        bool        isFlagCall() const { return op == opFlagCall; }
	// True if this represents one of the abstract flags locations, int or float
	        bool        isFlags() const { return op == opFlags || op == opFflags; }
	// True if is one of the main 4 flags
	        bool        isMainFlag() const { return op >= opZF && op <= opOF; }
	// True if this is a register location
	        bool        isRegOf() const { return op == opRegOf; }
	// True if this is a register location with a constant index
	        bool        isRegOfK() const;
	// True if this is a specific numeric register
	        bool        isRegN(int n) const;
	// True if this is a memory location (any memory nesting depth)
	        bool        isMemOf() const { return op == opMemOf; }
	// True if this is an address of
	        bool        isAddrOf() const { return op == opAddrOf; }
	// True if this is an array expression
	        bool        isArrayIndex() const { return op == opArrayIndex; }
	// True if this is a struct member access
	        bool        isMemberOf() const { return op == opMemberAccess; }
	// True if this is a temporary. Note some old code still has r[tmp]
	        bool        isTemp() const;
	// True if this is the anull Terminal (anulls next instruction)
	        bool        isAnull() const { return op == opAnull; }
	// True if this is the Nil Terminal (terminates lists; "NOP" expression)
	        bool        isNil() const { return op == opNil; }
	// True if this is %pc
	        bool        isPC() const { return op == opPC; }
	// True if is %afp, %afp+k, %afp-k, or a[m[<any of these>]]
	        bool        isAfpTerm() const;
	// True if is int const
	        bool        isIntConst() const { return op == opIntConst; }
	// True if is string const
	        bool        isStrConst() const { return op == opStrConst; }
	// Get string constant even if mangled
	        const char *getAnyStrConst() const;
	// True if is flt point const
	        bool        isFltConst() const { return op == opFltConst; }
	// True if inteter or string constant
	        bool        isConst() const { return op == opIntConst || op == opStrConst; }
	// True if is a post-var expression (var_op' in SSL file)
	        bool        isPostVar() const { return op == opPostVar; }
	// True if this is an opSize (size case; deprecated)
	        bool        isSizeCast() const { return op == opSize; }
	// True if this is a subscripted expression (SSA)
	        bool        isSubscript() const { return op == opSubscript; }
	// True if this is a phi assignmnet (SSA)
	//        bool        isPhi() const { return op == opPhi; }
	// True if this is a local variable
	        bool        isLocal() const { return op == opLocal; }
	// True if this is a global variable
	        bool        isGlobal() const { return op == opGlobal; }
	// True if this is a typeof
	        bool        isTypeOf() const { return op == opTypeOf; }
	// Get the index for this var
	//        int         getVarIndex() const;
	// True if this is a terminal
	virtual bool        isTerminal() const { return false; }
	// True if this is the constant "true"
	        bool        isTrue() const { return op == opTrue; }
	// True if this is the constant "false"
	        bool        isFalse() const { return op == opFalse; }
	// True if this is a disjunction, i.e. x or y
	        bool        isDisjunction() const { return op == opOr; }
	// True if this is a conjunction, i.e. x and y
	        bool        isConjunction() const { return op == opAnd; }
	// True if this is a boolean constant
	        bool        isBoolConst() const { return op == opTrue || op == opFalse; }
	// True if this is an equality (== or !=)
	        bool        isEquality() const { return op == opEquals /*|| op == opNotEqual*/; }
	// True if this is a comparison
	        bool        isComparison() const { return op == opEquals   || op == opNotEqual
	                                               || op == opGtr      || op == opLess
	                                               || op == opGtrUns   || op == opLessUns
	                                               || op == opGtrEq    || op == opLessEq
	                                               || op == opGtrEqUns || op == opLessEqUns; }
	// True if this is a TypeVal
	        bool        isTypeVal() const { return op == opTypeVal; }
	// True if this is a machine feature
	        bool        isMachFtr() const { return op == opMachFtr; }
	// True if this is a parameter. Note: opParam has two meanings: a SSL parameter, or a function parameter
	        bool        isParam() const { return op == opParam; }

	// True if this is a location
	        bool        isLocation() const { return op == opMemOf  || op == opRegOf
	                                             || op == opGlobal || op == opLocal
	                                             || op == opParam; }

	// True if this is a typed expression
	        bool        isTypedExp() const { return op == opTypedExp; }
	/** \} */


	// FIXME: are these used?
	virtual Exp        *match(const Exp *pattern) const;
	virtual bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const;

	/**
	 * \name Search and replace
	 * \{
	 */
	virtual bool        search(Exp *search, Exp *&result);
	        bool        searchAll(Exp *search, std::list<Exp *> &result);
	        Exp        *searchReplace(Exp *search, Exp *replace, bool &change);
	        Exp        *searchReplaceAll(Exp *search, Exp *replace, bool &change, bool once = false);
	static  void        doSearch(Exp *search, Exp *&pSrc, std::list<Exp **> &li, bool once);
	virtual void        doSearchChildren(Exp *search, std::list<Exp **> &li, bool once);
	/** \} */

	//        Exp        *getGuard() const;

	/**
	 * \name Expression simplification
	 * \{
	 */
	        void        partitionTerms(std::list<Exp *> &positives, std::list<Exp *> &negatives, std::vector<int> &integers, bool negate);
	virtual Exp        *simplifyArith() { return this; }
	static  Exp        *Accumulate(std::list<Exp *> exprs);
	        Exp        *simplify();
	virtual Exp        *polySimplify(bool &bMod) { bMod = false; return this; }
	virtual Exp        *simplifyAddr() { return this; }
	virtual Exp        *simplifyConstraint() { return this; }
	        Exp        *fixSuccessor();
	        Exp        *killFill();
	/** \} */

	        void        addUsedLocs(LocationSet &used, bool memOnly = false);

	        Exp        *removeSubscripts(bool &allZero);

	// Get number of definitions (statements this expression depends on)
	virtual int         getNumRefs() const { return 0; }

	        Exp        *fromSSAleft(UserProc *proc, Statement *d);

	virtual Exp        *genConstraints(Exp *result);

	/**
	 * \name Visitation
	 * \{
	 */
	// Note: best to have accept() as pure virtual, so you don't forget to implement it for new subclasses of Exp
	virtual bool        accept(ExpVisitor &) = 0;
	virtual Exp        *accept(ExpModifier &) = 0;
	        void        fixLocationProc(UserProc *p);
	        UserProc   *findProc();
	// Set or clear the constant subscripts
	        void        setConscripts(int n, bool bClear);
	        Exp        *stripSizes();
	// Subscript all e in this Exp with statement def:
	        Exp        *expSubscriptVar(Exp *e, Statement *def /*, Cfg *cfg */);
	// Subscript all e in this Exp with 0 (implicit assignments)
	        Exp        *expSubscriptValNull(Exp *e /*, Cfg *cfg */);
	// Subscript all locations in this expression with their implicit assignments
	        Exp        *expSubscriptAllNull(/*Cfg *cfg*/);
	// Perform call bypass and simple (assignment only) propagation to this exp
	// Note: can change this, so often need to clone before calling
	        Exp        *bypass();
	        void        bypassComp();                   // As above, but only the xxx of m[xxx]
	        int         getComplexityDepth(UserProc *proc);
	        int         getMemDepth();
	        Exp        *propagateAll();
	        Exp        *propagateAllRpt(bool &changed);
	        bool        containsFlags();                // Check if this exp contains any flag calls
	        bool        containsBadMemof(UserProc *p);  // Check if this Exp contains a bare (non subscripted) memof
	        bool        containsMemof(UserProc *proc);  // Check of this Exp contains any memof at all. Not used.
	/** \} */

	// Data flow based type analysis (implemented in type/dfa.cpp)
	// Pull type information up the expression tree
	virtual Type       *ascendType() { assert(0); return nullptr; }
	// Push type information down the expression tree
	virtual void        descendType(Type *parentType, bool &ch, Statement *s) { assert(0); }
};

// Not part of the Exp class, but logically belongs with it:
std::ostream &operator <<(std::ostream &, const Exp *) __attribute__((deprecated));
std::ostream &operator <<(std::ostream &, const Exp &);

/**
 * Const is a subclass of Exp, and holds either an integer, floating point,
 * string, or address constant.
 */
class Const : public Exp {
	friend class XMLProgParser;

	union {
		int         i;      // Integer
		// Note: although we have i and a as unions, both often use the same operator (opIntConst).
		// There is no opCodeAddr any more.
		ADDRESS     a;      // void* conflated with unsigned int: needs fixing
		uint64_t    ll;     // 64 bit integer
		double      d;      // Double precision float
		const char *p;      // Pointer to string
		                    // Don't store string: function could be renamed
		Proc       *pp;     // Pointer to function
	} u;
	int         conscript = 0;  // like a subscript for constants
	Type       *type;           // Constants need types during type analysis

public:
	// Special constructors overloaded for the various constants
	            Const(int);
	            Const(uint64_t);
	            Const(ADDRESS);
	            Const(double);
	            Const(const char *);
	            Const(const std::string &);
	            Const(Proc *);
	// Copy constructor
	            Const(const Const &);

	// Nothing to destruct: Don't deallocate the string passed to constructor

	// Clone
	Exp        *clone() const override;

	// Compare
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator *=(const Exp &) const override;

	// Get the constant
	int         getInt()  const { return u.i;  }
	uint64_t    getLong() const { return u.ll; }
	double      getFlt()  const { return u.d;  }
	const char *getStr()  const { return u.p;  }
	ADDRESS     getAddr() const { return u.a;  }
	const char *getFuncName() const;

	// Set the constant
	void        setInt(int i)         { u.i  = i;  op = opIntConst;  }
	void        setLong(uint64_t ll)  { u.ll = ll; op = opLongConst; }
	void        setFlt(double d)      { u.d  = d;  op = opFltConst;  }
	void        setStr(const char *p) { u.p  = p;  op = opStrConst;  }
	void        setAddr(ADDRESS a)    { u.a  = a;  op = opIntConst;  }

	// Get and set the type
	Type       *getType() const { return type; }
	void        setType(Type *ty) { type = ty; }

	void        print(std::ostream &os, bool html = false) const override;
	// Print "recursive" (extra parens not wanted at outer levels)
	void        printNoQuotes(std::ostream &os) const;
	void        printx(int ind) const override;

	void        appendDot(std::ostream &os) const override;
	Exp        *genConstraints(Exp *restrictTo) override;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;

	bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const override;

	int         getConscript() const { return conscript; }
	void        setConscript(int cs) { conscript = cs; }

	Type       *ascendType() override;
	void        descendType(Type *parentType, bool &ch, Statement *s) override;
};

/**
 * Terminal is a subclass of Exp, and holds special zero arity items such as
 * opFlags (abstract flags register).
 */
class Terminal : public Exp {
	friend class XMLProgParser;

public:
	// Constructors
	            Terminal(OPER op);
	            Terminal(const Terminal &o);  // Copy constructor

	// Clone
	Exp        *clone() const override;

	// Compare
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator *=(const Exp &) const override;

	void        print(std::ostream &os, bool html = false) const override;
	void        appendDot(std::ostream &os) const override;
	void        printx(int ind) const override;

	bool        isTerminal() const override { return true; }

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;

	Type       *ascendType() override;
	void        descendType(Type *parentType, bool &ch, Statement *s) override;

	bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const override;
};

/**
 * Unary is a subclass of Exp, holding one subexpression.
 */
class Unary : public Exp {
	friend class XMLProgParser;

protected:
	Exp        *subExp1 = nullptr;  // One subexpression pointer

	// Constructor, with just ID
	            Unary(OPER op);
public:
	// Constructor, with ID and subexpression
	            Unary(OPER op, Exp *e);
	// Copy constructor
	            Unary(const Unary &o);

	// Destructor
	virtual    ~Unary();

	// Clone
	Exp        *clone() const override;

	// Compare
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator *=(const Exp &) const override;

	// Arity
	int         getArity() const override { return 1; }

	// Print
	void        print(std::ostream &os, bool html = false) const override;
	void        appendDot(std::ostream &os) const override;
	void        printx(int ind) const override;

	// Set first subexpression
	void        setSubExp1(Exp *e) override;
	void        setSubExp1ND(Exp *e) { subExp1 = e; }
	// Get first subexpression
	Exp        *getSubExp1() const override;

	Exp        *match(const Exp *pattern) const override;
	bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const override;

	// Search children
	void        doSearchChildren(Exp *search, std::list<Exp **> &li, bool once) override;

	// Do the work of simplifying this expression
	Exp        *polySimplify(bool &bMod) override;
	Exp        *simplifyArith() override;
	Exp        *simplifyAddr() override;
	Exp        *simplifyConstraint() override;

	// Type analysis
	Exp        *genConstraints(Exp *restrictTo) override;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;

	Type       *ascendType() override;
	void        descendType(Type *parentType, bool &ch, Statement *s) override;
};

/**
 * Binary is a subclass of Unary, holding two subexpressions.
 */
class Binary : public Unary {
	friend class XMLProgParser;

protected:
	Exp        *subExp2 = nullptr;  // Second subexpression pointer

	// Constructor, with ID
	            Binary(OPER op);
public:
	// Constructor, with ID and subexpressions
	            Binary(OPER op, Exp *e1, Exp *e2);
	// Copy constructor
	            Binary(const Binary &o);

	// Destructor
	virtual    ~Binary();

	// Clone
	Exp        *clone() const override;

	// Compare
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator *=(const Exp &) const override;

	// Arity
	int         getArity() const override { return 2; }

	// Print
	void        print(std::ostream &os, bool html = false) const override;
	void        printr(std::ostream &os, bool html = false) const override;
	void        appendDot(std::ostream &os) const override;
	void        printx(int ind) const override;

	// Set second subexpression
	void        setSubExp2(Exp *e) override;
	// Get second subexpression
	Exp        *getSubExp2() const override;
	void        commute();

	Exp        *match(const Exp *pattern) const override;
	bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const override;

	// Search children
	void        doSearchChildren(Exp *search, std::list<Exp **> &li, bool once) override;

	// Do the work of simplifying this expression
	Exp        *polySimplify(bool &bMod) override;
	Exp        *simplifyArith() override;
	Exp        *simplifyAddr() override;
	Exp        *simplifyConstraint() override;

	// Type analysis
	Exp        *genConstraints(Exp *restrictTo) override;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;

	Type       *ascendType() override;
	void        descendType(Type *parentType, bool &ch, Statement *s) override;

private:
	Exp        *constrainSub(TypeVal *typeVal1, TypeVal *typeVal2);
};

/**
 * Ternary is a subclass of Binary, holding three subexpressions.
 */
class Ternary : public Binary {
	friend class XMLProgParser;

	Exp        *subExp3 = nullptr;  // Third subexpression pointer

	// Constructor, with operator
	            Ternary(OPER op);
public:
	// Constructor, with operator and subexpressions
	            Ternary(OPER op, Exp *e1, Exp *e2, Exp *e3);
	// Copy constructor
	            Ternary(const Ternary &o);

	// Destructor
	virtual    ~Ternary();

	// Clone
	Exp        *clone() const override;

	// Compare
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator *=(const Exp &) const override;

	// Arity
	int         getArity() const override { return 3; }

	// Print
	void        print(std::ostream &os, bool html = false) const override;
	void        printr(std::ostream &os, bool html = false) const override;
	void        appendDot(std::ostream &os) const override;
	void        printx(int ind) const override;

	// Set third subexpression
	void        setSubExp3(Exp *e) override;
	// Get third subexpression
	Exp        *getSubExp3() const override;

	// Search children
	void        doSearchChildren(Exp *search, std::list<Exp **> &li, bool once) override;

	Exp        *polySimplify(bool &bMod) override;
	Exp        *simplifyArith() override;
	Exp        *simplifyAddr() override;

	// Type analysis
	Exp        *genConstraints(Exp *restrictTo) override;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;

	bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const override;

	Type       *ascendType() override;
	void        descendType(Type *parentType, bool &ch, Statement *s) override;
};

/**
 * TypedExp is a subclass of Unary, holding one subexpression and a Type.
 */
class TypedExp : public Unary {
	friend class XMLProgParser;

	Type       *type = nullptr;

public:
	// Constructor
	            TypedExp();
	// Constructor, subexpression
	            TypedExp(Exp *e1);
	// Constructor, type, and subexpression.
	// A rare const parameter allows the common case of providing a temporary,
	// e.g. foo = new TypedExp(Type(INTEGER), ...);
	            TypedExp(Type *ty, Exp *e1);
	// Copy constructor
	            TypedExp(const TypedExp &o);

	// Clone
	Exp        *clone() const override;

	// Compare
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator <<(const Exp &) const override;
	bool        operator *=(const Exp &) const override;


	void        print(std::ostream &os, bool html = false) const override;
	void        appendDot(std::ostream &os) const override;
	void        printx(int ind) const override;

	// Get and set the type
	virtual Type *getType() const { return type; }
	virtual void setType(Type *ty) { type = ty; }

	// polySimplify
	Exp        *polySimplify(bool &bMod) override;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;

	Type       *ascendType() override;
	void        descendType(Type *parentType, bool &ch, Statement *s) override;
};

/**
 * FlagDef is a subclass of Unary, and holds a list of parameters (in the
 * subexpression), and a pointer to an RTL.
 */
class FlagDef : public Unary {
	friend class XMLProgParser;

	RTL        *rtl;

public:
	            FlagDef(Exp *params, RTL *rtl);  // Constructor
	virtual    ~FlagDef();                       // Destructor
	void        appendDot(std::ostream &os) const override;
	RTL        *getRtl() const { return rtl; }
	void        setRtl(RTL *r) { rtl = r; }

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;
};

/**
 * RefExp is a subclass of Unary, holding an ordinary Exp pointer, and a
 * pointer to a defining statement (could be a phi assignment).  This is used
 * for subscripting SSA variables.
 *
 * \par Example
 * m[1000] becomes m[1000]{3} if defined at statement 3
 *
 * The integer is really a pointer to the defining statement, printed as the
 * statement number for compactness.
 */
class RefExp : public Unary {
	friend class XMLProgParser;

	Statement  *def = nullptr;  // The defining statement

protected:
	            RefExp() : Unary(opSubscript) { }
public:
	// Constructor with expression (e) and statement defining it (def)
	            RefExp(Exp *e, Statement *def);
	            //RefExp(Exp *e);
	            //RefExp(RefExp &o);
	Exp        *clone() const override;
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator *=(const Exp &) const override;

	void        print(std::ostream &os, bool html = false) const override;
	void        printx(int ind) const override;
	//int         getNumRefs() const override { return 1; }
	Statement  *getDef() const { return def; }  // Ugh was called getRef()
	Exp        *addSubscript(Statement *def) { this->def = def; return this; }
	void        setDef(Statement *def) { this->def = def; }
	Exp        *genConstraints(Exp *restrictTo) override;
	bool        references(Statement *s) { return def == s; }
	Exp        *polySimplify(bool &bMod) override;
	Exp        *match(const Exp *pattern) const override;
	bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const override;

	bool        isImplicitDef() const;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;

	Type       *ascendType() override;
	void        descendType(Type *parentType, bool &ch, Statement *s) override;
};

/**
 * Just a Terminal with a Type.  Used for type values in constraints.
 */
class TypeVal : public Terminal {
	friend class XMLProgParser;

	Type       *val;

public:
	            TypeVal(Type *ty);
	           ~TypeVal();

	virtual Type *getType() const { return val; }
	virtual void setType(Type *t) { val = t; }
	Exp        *clone() const override;
	bool        operator ==(const Exp &) const override;
	bool        operator < (const Exp &) const override;
	bool        operator *=(const Exp &) const override;
	void        print(std::ostream &os, bool html = false) const override;
	void        printx(int ind) const override;
	Exp        *genConstraints(Exp *restrictTo) override { assert(0); return nullptr; }  // Should not be constraining constraints
	//Exp        *match(const Exp *pattern) const override;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;
};

class Location : public Unary {
	friend class XMLProgParser;

protected:
	UserProc   *proc = nullptr;

	            Location(OPER op) : Unary(op) { }
public:
	// Constructor with ID, subexpression, and UserProc*
	            Location(OPER op, Exp *e, UserProc *proc);
	// Copy constructor
	            Location(const Location &o);
	// Custom constructor
	static  Location *regOf(int r)                                         { return new Location(opRegOf, new Const(r), nullptr); }
	static  Location *regOf(Exp *e)                                        { return new Location(opRegOf, e, nullptr); }
	static  Location *memOf(Exp *e, UserProc *p = nullptr)                 { return new Location(opMemOf, e, p); }
	static  Location *tempOf(Exp *e)                                       { return new Location(opTemp, e, nullptr); }
	static  Location *global(const char *nam, UserProc *p)                 { return new Location(opGlobal, new Const(nam), p); }
	static  Location *global(const std::string &nam, UserProc *p)          { return new Location(opGlobal, new Const(nam), p); }
	static  Location *local(const char *nam, UserProc *p)                  { return new Location(opLocal, new Const(nam), p); }
	static  Location *local(const std::string &nam, UserProc *p)           { return new Location(opLocal, new Const(nam), p); }
	static  Location *param(const char *nam, UserProc *p = nullptr)        { return new Location(opParam, new Const(nam), p); }
	static  Location *param(const std::string &nam, UserProc *p = nullptr) { return new Location(opParam, new Const(nam), p); }
	// Clone
	Exp        *clone() const override;

	void        setProc(UserProc *p) { proc = p; }
	UserProc   *getProc() const { return proc; }

	Exp        *polySimplify(bool &bMod) override;
	virtual void getDefinitions(LocationSet &defs) const;

	// Visitation
	bool        accept(ExpVisitor &) override;
	Exp        *accept(ExpModifier &) override;
	bool        match(const char *pattern, std::map<std::string, const Exp *> &bindings) const override;
};

#endif
