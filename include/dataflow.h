/**
 * \file
 * \brief Interface for SSA-based data flow analysis.
 *
 * \authors
 * Copyright (C) 2005-2006, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef DATAFLOW_H
#define DATAFLOW_H

#include "boomerang.h"  // For USE_DOMINANCE_NUMS etc
#include "exphelp.h"    // For lessExpStar, etc
#include "managed.h"    // For LocationSet

#include <ostream>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>

class BasicBlock;
class Cfg;
class Exp;
class PhiAssign;
class Statement;
class UserProc;

class DataFlow {
	/******************** Dominance Frontier Data *******************/

	/* These first two are not from Appel; they map BasicBlock*s to indices */
	std::vector<BasicBlock *> BBs;        // Pointers to BBs from indices
	std::map<BasicBlock *, int> indices;  // Indices from pointers to BBs
	/*
	 * Calculating the dominance frontier
	 */
	// If there is a path from a to b in the cfg, then a is an ancestor of b
	// if dfnum[a] < denum[b]
	std::vector<int> dfnum;             // Number set in depth first search
	std::vector<int> semi;              // Semi dominators
	std::vector<int> ancestor;          // Defines the forest that becomes the spanning tree
	std::vector<int> idom;              // Immediate dominator
	std::vector<int> samedom;           // ? To do with deferring
	std::vector<int> vertex;            // ?
	std::vector<int> parent;            // Parent in the dominator tree?
	std::vector<int> best;              // Improves ancestorWithLowestSemi
	std::vector<std::set<int> > bucket; // Deferred calculation?
	int         N;                      // Current node number in algorithm
	std::vector<std::set<int> > DF;     // The dominance frontiers

	/*
	 * Inserting phi-functions
	 */
	// Array of sets of locations defined in BB n
	std::vector<std::set<Exp *, lessExpStar> > A_orig;
	// Map from expression to set of block numbers
	std::map<Exp *, std::set<int>, lessExpStar> defsites;
	// Set of block numbers defining all variables
	std::set<int> defallsites;
	// Array of sets of BBs needing phis
	std::map<Exp *, std::set<int>, lessExpStar> A_phi;
	// A Boomerang requirement: Statements defining particular subscripted locations
	std::map<Exp *, Statement *, lessExpStar> defStmts;

	/*
	 * Renaming variables
	 */
	// The stack which remembers the last definition of an expression.
	// A map from expression (Exp *) to a stack of (pointers to) Statements
	std::map<Exp *, std::stack<Statement *>, lessExpStar> Stacks;

	// Initially false, meaning that locals and parameters are not renamed and hence not propagated.
	// When true, locals and parameters can be renamed if their address does not escape the local procedure.
	// See Mike's thesis for details.
	bool        renameLocalsAndParams = false;

public:
	            DataFlow() { }  // Constructor
	/*
	 * Dominance frontier and SSA code
	 */
	void        DFS(int p, int n);
	void        dominators(Cfg *cfg);
	int         ancestorWithLowestSemi(int v);
	void        Link(int p, int n);
	void        computeDF(int n);
	// Place phi functions. Return true if any change
	bool        placePhiFunctions(UserProc *proc);
	// Rename variables in basicblock n. Return true if any change made
	bool        renameBlockVars(UserProc *proc, int n, bool clearStacks = false);
	bool        doesDominate(int n, int w);
	void        setRenameLocalsParams(bool b) { renameLocalsAndParams = b; }
	bool        canRenameLocalsParams() { return renameLocalsAndParams; }
	bool        canRename(Exp *e, UserProc *proc);
	void        convertImplicits(Cfg *cfg);
	// Find the locations used by a live, dominating phi-function. Also removes dead phi-funcions
	void        findLiveAtDomPhi(int n, LocationSet &usedByDomPhi, LocationSet &usedByDomPhi0, std::map<Exp *, PhiAssign *, lessExpStar> &defdByPhi);
#if USE_DOMINANCE_NUMS
	void        setDominanceNums(int n, int &currNum);  // Set the dominance statement number
#endif
	void        clearA_phi() { A_phi.clear(); }

	// For testing:
	int         pbbToNode(BasicBlock *bb) { return indices[bb]; }
	std::set<int> &getDF(int node) { return DF[node]; }
	BasicBlock *nodeToBB(int node) { return BBs[node]; }
	int         getIdom(int node) { return idom[node]; }
	int         getSemi(int node) { return semi[node]; }
	std::set<int> &getA_phi(Exp *e) { return A_phi[e]; }
};

/*  *   *   *   *   *   *\
*                        *
*   C o l l e c t o r s  *
*                        *
\*  *   *   *   *   *   */

/**
 * DefCollector class. This class collects all definitions that reach the statement that contains this collector.
 */
class DefCollector {
	/*
	 * True if initialised. When not initialised, callees should not subscript parameters inserted into the
	 * associated CallStatement
	 */
	bool        initialised = false;
	/**
	 * The set of definitions.
	 */
	AssignSet   defs;
public:
	/**
	 * Constructor
	 */
	            DefCollector() { }

	/**
	 * makeCloneOf(): clone the given Collector into this one
	 */
	void        makeCloneOf(DefCollector &other);

	/*
	 * Return true if initialised
	 */
	bool        isInitialised() { return initialised; }

	/*
	 * Clear the location set
	 */
	void        clear() { defs.clear(); initialised = false; }

	/*
	 * Insert a new member (make sure none exists yet)
	 */
	void        insert(Assign *a);
	/*
	 * Print the collected locations to stream os
	 */
	void        print(std::ostream &os, bool html = false);

	/*
	 * Print to string (for debugging)
	 */
	std::string prints();

	/*
	 * begin() and end() so we can iterate through the locations
	 */
	typedef AssignSet::iterator iterator;
	iterator    begin() { return defs.begin(); }
	iterator    end()   { return defs.end(); }
	bool        existsOnLeft(Exp *e) { return defs.definesLoc(e); }

	/*
	 * Update the definitions with the current set of reaching definitions
	 * proc is the enclosing procedure
	 */
	void        updateDefs(std::map<Exp *, std::stack<Statement *>, lessExpStar> &Stacks, UserProc *proc);

	/**
	 * Find the definition for a location.  If not found, return nullptr.
	 */
	Exp        *findDefFor(Exp *e);

	/**
	 * Search and replace all occurrences
	 */
	void        searchReplaceAll(Exp *from, Exp *to, bool &change);
};

/**
 * UseCollector class. This class collects all uses (live variables) that will be defined by the statement that
 * contains this collector (or the UserProc that contains it).
 * Typically the entries are not subscripted, like parameters or locations on the LHS of assignments
 */
class UseCollector {
	/*
	 * True if initialised. When not initialised, callees should not subscript parameters inserted into the
	 * associated CallStatement
	 */
	bool        initialised = false;
	/**
	 * The set of locations. Use lessExpStar to compare properly
	 */
	LocationSet locs;
public:
	/**
	 * Constructor
	 */
	            UseCollector() { }

	/**
	 * makeCloneOf(): clone the given Collector into this one
	 */
	void        makeCloneOf(UseCollector &other);

	/*
	 * Return true if initialised
	 */
	bool        isInitialised() { return initialised; }

	/*
	 * Clear the location set
	 */
	void        clear() { locs.clear(); initialised = false; }

	/*
	 * Insert a new member
	 */
	void        insert(Exp *e) { locs.insert(e); }
	/*
	 * Print the collected locations to stream os
	 */
	void        print(std::ostream &os, bool html = false);

	/*
	 * Print to string (for debugging)
	 */
	std::string prints();

	/*
	 * begin() and end() so we can iterate through the locations
	 */
	typedef LocationSet::iterator iterator;
	iterator    begin() { return locs.begin(); }
	iterator    end()   { return locs.end(); }
	bool        exists(Exp *e) { return locs.exists(e); }  // True if e is in the collection
	LocationSet &getLocSet() { return locs; }
public:
	/*
	 * Add a new use from Statement u
	 */
	void        updateLocs(Statement *u);
	void        remove(Exp *loc) { locs.remove(loc); }        // Remove the given location
	void        remove(iterator it) { locs.remove(it); }      // Remove the current location
	void        fromSSAform(UserProc *proc, Statement *def);  // Translate out of SSA form
	bool        operator ==(UseCollector &other);
};

#endif
