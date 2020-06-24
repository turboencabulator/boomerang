/**
 * \file
 * \brief Interface for the procedure classes
 *
 * The procedure classes are used to store information about variables in the
 * procedure such as parameters and locals.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2000-2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002-2006, Trent Waddington and Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef PROC_H
#define PROC_H

#include "boomerang.h"  // For USE_DOMINANCE_NUMS etc
#include "cfg.h"        // For cfg->simplify()
#include "dataflow.h"   // For class UseCollector
#include "exp.h"        // For lessExpStar
#include "statement.h"  // For embedded ReturnStatement pointer, etc
#include "type.h"

#include <ostream>
#include <list>
#include <map>
#include <set>
#include <string>

#include <cassert>

class BasicBlock;
class Cluster;
class HLLCode;
class Prog;
class Signature;
class SyntaxNode;
class UserProc;

/**
 * Interface for the procedure classes, which are used to store information
 * about variables in the procedure such as parameters and locals.
 */
class Proc {
	friend class XMLProgParser;

protected:
	                    Proc() = default;
public:
	                    Proc(Prog *prog, ADDRESS uNative, Signature *sig);

	virtual            ~Proc() = default;

	        const std::string &getName() const;
	        void        setName(const std::string &);

	        ADDRESS     getNativeAddress() const;
	        void        setNativeAddress(ADDRESS a);

	/**
	 * Get the program this procedure belongs to.
	 */
	        Prog       *getProg() const { return prog; }
	        void        setProg(Prog *p) { prog = p; }

	/**
	 * Returns a pointer to the Signature.
	 */
	        Signature  *getSignature() const { return signature; }
	        void        setSignature(Signature *sig) { signature = sig; }

	virtual void        renameParam(const std::string &, const std::string &);

	virtual bool        isNoReturn() const = 0;

	virtual Exp        *getProven(Exp *left) const = 0;
	virtual Exp        *getPremised(Exp *left) const = 0;
	virtual bool        isPreserved(Exp *e) const = 0;

	        void        setProvenTrue(Exp *fact);

	/**
	 * Get the callers.
	 *
	 * \note The callers will be in a random order (determined by memory
	 * allocation).
	 */
	        const std::set<CallStatement *> &getCallers() const { return callerSet; }

	/**
	 * Add to the set of callers.
	 */
	        void        addCaller(CallStatement *caller) { callerSet.insert(caller); }
	        void        addCallers(std::set<UserProc *> &callers);

	        void        removeParameter(Exp *e);
	virtual void        removeReturn(Exp *e);
	//virtual void        addReturn(Exp *e);

	        void        printDetailsXML() const;

	        Cluster    *getCluster() const { return cluster; }
	        void        setCluster(Cluster *c) { cluster = c; }

protected:
	        Prog       *prog = nullptr;  ///< Program containing this procedure.

	/**
	 * The formal signature of this procedure.  This information is
	 * determined either by the common.hs file (for a library function) or
	 * by analysis.
	 *
	 * \note This belongs in the CALL, because the same procedure can have
	 * different signatures if it happens to have varargs.  Temporarily
	 * here till it can be permanently moved.
	 */
	        Signature  *signature = nullptr;

	/** Persistent state */
	        ADDRESS     address = 0;              ///< Procedure's address.
	/// All the expressions that have been proven true. (Could perhaps do with a list of some that are proven false)
	// FIXME: shouldn't this be in UserProc, with logic associated with the signature doing the equivalent thing
	// for LibProcs?
	/// Proof the form r28 = r28 + 4 is stored as map from "r28" to "r28+4" (NOTE: no subscripts)
	        std::map<Exp *, Exp *, lessExpStar> provenTrue;
	// Cache of queries proven false (to save time)
	        //std::map<Exp *, Exp *, lessExpStar> provenFalse;
	// Premises for recursion group analysis. This is a preservation that is assumed true only for definitions by
	// calls reached in the proof. It also prevents infinite looping of this proof logic.
	        std::map<Exp *, Exp *, lessExpStar> recurPremises;

	        std::set<CallStatement *> callerSet;  ///< Set of callers (CallStatements that call this procedure).
	        Cluster    *cluster = nullptr;        ///< Cluster this procedure is contained within.
};

/**
 * Stores information for library procedures.  These are not decompiled.
 * Instead they serve as leaf nodes in the call graph and provide context for
 * calls from UserProcs.
 */
class LibProc : public Proc {
	friend class XMLProgParser;

protected:
	            LibProc() = default;
public:
	            LibProc(Prog *, const std::string &, ADDRESS);

	bool        isNoReturn() const override;

	Exp        *getProven(Exp *left) const override;
	Exp        *getPremised(Exp *left) const override { return nullptr; }
	bool        isPreserved(Exp *e) const override;

	void        getInternalStatements(StatementList &internal);
};

enum ProcStatus {
	PROC_UNDECODED,       ///< Has not even been decoded
	PROC_DECODED,         ///< Decoded, no attempt at decompiling
	PROC_SORTED,          ///< Decoded, and CFG has been sorted by address
	PROC_VISITED,         ///< Has been visited on the way down in decompile()
	PROC_INCYCLE,         ///< Is involved in cycles, has not completed early decompilation as yet
	PROC_PRESERVEDS,      ///< Has had preservation analysis done
	PROC_EARLYDONE,       ///< Has completed everything except the global analyses
	PROC_FINAL,           ///< Has had final decompilation
	//PROC_RETURNS,       ///< Has had returns intersected with all caller's defines
	PROC_CODE_GENERATED,  ///< Has had code generated
};

typedef std::set<UserProc *> ProcSet;
typedef std::list<UserProc *> ProcList;

/**
 * Stores information for user procedures.  These will be decompiled.
 */
class UserProc : public Proc {
	friend class XMLProgParser;

	/**
	 * The control flow graph.
	 */
	Cfg        *cfg = nullptr;

	/**
	 * The status of this user procedure.
	 *
	 * Status: undecoded .. final decompiled
	 */
	ProcStatus  status = PROC_UNDECODED;

	/**
	 * Somewhat DEPRECATED now. Eventually use the localTable.
	 *
	 * This map records the names and types for local variables.  It
	 * should be a subset of the symbolMap, which also stores parameters.
	 *
	 * It is a convenient place to store the types of locals after
	 * conversion from SSA form, since it is then difficult to access the
	 * definitions of locations.
	 *
	 * This map could be combined with symbolMap below, but beware of
	 * parameters (in symbols but not locals).
	 */
	std::map<std::string, Type *> locals;

	int         nextLocal = 0;  // Number of the next local. Can't use locals.size() because some get deleted
	int         nextParam = 0;  // Number for param1, param2, etc

	/**
	 * A map between machine dependent locations and their corresponding
	 * symbolic, machine independent representations.
	 *
	 * Example: m[r28{0} - 8] -> local5; this means that *after*
	 * transforming out of SSA form, any locations not specifically mapped
	 * otherwise (e.g. m[r28{0} - 8]{55} -> local6) will get this name.
	 *
	 * It is a *multi*map because one location can have several default
	 * names differentiated by type.  E.g. r24 -> eax for int,
	 * r24 -> eax_1 for float.
	 */
public:
	typedef std::multimap<Exp *, Exp *, lessExpStar> SymbolMap;
private:
	SymbolMap   symbolMap;

	/**
	 * The local "symbol table", which is aware of overlaps.
	 */
	DataIntervalMap localTable;

	/**
	 * Set of callees (Procedures that this procedure calls).
	 * Used for call graph, among other things.
	 */
	std::list<Proc *> calleeList;

	/**
	 * A collector for initial parameters (locations used before being
	 * defined).  Note that final parameters don't use this; it's only of
	 * use during group decompilation analysis (sorting out recursion).
	 */
	UseCollector col;

	/**
	 * The list of parameters, ordered and filtered.
	 *
	 * Note that a LocationList could be used, but then there would be
	 * nowhere to store the types (for DFA based TA).  The RHS is just
	 * ignored; the list is of ImplicitAssigns.
	 *
	 * DESIGN ISSUE: it would be nice for the parameters' implicit
	 * assignments to be the sole definitions, i.e. not need other
	 * implicit assignments for these.  But the targets of RefExp's are
	 * not expected to change address, so they are not suitable at present
	 * (since the addresses regularly get changed as the parameters get
	 * recreated).
	 */
	StatementList parameters;

	/**
	 * The set of address-escaped locals and parameters.  If in this list,
	 * they should not be propagated.
	 */
	LocationSet addressEscapedVars;

	// The modifieds for the procedure are now stored in the return statement

	/**
	 * DataFlow object.  Holds information relevant to transforming to and
	 * from SSA form.
	 */
	DataFlow    df;

	/**
	 * Current statement number.  Makes it easier to split decompile()
	 * into smaller pieces.
	 */
	int         stmtNumber;

	/**
	 * Pointer to a set of procedures involved in a recursion group.
	 *
	 * \note Each procedure in the cycle points to the same set!
	 * However, there can be several separate cycles.  E.g. in
	 * test/source/recursion.c, there is a cycle with f and g, while
	 * another is being built up (it only has c, d, and e at the point
	 * where the f-g cycle is found).
	 */
	ProcSet    *cycleGrp = nullptr;

protected:
	            UserProc();
public:
	            UserProc(Prog *, const std::string &, ADDRESS);
	           ~UserProc() override;

	void        setDecoded();
	void        unDecode();

	/**
	 * Returns a pointer to the CFG object.
	 */
	Cfg        *getCFG() const { return cfg; }
protected:
	void        setCFG(Cfg *c) { cfg = c; }
public:

	/**
	 * Returns a pointer to the DataFlow object.
	 */
	DataFlow   *getDataFlow() { return &df; }

	//void        deleteCFG();

	bool        isNoReturn() const override;

	SyntaxNode *getAST() const;
	void        printAST(SyntaxNode *a = nullptr) const;

	/**
	 * Returns whether or not this procedure can be decoded (i.e. has it already been decoded).
	 */
	bool        isDecoded() const { return status >= PROC_DECODED; }
	bool        isDecompiled() const { return status >= PROC_FINAL; }
	bool        isEarlyRecursive() const { return cycleGrp && status <= PROC_INCYCLE; }
	bool        doesRecurseTo(UserProc *p) const { return cycleGrp && cycleGrp->count(p); }

	bool        isSorted() const { return status >= PROC_SORTED; }
	void        setSorted() { setStatus(PROC_SORTED); }

	ProcStatus  getStatus() const { return status; }
	void        setStatus(ProcStatus s);

	void        generateCode(HLLCode *hll);

	void        print(std::ostream &out, bool html = false) const;
	void        printParams(std::ostream &out, bool html = false) const;
	std::string prints() const;
	void        printDFG();
	void        printSymbolMap(std::ostream &out, bool html = false) const;
	void        testSymbolMap();
	void        dumpLocals(std::ostream &os, bool html = false) const;

	/// simplify the statements in this proc
	void        simplify() { cfg->simplify(); }

	ProcSet    *decompile(ProcList *path, int &indent);
	void        initialiseDecompile();
	/// Prepare for preservation analysis only.
	void        prePresDecompile();
	void        earlyDecompile();
	ProcSet    *middleDecompile(ProcList *path, int indent);
	void        recursionGroupAnalysis(ProcList *path, int indent);
	void        typeAnalysis();
	/// Inserting casts as needed (for this procedure)
	void        insertCasts();
	//void        rangeAnalysis();
	//void        logSuspectMemoryDefs();
	// Split the set of cycle-associated procs into individual subcycles.
	//void        findSubCycles(CycleList &path, CycleSet &cs, CycleSetSet &sset);
	bool        inductivePreservation(UserProc *topOfCycle);
	void        markAsNonChildless(ProcSet *cs);
	void        updateCalls();
	void        branchAnalysis();
	void        fixUglyBranches();
	// Place the phi functions
	void        placePhiFunctions() { df.placePhiFunctions(this); }

	//void        propagateAtDepth(int depth);
	bool        doRenameBlockVars(int pass, bool clearStacks = false);
	//int         getMaxDepth() { return maxDepth; }  // FIXME: needed?
	bool        canRename(Exp *e) { return df.canRename(e, this); }

	//Statement  *getStmtAtLex(unsigned int begin, unsigned int end);

	void        initStatements();
	void        numberStatements();
	bool        nameStackLocations();
	void        removeRedundantPhis();
	void        findPreserveds();
	void        findSpPreservation();
	void        removeSpAssignsIfPossible();
	void        removeMatchingAssignsIfPossible(Exp *e);
	void        updateReturnTypes();
	void        fixCallAndPhiRefs(int d);           ///< Perform call and phi statement bypassing at depth d
	void        fixCallAndPhiRefs();
	/// Helper function for fixCallAndPhiRefs
	void        fixRefs(int n, int depth, std::map<Exp *, Exp *, lessExpStar> &pres, StatementList &removes);
	void        initialParameters();
	void        mapLocalsAndParams();
	void        findFinalParameters();
	int         nextParamNum() { return ++nextParam; }
	void        addParameter(Exp *e, Type *ty);
	void        insertParameter(Exp *e, Type *ty);
	//void        addNewReturns(int depth);
	void        updateArguments();
	void        updateCallDefines();
	void        reverseStrengthReduction();
	//void        trimParameters(int depth = -1);
	void        processFloatConstants();
	//void        mapExpressionsToParameters();       ///< must be in SSA form
	void        mapExpressionsToLocals(bool lastPass = false);
	void        addParameterSymbols();
	bool        isLocal(Exp *e) const;
	bool        isLocalOrParam(Exp *e) const;
	bool        isLocalOrParamPattern(Exp *e) const;
	bool        existsLocal(const std::string &) const;
	bool        isAddressEscapedVar(Exp *e) const { return addressEscapedVars.exists(e); }
	bool        isPropagatable(Exp *e) const;

	void        assignProcsToCalls();

	void        finalSimplify();

	void        eliminateDuplicateArgs();

private:
	void        searchRegularLocals(OPER minusOrPlus, bool lastPass, int sp, StatementList &stmts);
public:
	bool        removeNullStatements();
	bool        removeDeadStatements();
	typedef std::map<Statement *, int> RefCounter;
	void        countRefs(RefCounter &refCounts);
	void        remUnusedStmtEtc();
	void        remUnusedStmtEtc(RefCounter &refCounts /* , int depth*/);
	void        removeUnusedLocals();
	void        mapTempsToLocals();
	void        removeCallLiveness();
	bool        propagateAndRemoveStatements();
	bool        propagateStatements(bool &convert, int pass);
	void        findLiveAtDomPhi(LocationSet &usedByDomPhi);
#if USE_DOMINANCE_NUMS
	void        setDominanceNumbers();
#endif
	void        propagateToCollector();
	void        clearUses();
	//void        clearRanges();
	//int         findMaxDepth();  ///< Find max memory nesting depth.

	void        fromSSAform();
	void        findPhiUnites(ConnectionGraph &pu);
	void        insertAssignAfter(Statement *s, Exp *left, Exp *right);
	void        removeSubscriptsFromSymbols();
	void        removeSubscriptsFromParameters();
	void        insertStatementAfter(Statement *s, Statement *a);
	void        nameParameterPhis();
	void        mapParameters();

	void        conTypeAnalysis();
	void        dfaTypeAnalysis();
	bool        ellipsisProcessing();

	// For the final pass of removing returns that are never used
	//typedef std::map<UserProc *, std::set<Exp *, lessExpStar> > ReturnCounter;
	bool        doesParamChainToCall(Exp *param, UserProc *p, ProcSet *visited);
	bool        isRetNonFakeUsed(CallStatement *c, Exp *loc, UserProc *p, ProcSet *visited);
	bool        removeRedundantParameters();
	bool        removeRedundantReturns(std::set<UserProc *> &removeRetSet);
	bool        checkForGainfulUse(Exp *e, ProcSet &visited);
	void        updateForUseChange(std::set<UserProc *> &removeRetSet);
	//void        countUsedReturns(ReturnCounter &rc);
	//void        doCountReturns(Statement *def, ReturnCounter &rc, Exp *loc);

	bool        prove(Exp *query, bool conditional = false);
	bool        prover(Exp *query, std::set<PhiAssign *> &lastPhis, std::map<PhiAssign *, Exp *> &cache, Exp *original, PhiAssign *lastPhi = nullptr);

	void        promoteSignature();

	void        getStatements(StatementList &stmts);

	void        removeReturn(Exp *e) override;
	//void        addReturn(Exp *e) override;

	void        removeStatement(Statement *stmt);

	// /// inline constants / decode function pointer constants
	//bool        processConstants();
	//void        processTypes();

	bool        searchAll(Exp *search, std::list<Exp *> &result);

	void        getDefinitions(LocationSet &defs) const;
	void        addImplicitAssigns();
	void        makeSymbolsImplicit();
	void        makeParamsImplicit();
	StatementList &getParameters() { return parameters; }
	StatementList &getModifieds() { return theReturnStatement->getModifieds(); }

	Exp        *getSymbolExp(Exp *le, Type *ty = nullptr, bool lastPass = false);

	std::string newLocalName(Exp *e);
	Exp        *newLocal(Type *, Exp *);
	Exp        *newLocal(Type *, Exp *, const std::string &);
	void        addLocal(Type *, const std::string &, Exp *);
	Type       *getLocalType(const std::string &) const;
	void        setLocalType(const std::string &, Type *);

	Type       *getParamType(const std::string &) const;

	Exp        *expFromSymbol(const std::string &) const;
	void        setExpSymbol(const std::string &, Exp *, Type *);
	void        mapSymbolTo(Exp *from, Exp *to);
	void        mapSymbolToRepl(Exp *from, Exp *oldTo, Exp *newTo);
	void        removeSymbolMapping(Exp *from, Exp *to);
	Exp        *getSymbolFor(Exp *e, Type *ty) const;
	const char *lookupSym(Exp *e, Type *ty) const;
	const char *lookupSymFromRef(RefExp *r) const;
	const char *lookupSymFromRefAny(RefExp *r) const;
	const char *lookupParam(Exp *e) const;
	void        checkLocalFor(RefExp *r);
	Type       *getTypeForLocation(Exp *e) const;
	const char *findLocal(Exp *e, Type *ty) const;
	const char *findLocalFromRef(RefExp *r) const;
	const char *findFirstSymbol(Exp *e) const;
	void        renameLocal(const std::string &, const std::string &);
	void        renameParam(const std::string &, const std::string &) override;

	const char *getRegName(Exp *r) const;
	void        setParamType(int, Type *);

	BasicBlock *getEntryBB() const;
	void        setEntryBB();

	/**
	 * Get the callees.
	 */
	const std::list<Proc *> &getCallees() const { return calleeList; }
	void        addCallee(Proc *callee);

	bool        containsAddr(ADDRESS uAddr) const;

	/**
	 * Change BB containing this statement from a COMPCALL to a CALL.
	 */
	void        undoComputedBB(Statement *stmt) { cfg->undoComputedBB(stmt); }

	/*
	 * Return true if this proc uses the special aggregate pointer as the
	 * first parameter
	 */
	//virtual bool isAggregateUsed() { return aggregateUsed; }

	Exp        *getProven(Exp *left) const override;
	Exp        *getPremised(Exp *left) const override;
	// Set a location as a new premise, i.e. assume e=e
	void        setPremise(Exp *e) { e = e->clone(); recurPremises[e] = e; }
	void        killPremise(Exp *e) { recurPremises.erase(e); }
	bool        isPreserved(Exp *e) const override;

	void        printDecodedXML() const;
	void        printAnalysedXML() const;
	void        printSSAXML() const;
	void        printXML();
	void        printUseGraph();


	bool        searchAndReplace(Exp *search, Exp *replace);

	void        castConst(int num, Type *ty);

	/**
	 * Add a location to the UseCollector; this means this location is
	 * used before defined, and hence is an *initial* parameter.  Note
	 * that final parameters don't use this information; it's only for
	 * handling recursion.
	 */
	void        useBeforeDefine(Exp *loc) { col.insert(loc); }

	void        processDecodedICTs();

private:
	/**
	 * We ensure that there is only one return statement now.  See code in
	 * frontend/frontend.cpp handling case STMT_RET.  If no return
	 * statement, this will be null.
	 */
	ReturnStatement *theReturnStatement = nullptr;
	int         DFGcount = 0;
public:
	void        setTheReturnStatement(ReturnStatement *s) { theReturnStatement = s; }
	ReturnStatement *getTheReturnStatement() const { return theReturnStatement; }
	bool        filterReturns(Exp *e);
	bool        filterParams(Exp *e);

	//void        setImplicitRef(Statement *, Exp *, Type *);
};

std::ostream & operator <<(std::ostream &, const UserProc &);

#endif
