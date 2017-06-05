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

/*==============================================================================
 * Procedure class.
 *============================================================================*/
/// Interface for the procedure classes, which are used to store information about variables in the
/// procedure such as parameters and locals.
class Proc {
public:
	/**
	 * Constructor with name, native address and optional bLib.
	 */
	                    Proc(Prog *prog, ADDRESS uNative, Signature *sig);

	virtual            ~Proc();

	/**
	 * Gets name of this procedure.
	 */
	        const char *getName();

	/**
	 * Gets sets the name of this procedure.
	 */
	        void        setName(const char *nam);

	/**
	 * Get the native address.
	 */
	        ADDRESS     getNativeAddress();

	/**
	 * Set the native address
	 */
	        void        setNativeAddress(ADDRESS a);

	/**
	 * Get the program this procedure belongs to.
	 */
	        Prog       *getProg() { return prog; }
	        void        setProg(Prog *p) { prog = p; }

	/**
	 * Get/Set the first procedure that calls this procedure (or null for main/start).
	 */
	        Proc       *getFirstCaller();
	        void        setFirstCaller(Proc *p) { if (m_firstCaller == NULL) m_firstCaller = p; }

	/**
	 * Returns a pointer to the Signature
	 */
	        Signature  *getSignature() { return signature; }
	        void        setSignature(Signature *sig) { signature = sig; }

	virtual void        renameParam(const char *oldName, const char *newName);

	/*
	 * Prints this procedure to an output stream.
	 */
	//virtual std::ostream &put(std::ostream &os) = 0;

	/**
	 * Modify actuals so that it is now the list of locations that must
	 * be passed to this procedure. The modification will be to either add
	 * dummy locations to actuals, delete from actuals, or leave it
	 * unchanged.
	 * Add "dummy" params: this will be required when there are
	 *   less live outs at a call site than the number of parameters
	 *   expected by the procedure called. This will be a result of
	 *   one of two things:
	 *   i) a value returned by a preceeding call is used as a
	 *      parameter and as such is not detected as defined by the
	 *      procedure. E.g.:
	 *
	 *         foo(bar(x));
	 *
	 *      Here, the result of bar(x) is used as the first and only
	 *      parameter to foo. On some architectures (such as SPARC),
	 *      the location used as the first parameter (e.g. %o0) is
	 *      also the location in which a value is returned. So, the
	 *      call to bar defines this location implicitly as shown in
	 *      the following SPARC assembly that may be generated by from
	 *      the above code:
	 *
	 *          mov   x, %o0
	 *          call  bar
	 *          nop
	 *          call  foo
	 *
	 *     As can be seen, there is no definition of %o0 after the
	 *     call to bar and before the call to foo. Adding the integer
	 *     return location is therefore a good guess for the dummy
	 *     location to add (but may occasionally be wrong).
	 *
	 *  ii) uninitialised variables are used as parameters to a call
	 *
	 *  Note that both of these situations can only occur on
	 *  architectures such as SPARC that use registers for parameter
	 *  passing. Stack parameters must always be pushed so that the
	 *  callee doesn't access the caller's non-parameter portion of
	 *  stack.
	 *
	 * This used to be a virtual function, implemented differenty for
	 * LibProcs and for UserProcs. But in fact, both need the exact same
	 * treatment; the only difference is how the local member "parameters"
	 * is set (from common.hs in the case of LibProc objects, or from analysis
	 * in the case of UserProcs).
	 */
	        void        matchParams(std::list<Exp *> &actuals, UserProc &caller);

	/**
	 * Get a list of types to cast a given list of actual parameters to
	 */
	        std::list<Type> *getParamTypeList(const std::list<Exp *> &actuals);

	/**
	 * Return true if this is a library proc
	 */
	virtual bool        isLib() { return false; }

	/**
	 * Return true if this procedure doesn't return
	 */
	virtual bool        isNoReturn() = 0;

	/**
	 * OutPut operator for a Proc object.
	 */
	friend  std::ostream &operator <<(std::ostream &os, Proc &proc);

	virtual Exp        *getProven(Exp *left) = 0;    // Get the RHS, if any, that is proven for left
	virtual Exp        *getPremised(Exp *left) = 0;  // Get the RHS, if any, that is premised for left
	virtual bool        isPreserved(Exp *e) = 0;     ///< Return whether e is preserved by this proc

	/// Set an equation as proven. Useful for some sorts of testing
	        void        setProvenTrue(Exp *fact);

	/**
	 * Get the callers
	 * Note: the callers will be in a random order (determined by memory allocation)
	 */
	        std::set<CallStatement *> &getCallers() { return callerSet; }

	/**
	 * Add to the set of callers
	 */
	        void        addCaller(CallStatement *caller) { callerSet.insert(caller); }

	/**
	 * Add to a set of caller Procs
	 */
	        void        addCallers(std::set<UserProc *> &callers);

	        void        removeParameter(Exp *e);
	virtual void        removeReturn(Exp *e);
	//virtual void        addReturn(Exp *e);
	//        void        sortParameters();

	virtual void        printCallGraphXML(std::ostream &os, int depth, bool recurse = true);
	        void        printDetailsXML();
	        void        clearVisited() { visited = false; }
	        bool        isVisited() { return visited; }

	        Cluster    *getCluster() { return cluster; }
	        void        setCluster(Cluster *c) { cluster = c; }

protected:

	        bool        visited = false;  ///< For printCallGraphXML

	        Prog       *prog = NULL;  ///< Program containing this procedure.

	/**
	 * The formal signature of this procedure. This information is determined
	 * either by the common.hs file (for a library function) or by analysis.
	 *
	 * NOTE: This belongs in the CALL, because the same procedure can have different signatures if it happens to
	 * have varargs. Temporarily here till it can be permanently moved.
	 */
	        Signature  *signature = NULL;

	/** Persistent state */
	        ADDRESS     address = 0;           ///< Procedure's address.
	        Proc       *m_firstCaller = NULL;  ///< first procedure to call this procedure.
	        ADDRESS     m_firstCallerAddr = 0; ///< can only be used once.
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
	        Cluster    *cluster = NULL;           ///< Cluster this procedure is contained within.

	friend class XMLProgParser;
	                    Proc() { }

};

/*==============================================================================
 * LibProc class.
 *============================================================================*/
class LibProc : public Proc {
public:
	                    LibProc(Prog *prog, std::string &name, ADDRESS address);
	virtual            ~LibProc();

	/**
	 * Return true, since is a library proc
	 */
	        bool        isLib() { return true; }

	virtual bool        isNoReturn();

	virtual Exp        *getProven(Exp *left);                    // Get the RHS that is proven for left
	virtual Exp        *getPremised(Exp *left) { return NULL; }  // Get the RHS that is premised for left
	virtual bool        isPreserved(Exp *e);                     ///< Return whether e is preserved by this proc

	/*
	 * Prints this procedure to an output stream.
	 */
	        //std::ostream &put(std::ostream &os);

	        void        getInternalStatements(StatementList &internal);
protected:

	friend class XMLProgParser;
	                    LibProc() { }
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

/*==============================================================================
 * UserProc class.
 *============================================================================*/

class UserProc : public Proc {
	/**
	 * The control flow graph.
	 */
	        Cfg        *cfg = NULL;

	/**
	 * The status of this user procedure.
	 * Status: undecoded .. final decompiled
	 */
	        ProcStatus  status = PROC_UNDECODED;

	/*
	 * Somewhat DEPRECATED now. Eventually use the localTable.
	 * This map records the names and types for local variables. It should be a subset of the symbolMap, which also
	 * stores parameters.
	 * It is a convenient place to store the types of locals after
	 * conversion from SSA form, since it is then difficult to access the definitions of locations.
	 * This map could be combined with symbolMap below, but beware of parameters (in symbols but not locals)
	 */
	        std::map<std::string, Type *> locals;

	        int         nextLocal = 0;  // Number of the next local. Can't use locals.size() because some get deleted
	        int         nextParam = 0;  // Number for param1, param2, etc

	/**
	 * A map between machine dependent locations and their corresponding symbolic, machine independent
	 * representations.  Example: m[r28{0} - 8] -> local5; this means that *after* transforming out of SSA
	 * form, any locations not specifically mapped otherwise (e.g. m[r28{0} - 8]{55} -> local6) will get this
	 * name.
	 * It is a *multi*map because one location can have several default names differentiated by type.
	 * E.g. r24 -> eax for int, r24 -> eax_1 for float
	 */
public:
	        typedef std::multimap<Exp *, Exp *, lessExpStar> SymbolMap;
private:
	        SymbolMap   symbolMap;

	/**
	 * The local "symbol table", which is aware of overlaps
	 */
	        DataIntervalMap localTable;

	/**
	 * Set of callees (Procedures that this procedure calls). Used for call graph, among other things
	 */
	        std::list<Proc *> calleeList;

	/**
	 * A collector for initial parameters (locations used before being defined).  Note that final parameters don't
	 * use this; it's only of use during group decompilation analysis (sorting out recursion)
	 */
	        UseCollector col;

	/**
	 * The list of parameters, ordered and filtered.
	 * Note that a LocationList could be used, but then there would be nowhere to store the types (for DFA based TA)
	 * The RHS is just ignored; the list is of ImplicitAssigns.
	 * DESIGN ISSUE: it would be nice for the parameters' implicit assignments to be the sole definitions, i.e. not
	 * need other implicit assignments for these. But the targets of RefExp's are not expected to change address,
	 * so they are not suitable at present (since the addresses regularly get changed as the parameters get
	 * recreated).
	 */
	        StatementList parameters;

	/**
	 * The set of address-escaped locals and parameters. If in this list, they should not be propagated
	 */
	        LocationSet addressEscapedVars;

	// The modifieds for the procedure are now stored in the return statement

	/**
	 * DataFlow object. Holds information relevant to transforming to and from SSA form.
	 */
	        DataFlow    df;

	/**
	 * Current statement number. Makes it easier to split decompile() into smaller pieces.
	 */
	        int         stmtNumber;

	/**
	 * Pointer to a set of procedures involved in a recursion group.
	 * NOTE: Each procedure in the cycle points to the same set! However, there can be several separate cycles.
	 * E.g. in test/source/recursion.c, there is a cycle with f and g, while another is being built up (it only
	 * has c, d, and e at the point where the f-g cycle is found).
	 */
	        ProcSet    *cycleGrp = NULL;

	/**
	 * A map of stack locations (negative values) to types.  This is currently
	 * PENTIUM specific and is computed from range information.
	 */
	        std::map<int, Type *> stackMap;

	/// function to do safe adding.
	        void addToStackMap(int c, Type *ty);

public:

	                    UserProc(Prog *prog, std::string &name, ADDRESS address);
	virtual            ~UserProc();

	/**
	 * Records that this procedure has been decoded.
	 */
	        void        setDecoded();

	/**
	 * Removes the decoded bit and throws away all the current information about this procedure.
	 */
	        void        unDecode();

	/**
	 * Returns a pointer to the CFG object.
	 */
	        Cfg        *getCFG() { return cfg; }

	/**
	 * Returns a pointer to the DataFlow object.
	 */
	        DataFlow   *getDataFlow() { return &df; }

	/**
	 * Deletes the whole CFG and all the RTLs and Exps associated with it. Also nulls the internal cfg
	 * pointer (to prevent strange errors)
	 */
	        void        deleteCFG();

	virtual bool        isNoReturn();

	/**
	 * Returns an abstract syntax tree for the procedure in the internal representation. This function actually
	 * _calculates_ * this value and is expected to do so expensively.
	 */
	        SyntaxNode *getAST();
	// print it to a file
	        void        printAST(SyntaxNode *a = NULL);

	/**
	 * Returns whether or not this procedure can be decoded (i.e. has it already been decoded).
	 */
	        bool        isDecoded() { return status >= PROC_DECODED; }
	        bool        isDecompiled() { return status >= PROC_FINAL; }
	        bool        isEarlyRecursive() { return cycleGrp != NULL && status <= PROC_INCYCLE; }
	        bool        doesRecurseTo(UserProc *p) { return cycleGrp && cycleGrp->find(p) != cycleGrp->end(); }

	        bool        isSorted() { return status >= PROC_SORTED; }
	        void        setSorted() { setStatus(PROC_SORTED); }

	        ProcStatus  getStatus() { return status; }
	        void        setStatus(ProcStatus s);

	/// code generation
	        void        generateCode(HLLCode *hll);

	/// print this proc, mainly for debugging
	        void        print(std::ostream &out, bool html = false);
	        void        printParams(std::ostream &out, bool html = false);
	        char       *prints();
	        void        dump();
	        void        printToLog();
	        void        printDFG();
	        void        printSymbolMap(std::ostream &out, bool html = false);  ///< Print just the symbol map
	        void        dumpSymbolMap();   ///< For debugging
	        void        dumpSymbolMapx();  ///< For debugging
	        void        testSymbolMap();   ///< For debugging
	        void        dumpLocals(std::ostream &os, bool html = false);
	        void        dumpLocals();

	/// simplify the statements in this proc
	        void        simplify() { cfg->simplify(); }

	// simple windows mode decompile
	        void        windowsModeDecompile();

	/// Begin the decompile process at this procedure. path is a list of pointers to procedures, representing the
	/// path from the current entry point to the current procedure in the call graph. Pass an empty set at the top
	/// level.  indent is the indentation level; pass 0 at the top level
	        ProcSet    *decompile(ProcList *path, int &indent);
	/// Initialise decompile: sort CFG, number statements, dominator tree, etc.
	        void        initialiseDecompile();
	/// Prepare for preservation analysis only.
	        void        prePresDecompile();
	/// Early decompile: Place phi functions, number statements, first rename, propagation: ready for preserveds.
	        void        earlyDecompile();
	/// Middle decompile: All the decompilation from preservation up to but not including removing unused
	/// statements. Returns the cycle set from the recursive call to decompile()
	        ProcSet    *middleDecompile(ProcList *path, int indent);
	/// Analyse the whole group of procedures for conditional preserveds, and update till no change.
	/// Also finalise the whole group.
	        void        recursionGroupAnalysis(ProcList *path, int indent);
	/// Global type analysis (for this procedure).
	        void        typeAnalysis();
	/// Inserting casts as needed (for this procedure)
	        void        insertCasts();
	// Range analysis (for this procedure).
	        void        rangeAnalysis();
	// Detect and log possible buffer overflows
	        void        logSuspectMemoryDefs();
	// Split the set of cycle-associated procs into individual subcycles.
	        //void        findSubCycles(CycleList &path, CycleSet &cs, CycleSetSet &sset);
	// The inductive preservation analysis.
	        bool        inductivePreservation(UserProc *topOfCycle);
	// Mark calls involved in the recursion cycle as non childless (each child has had middleDecompile called on
	// it now).
	        void        markAsNonChildless(ProcSet *cs);
	// Update the defines and arguments in calls.
	        void        updateCalls();
	// Look for short circuit branching
	        void        branchAnalysis();
	// Fix any ugly branch statements (from propagating too much)
	        void        fixUglyBranches();
	// Place the phi functions
	        void        placePhiFunctions() { df.placePhiFunctions(this); }

	        //void        propagateAtDepth(int depth);
	// Rename block variables, with log if verbose. Return true if a change
	        bool        doRenameBlockVars(int pass, bool clearStacks = false);
	        //int         getMaxDepth() { return maxDepth; }  // FIXME: needed?
	        bool        canRename(Exp *e) { return df.canRename(e, this); }

	        Statement  *getStmtAtLex(unsigned int begin, unsigned int end);

	/**
	 * All the decompile stuff except propagation, DFA repair, and null/unused statement removal.
	 * \todo Check if this function is used, and remove it if it isn't
	 */
	        void        complete();

	/// Initialise the statements, e.g. proc, bb pointers
	        void        initStatements();
	        void        numberStatements();
	        bool        nameStackLocations();
	        void        removeRedundantPhis();
	        void        findPreserveds();                   ///< Was trimReturns()
	        void        findSpPreservation();               ///< Preservations only for the stack pointer
	        void        removeSpAssignsIfPossible();
	        void        removeMatchingAssignsIfPossible(Exp *e);
	        void        updateReturnTypes();
	        void        fixCallAndPhiRefs(int d);           ///< Perform call and phi statement bypassing at depth d
	        void        fixCallAndPhiRefs();                ///< Perform call and phi statement bypassing at all depths
	/// Helper function for fixCallAndPhiRefs
	        void        fixRefs(int n, int depth, std::map<Exp *, Exp *, lessExpStar> &pres, StatementList &removes);
	        void        initialParameters();                ///< Get initial parameters based on proc's use collector
	        void        mapLocalsAndParams();               ///< Map expressions to locals and initial parameters
	        void        findFinalParameters();
	        int         nextParamNum() { return ++nextParam; }
	        void        addParameter(Exp *e, Type *ty);     ///< Add parameter to signature
	        void        insertParameter(Exp *e, Type *ty);  ///< Insert into parameters list correctly sorted
	        //void        addNewReturns(int depth);
	        void        updateArguments();                  ///< Update the arguments in calls
	        void        updateCallDefines();                ///< Update the defines in calls
	        void        reverseStrengthReduction();
	/// Trim parameters. If depth not given or == -1, perform at all depths
	        void        trimParameters(int depth = -1);
	        void        processFloatConstants();
	        //void        mapExpressionsToParameters();       ///< must be in SSA form
	        void        mapExpressionsToLocals(bool lastPass = false);
	        void        addParameterSymbols();
	        bool        isLocal(Exp *e);                    ///< True if e represents a stack local variable
	        bool        isLocalOrParam(Exp *e);             ///< True if e represents a stack local or stack param
	        bool        isLocalOrParamPattern(Exp *e);      ///< True if e could represent a stack local or stack param
	        bool        existsLocal(const char *name);      ///< True if a local exists with name \a name
	        bool        isAddressEscapedVar(Exp *e) { return addressEscapedVars.exists(e); }
	        bool        isPropagatable(Exp *e);             ///< True if e can be propagated

	/// find the procs the calls point to
	        void        assignProcsToCalls();

	/// perform final simplifications
	        void        finalSimplify();

	// eliminate duplicate arguments
	        void        eliminateDuplicateArgs();

private:
	        void        searchRegularLocals(OPER minusOrPlus, bool lastPass, int sp, StatementList &stmts);
public:
	        bool        removeNullStatements();
	        bool        removeDeadStatements();
	typedef std::map<Statement *, int> RefCounter;
	        void        countRefs(RefCounter &refCounts);
	/// Remove unused statements.
	        void        remUnusedStmtEtc();
	        void        remUnusedStmtEtc(RefCounter &refCounts /* , int depth*/);
	        void        removeUnusedLocals();
	        void        mapTempsToLocals();
	        void        removeCallLiveness();  // Remove all liveness info in UseCollectors in calls
	        bool        propagateAndRemoveStatements();
	/// Propagate statemtents; return true if change; set convert if an indirect call is converted to direct
	/// (else clear)
	        bool        propagateStatements(bool &convert, int pass);
	        void        findLiveAtDomPhi(LocationSet &usedByDomPhi);
#if USE_DOMINANCE_NUMS
	        void        setDominanceNumbers();
#endif
	        void        propagateToCollector();
	        void        clearUses();  ///< Clear the useCollectors (in this Proc, and all calls).
	        void        clearRanges();
	        //int         findMaxDepth();  ///< Find max memory nesting depth.

	        void        fromSSAform();
	        void        findPhiUnites(ConnectionGraph &pu);  // Find the locations united by Phi-functions
	        void        insertAssignAfter(Statement *s, Exp *left, Exp *right);
	        void        removeSubscriptsFromSymbols();
	        void        removeSubscriptsFromParameters();
	//// Insert statement \a a after statement \a s.
	        void        insertStatementAfter(Statement *s, Statement *a);
	// Add a mapping for the destinations of phi functions that have one argument that is a parameter
	        void        nameParameterPhis();
	        void        mapParameters();

	        void        conTypeAnalysis();
	        void        dfaTypeAnalysis();
	/// Trim parameters to procedure calls with ellipsis (...). Also add types for ellipsis parameters, if any
	/// Returns true if any signature types so added.
	        bool        ellipsisProcessing();

	// For the final pass of removing returns that are never used
	//typedef std::map<UserProc *, std::set<Exp *, lessExpStar> > ReturnCounter;
	// Used for checking for unused parameters
	        bool        doesParamChainToCall(Exp *param, UserProc *p, ProcSet *visited);
	        bool        isRetNonFakeUsed(CallStatement *c, Exp *loc, UserProc *p, ProcSet *visited);
	// Remove redundant parameters. Return true if remove any
	        bool        removeRedundantParameters();
	/// Remove any returns that are not used by any callers
	/// return true if any returns are removed
	        bool        removeRedundantReturns(std::set<UserProc *> &removeRetSet);
	/// Reurn true if location e is used gainfully in this procedure. visited is a set of UserProcs already
	/// visited.
	        bool        checkForGainfulUse(Exp *e, ProcSet &visited);
	/// Update parameters and call livenesses to take into account the changes causes by removing a return from this
	/// procedure, or a callee's parameter (which affects this procedure's arguments, which are also uses).
	        void        updateForUseChange(std::set<UserProc *> &removeRetSet);
	        //void        countUsedReturns(ReturnCounter &rc);
	        //void        doCountReturns(Statement *def, ReturnCounter &rc, Exp *loc);

	/// returns true if the prover is working right now
	        bool        canProveNow();
	/// prove any arbitary property of this procedure. If conditional is true, do not save the result, as it may
	/// be conditional on premises stored in other procedures
	        bool        prove(Exp *query, bool conditional = false);
	/// helper function, should be private
	        bool        prover(Exp *query, std::set<PhiAssign *> &lastPhis, std::map<PhiAssign *, Exp *> &cache, Exp *original, PhiAssign *lastPhi = NULL);

	/// promote the signature if possible
	        void        promoteSignature();

	/// get all the statements
	        void        getStatements(StatementList &stmts);

	virtual void        removeReturn(Exp *e);
	//virtual void        addReturn(Exp *e);

	/// remove a statement
	        void        removeStatement(Statement *stmt);

	// /// inline constants / decode function pointer constants
	        //bool        processConstants();
	        //void        processTypes();

	        bool        searchAll(Exp *search, std::list<Exp *> &result);

	        void        getDefinitions(LocationSet &defs);
	        void        addImplicitAssigns();
	        void        makeSymbolsImplicit();
	        void        makeParamsImplicit();
	        StatementList &getParameters() { return parameters; }
	        StatementList &getModifieds() { return theReturnStatement->getModifieds(); }

private:
	/**
	 * Find a pointer to the Exp* representing the given var
	 * Used by the above 2
	 */
	        Exp       **findVarEntry(int idx);

	/**
	 * A special pass to check the sizes of memory that is about to be converted into a var, ensuring that the
	 * largest size used in the proc is used for all references (and it's declared that size).
	 */
	        void        checkMemSizes();

	/**
	 * Implement the above for one given Exp*
	 */
	        void        checkMemSize(Exp *e);

public:
	/**
	 * Return an expression that is equivilent to e in terms of local variables.  Creates new locals as needed.
	 */
	        Exp        *getSymbolExp(Exp *le, Type *ty = NULL, bool lastPass = false);


	/**
	 * Given a machine dependent location, return a generated symbolic representation for it.
	 */
	        void        toSymbolic(TypedExp *loc, TypedExp *result, bool local = true);

	/*
	 * Return a string for a new local suitable for e
	 */
	        const char *newLocalName(Exp *e);

	/**
	 * Return the next available local variable; make it the given type. Note: was returning TypedExp*.
	 * If nam is non null, use that name
	 */
	        Exp        *newLocal(Type *ty, Exp *e, const char *nam = NULL);

	/**
	 * Add a new local supplying all needed information.
	 */
	        void        addLocal(Type *ty, const char *nam, Exp *e);

	/// return a local's type
	        Type       *getLocalType(const char *nam);
	        void        setLocalType(const char *nam, Type *ty);

	        Type       *getParamType(const char *nam);

	/// return a symbol's exp (note: the original exp, like r24, not local1)
	        Exp        *expFromSymbol(const char *nam);
	        void        setExpSymbol(const char *nam, Exp *e, Type *ty);
	        void        mapSymbolTo(Exp *from, Exp *to);
	/// As above but with replacement
	        void        mapSymbolToRepl(Exp *from, Exp *oldTo, Exp *newTo);
	        void        removeSymbolMapping(Exp *from, Exp *to);  /// Remove this mapping
	/// Lookup the expression in the symbol map. Return NULL or a C string with the symbol. Use the Type* ty to
	/// select from several names in the multimap; the name corresponding to the first compatible type is returned
	        Exp        *getSymbolFor(Exp *e, Type *ty);  /// Lookup the symbol map considering type
	        const char *lookupSym(Exp *e, Type *ty);
	        const char *lookupSymFromRef(RefExp *r);     // Lookup a specific symbol for the given ref
	        const char *lookupSymFromRefAny(RefExp *r);  // Lookup a specific symbol if any, else the general one if any
	        const char *lookupParam(Exp *e);             // Find the implicit definition for e and lookup a symbol
	        void        checkLocalFor(RefExp *r);        // Check if r is already mapped to a local, else add one
	        Type       *getTypeForLocation(Exp *e);      // Find the type of the local or parameter e
	/// Determine whether e is a local, either as a true opLocal (e.g. generated by fromSSA), or if it is in the
	/// symbol map and the name is in the locals map. If it is a local, return its name, else NULL
	        const char *findLocal(Exp *e, Type *ty);
	        const char *findLocalFromRef(RefExp *r);
	        const char *findFirstSymbol(Exp *e);
	        int         getNumLocals() { return (int)locals.size(); }
	        const char *getLocalName(int n);
	        const char *getSymbolName(Exp *e);  ///< As getLocalName, but look for expression e
	        void        renameLocal(const char *oldName, const char *newName);
	virtual void        renameParam(const char *oldName, const char *newName);

	        const char *getRegName(Exp *r);  /// Get a name like eax or o2 from r24 or r8
	        void        setParamType(const char *nam, Type *ty);
	        void        setParamType(int idx, Type *ty);

	/**
	 * Print the locals declaration in C style.
	 */
	        void        printLocalsAsC(std::ostream &os);

	/**
	 * Get the BB that is the entry point (not always the first BB)
	 */
	        BasicBlock *getEntryBB();

	/*
	 * Prints this procedure to an output stream.
	 */
	        //std::ostream &put(std::ostream &os);

	/**
	 * Set the entry BB for this procedure (constructor has the entry address)
	 */
	        void        setEntryBB();

	/**
	 * Get the callees.
	 */
	        std::list<Proc *> &getCallees() { return calleeList; }

	/**
	 * Add to the set of callees.
	 */
	        void        addCallee(Proc *callee);

	/**
	 * Add to a set of callee Procs.
	 */
	        void        addCallees(std::list<UserProc *> &callees);

	/**
	 * return true if this procedure contains the given address.
	 */
	        bool        containsAddr(ADDRESS uAddr);

	/**
	 * Change BB containing this statement from a COMPCALL to a CALL.
	 */
	        void        undoComputedBB(Statement *stmt) { cfg->undoComputedBB(stmt); }

	/*
	 * Return true if this proc uses the special aggregate pointer as the
	 * first parameter
	 */
	//virtual bool        isAggregateUsed() { return aggregateUsed; }

	virtual Exp        *getProven(Exp *left);
	virtual Exp        *getPremised(Exp *left);
	// Set a location as a new premise, i.e. assume e=e
	        void        setPremise(Exp *e) { e = e->clone(); recurPremises[e] = e; }
	        void        killPremise(Exp *e) { recurPremises.erase(e); }
	virtual bool        isPreserved(Exp *e);  ///< Return whether e is preserved by this proc

	virtual void        printCallGraphXML(std::ostream &os, int depth, bool recurse = true);
	        void        printDecodedXML();
	        void        printAnalysedXML();
	        void        printSSAXML();
	        void        printXML();
	        void        printUseGraph();


	        bool        searchAndReplace(Exp *search, Exp *replace);

	/// Cast the constant whose conscript is num to be type ty
	        void        castConst(int num, Type *ty);

	/// Add a location to the UseCollector; this means this location is used before defined, and hence is an
	/// *initial* parameter. Note that final parameters don't use this information; it's only for handling recursion.
	        void        useBeforeDefine(Exp *loc) { col.insert(loc); }

	/// Copy the decoded indirect control transfer instructions' RTLs to the front end's map, and decode any new
	/// targets for this CFG
	        void        processDecodedICTs();

private:
	/// We ensure that there is only one return statement now. See code in frontend/frontend.cpp handling case
	/// STMT_RET. If no return statement, this will be NULL.
	        ReturnStatement *theReturnStatement = NULL;
	        int         DFGcount = 0;
public:
	        ADDRESS     getTheReturnAddr() { return theReturnStatement == NULL ? NO_ADDRESS : theReturnStatement->getRetAddr(); }
	        void        setTheReturnAddr(ReturnStatement *s, ADDRESS r) {
		                    assert(theReturnStatement == NULL);
		                    theReturnStatement = s;
		                    theReturnStatement->setRetAddr(r);
	        }
	        ReturnStatement *getTheReturnStatement() { return theReturnStatement; }
	        bool        filterReturns(Exp *e);  ///< Decide whether to filter out e (return true) or keep it
	        bool        filterParams(Exp *e);   ///< As above but for parameters and arguments

	/// Find and if necessary insert an implicit reference before s whose address expression is a and type is t.
	        void        setImplicitRef(Statement *s, Exp *a, Type *ty);

protected:
	friend class XMLProgParser;
	                    UserProc();
	        void        setCFG(Cfg *c) { cfg = c; }
};

#endif
