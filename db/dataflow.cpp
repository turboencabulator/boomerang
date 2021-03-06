/**
 * \file
 * \brief Implementation of the DataFlow class.
 *
 * \authors
 * Copyright (C) 2005, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "dataflow.h"

#include "boomerang.h"
#include "cfg.h"
#include "exp.h"
#include "frontend.h"
#include "proc.h"
#include "visitor.h"

#include <sstream>

#include <cassert>

/*
 * Dominator frontier code largely as per Appel 2002 ("Modern Compiler Implementation in Java")
 */

void
DataFlow::DFS(int p, int n)
{
	if (dfnum[n] == 0) {
		dfnum[n] = N; vertex[N] = n; parent[n] = p;
		++N;
		// For each successor w of n
		const auto &outEdges = BBs[n]->getOutEdges();
		for (const auto &edge : outEdges) {
			DFS(n, indices[edge]);
		}
	}
}

// Essentially Algorithm 19.9 of Appel's "modern compiler implementation in Java" 2nd ed 2002
void
DataFlow::dominators(Cfg *cfg)
{
	BasicBlock *r = cfg->getEntryBB();
	unsigned numBB = cfg->getNumBBs();
	BBs.resize(numBB, (BasicBlock *)-1);
	N = 0; BBs[0] = r;
	indices.clear();  // In case restart decompilation due to switch statements
	indices[r] = 0;
	// Initialise to "none"
	dfnum.resize(numBB, 0);
	semi.resize(numBB, -1);
	ancestor.resize(numBB, -1);
	idom.resize(numBB, -1);
	samedom.resize(numBB, -1);
	vertex.resize(numBB, -1);
	parent.resize(numBB, -1);
	best.resize(numBB, -1);
	bucket.resize(numBB);
	DF.resize(numBB);
	// Set up the BBs and indices vectors. Do this here because sometimes a BB can be unreachable (so relying on
	// in-edges doesn't work)
	int idx = 1;
	for (const auto &bb : *cfg) {
		if (bb != r) {  // Entry BB r already done
			indices[bb] = idx;
			BBs[idx++] = bb;
		}
	}
	DFS(-1, 0);
	int i;
	for (i = N - 1; i >= 1; --i) {
		int n = vertex[i]; int p = parent[n]; int s = p;
		/* These lines calculate the semi-dominator of n, based on the Semidominator Theorem */
		// for each predecessor v of n
		const auto &inEdges = BBs[n]->getInEdges();
		for (const auto &edge : inEdges) {
			if (!indices.count(edge)) {
				std::cerr << "BB not in indices: "; edge->print(std::cerr);
				assert(false);
			}
			int v = indices[edge];
			int sdash;
			if (dfnum[v] <= dfnum[n])
				sdash = v;
			else
				sdash = semi[ancestorWithLowestSemi(v)];
			if (dfnum[sdash] < dfnum[s])
				s = sdash;
		}
		semi[n] = s;
		/* Calculation of n's dominator is deferred until the path from s to n has been linked into the forest */
		bucket[s].insert(n);
		Link(p, n);
		// for each v in bucket[p]
		for (const auto &v : bucket[p]) {
			/* Now that the path from p to v has been linked into the spanning forest, these lines calculate the
				dominator of v, based on the first clause of the Dominator Theorem, or else defer the calculation until
				y's dominator is known. */
			int y = ancestorWithLowestSemi(v);
			if (semi[y] == semi[v])
				idom[v] = p;  // Success!
			else
				samedom[v] = y;  // Defer
		}
		bucket[p].clear();
	}
	for (i = 1; i < N - 1; ++i) {
		/* Now all the deferred dominator calculations, based on the second clause of the Dominator Theorem, are
			performed. */
		int n = vertex[i];
		if (samedom[n] != -1) {
			idom[n] = idom[samedom[n]];  // Deferred success!
		}
	}
	computeDF(0);  // Finally, compute the dominance frontiers
}

// Basically algorithm 19.10b of Appel 2002 (uses path compression for O(log N) amortised time per operation
// (overall O(N log N))
int
DataFlow::ancestorWithLowestSemi(int v)
{
	int a = ancestor[v];
	if (ancestor[a] != -1) {
		int b = ancestorWithLowestSemi(a);
		ancestor[v] = ancestor[a];
		if (dfnum[semi[b]] < dfnum[semi[best[v]]])
			best[v] = b;
	}
	return best[v];
}

void
DataFlow::Link(int p, int n)
{
	ancestor[n] = p; best[n] = n;
}

// Return true if n dominates w
bool
DataFlow::doesDominate(int n, int w) const
{
	while (idom[w] != -1) {
		if (idom[w] == n)
			return true;
		w = idom[w];  // Move up the dominator tree
	}
	return false;
}

void
DataFlow::computeDF(int n)
{
	std::set<int> S;
	/* This loop computes DF_local[n] */
	const auto &outEdges = BBs[n]->getOutEdges();
	for (const auto &edge : outEdges) {
		int y = indices[edge];
		if (idom[y] != n)
			S.insert(y);
	}
	// for each child c of n in the dominator tree
	// Note: this is a linear search!
	int sz = idom.size();  // ? Was ancestor.size()
	for (int c = 0; c < sz; ++c) {
		if (idom[c] != n) continue;
		computeDF(c);
		/* This loop computes DF_up[c] */
		const auto &s = DF[c];
		for (const auto &w : s) {
			// if n does not dominate w, or if n = w
			if (n == w || !doesDominate(n, w)) {
				S.insert(w);
			}
		}
	}
	DF[n] = S;
}


bool
DataFlow::canRename(Exp *e, UserProc *proc) const
{
	if (auto re = dynamic_cast<RefExp *>(e)) e = re->getSubExp1();  // Look inside refs
	if (e->isRegOf())    return true;   // Always rename registers
	if (e->isTemp())     return true;   // Always rename temps (always want to propagate away)
	if (e->isFlags())    return true;   // Always rename flags
	if (e->isMainFlag()) return true;   // Always rename individual flags like %CF
	if (e->isLocal())    return true;   // Rename hard locals in the post fromSSA pass
	if (!e->isMemOf())   return false;  // Can't rename %pc or other junk
	// I used to check here if there was a symbol for the memory expression, and if so allow it to be renamed. However,
	// even named locals and parameters could have their addresses escape the local function, so we need another test
	// anyway. So locals and parameters should not be renamed (and hence propagated) until escape analysis is done (and
	// hence renaleLocalsAndParams is set)
	// Besides,  before we have types and references, it is not easy to find a type for the location, so we can't tell
	// if e.g. m[esp{-}+12] is evnp or a separate local.
	// It certainly needs to have the local/parameter pattern
	if (!proc->isLocalOrParamPattern(e)) return false;
	// e is a local or parameter; allow it to be propagated iff we've done escape analysis and the address has not
	return renameLocalsAndParams && !proc->isAddressEscapedVar(e);  // escaped
}

bool
DataFlow::placePhiFunctions(UserProc *proc)
{
	// First free some memory no longer needed
	dfnum.clear();
	semi.clear();
	ancestor.clear();
	samedom.clear();
	vertex.clear();
	parent.clear();
	best.clear();
	bucket.clear();
	defsites.clear();   // Clear defsites map,
	defallsites.clear();
	A_orig.clear();     // and A_orig,
	defStmts.clear();   // and the map from variable to defining Stmt

	bool change = false;

	// Set the sizes of needed vectors
	unsigned numBB = indices.size();
	Cfg *cfg = proc->getCFG();
	assert(numBB == cfg->getNumBBs());
	A_orig.resize(numBB);

	// We need to create A_orig[n] for all n, the array of sets of locations defined at BB n
	// Recreate each call because propagation and other changes make old data invalid
	unsigned n;
	for (n = 0; n < numBB; ++n) {
		BasicBlock::rtlit rit;
		BasicBlock::stlit sit;
		BasicBlock *bb = BBs[n];
		for (Statement *s = bb->getFirstStmt(rit, sit); s; s = bb->getNextStmt(rit, sit)) {
			LocationSet ls;
			s->getDefinitions(ls);
			auto call = dynamic_cast<CallStatement *>(s);
			if (call && call->isChildless())  // If this is a childless call
				defallsites.insert(n);  // then this block defines every variable
			for (const auto &loc : ls) {
				if (canRename(loc, proc)) {
					A_orig[n].insert(loc->clone());
					defStmts[loc] = s;
				}
			}
		}
	}

	// For each node n
	for (n = 0; n < numBB; ++n) {
		const auto &s = A_orig[n];
		for (const auto &a : s) {
			defsites[a].insert(n);
		}
	}

	// For each variable a (in defsites, i.e. defined anywhere)
	for (auto &defsite : defsites) {
		Exp *a = defsite.first;

		// Special processing for define-alls
		defsite.second.insert(defallsites.begin(), defallsites.end());

		// W <- defsite.second;
		std::set<int> W = defsite.second;  // set copy
		// While W not empty
		while (!W.empty()) {
			// Remove some node n from W
			int n = *W.begin();  // Copy first element
			W.erase(W.begin());  // Remove first element
			const auto &DFn = DF[n];
			for (const auto &y : DFn) {
				// if y not element of A_phi[a]
				std::set<int> &s = A_phi[a];
				if (!s.count(y)) {
					// Insert trivial phi function for a at top of block y: a := phi()
					change = true;
					Statement *as = new PhiAssign(a->clone());
					BasicBlock *Ybb = BBs[y];
					Ybb->prependStmt(as, proc);
					// A_phi[a] <- A_phi[a] U {y}
					s.insert(y);
					// if a !elementof A_orig[y]
					if (!A_orig[y].count(a)) {
						// W <- W U {y}
						W.insert(y);
					}
				}
			}
		}
	}
	return change;
}



static Exp *defineAll = new Terminal(opDefineAll);  // An expression representing <all>

// There is an entry in stacks[defineAll] that represents the latest definition from a define-all source. It is needed
// for variables that don't have a definition as yet (i.e. stacks[x].empty() is true). As soon as a real definition to
// x appears, stacks[defineAll] does not apply for variable x. This is needed to get correct operation of the use
// collectors in calls.

// Care with the Stacks object (a map from expression to stack); using Stacks[q].empty() can needlessly insert an empty
// stack
#define STACKS_EMPTY(q) (!Stacks.count(q) || Stacks[q].empty())

// Subscript dataflow variables
static int progress = 0;
bool
DataFlow::renameBlockVars(UserProc *proc, int n, bool clearStacks /* = false */)
{
	if (++progress > 200) {
		std::cerr << 'r' << std::flush;
		progress = 0;
	}
	bool changed = false;

	// Need to clear the Stacks of old, renamed locations like m[esp-4] (these will be deleted, and will cause compare
	// failures in the Stacks, so it can't be correctly ordered and hence balanced etc, and will lead to segfaults)
	if (clearStacks) Stacks.clear();

	// For each statement S in block n
	BasicBlock::rtlit rit;
	BasicBlock::stlit sit;
	BasicBlock *bb = BBs[n];
	Statement *S;
	for (S = bb->getFirstStmt(rit, sit); S; S = bb->getNextStmt(rit, sit)) {
		// if S is not a phi function (per Appel)
		/* if (!dynamic_cast<PhiAssign *>(S)) */ {
			// For each use of some variable x in S (not just assignments)
			LocationSet locs;
			if (auto pa = dynamic_cast<PhiAssign *>(S)) {
				Exp *phiLeft = pa->getLeft();
				if (phiLeft->isMemOf() || phiLeft->isRegOf())
					phiLeft->getSubExp1()->addUsedLocs(locs);
				// A phi statement may use a location defined in a childless call, in which case its use collector
				// needs updating
				for (const auto &pp : *pa) {
					if (auto call = dynamic_cast<CallStatement *>(pp.def))
						call->useBeforeDefine(phiLeft->clone());
				}
			} else {  // Not a phi assignment
				S->addUsedLocs(locs);
			}
			for (const auto &x : locs) {
				// Don't rename memOfs that are not renamable according to the current policy
				if (!canRename(x, proc)) continue;
				Statement *def = nullptr;
				if (auto re = dynamic_cast<RefExp *>(x)) {  // Already subscripted?
					// No renaming required, but redo the usage analysis, in case this is a new return, and also because
					// we may have just removed all call livenesses
					// Update use information in calls, and in the proc (for parameters)
					Exp *base = re->getSubExp1();
					def = re->getDef();
					if (auto call = dynamic_cast<CallStatement *>(def)) {
						// Calls have UseCollectors for locations that are used before definition at the call
						call->useBeforeDefine(base->clone());
						continue;
					}
					// Update use collector in the proc (for parameters)
					if (!def)
						proc->useBeforeDefine(base->clone());
					continue;  // Don't re-rename the renamed variable
				}
				// Else x is not subscripted yet
				if (STACKS_EMPTY(x)) {
					if (!Stacks[defineAll].empty())
						def = Stacks[defineAll].top();
					else {
						// If the both stacks are empty, use a nullptr definition. This will be changed into a pointer
						// to an implicit definition at the start of type analysis, but not until all the m[...]
						// have stopped changing their expressions (complicates implicit assignments considerably).
						def = nullptr;
						// Update the collector at the start of the UserProc
						proc->useBeforeDefine(x->clone());
					}
				} else
					def = Stacks[x].top();
				if (auto call = dynamic_cast<CallStatement *>(def))
					// Calls have UseCollectors for locations that are used before definition at the call
					call->useBeforeDefine(x->clone());
				// Replace the use of x with x{def} in S
				changed = true;
				if (auto pa = dynamic_cast<PhiAssign *>(S)) {
					Exp *phiLeft = pa->getLeft();
					phiLeft->setSubExp1(phiLeft->getSubExp1()->expSubscriptVar(x, def /*, this*/));
				} else {
					S->subscriptVar(x, def /*, this */);
				}
			}
		}

		// MVE: Check for Call and Return Statements; these have DefCollector objects that need to be updated
		// Do before the below, so CallStatements have not yet processed their defines
		if (auto cs = dynamic_cast<CallStatement *>(S)) {
			auto col = cs->getDefCollector();
			col->updateDefs(Stacks, proc);
		} else if (auto rs = dynamic_cast<ReturnStatement *>(S)) {
			auto col = rs->getCollector();
			col->updateDefs(Stacks, proc);
		}

		// For each definition of some variable a in S
		LocationSet defs;
		S->getDefinitions(defs);
		for (const auto &a : defs) {
			// Don't consider a if it cannot be renamed
			bool suitable = canRename(a, proc);
			if (suitable) {
				// Push i onto Stacks[a]
				// Note: we clone a because otherwise it could be an expression that gets deleted through various
				// modifications. This is necessary because we do several passes of this algorithm to sort out the
				// memory expressions
				Stacks[a->clone()].push(S);
				// Replace definition of a with definition of a_i in S (we don't do this)
			}
			// FIXME: MVE: do we need this awful hack?
			if (a->isLocal()) {
				Exp *a1 = S->getProc()->expFromSymbol(((Const *)a->getSubExp1())->getStr());
				assert(a1);
				// Stacks already has a definition for a (as just the bare local)
				if (suitable) {
					Stacks[a1->clone()].push(S);
				}
			}
		}
		// Special processing for define-alls (presently, only childless calls).
		// But note that only everythings at the current memory level are defined!
		auto cs = dynamic_cast<CallStatement *>(S);
		if (cs && cs->isChildless() && !Boomerang::get().assumeABI) {
			// S is a childless call (and we're not assuming ABI compliance)
			Stacks[defineAll];  // Ensure that there is an entry for defineAll
			for (auto &stack : Stacks) {
				//if (dd.first->isMemDepth(memDepth))
					stack.second.push(S);  // Add a definition for all vars
			}
		}
	}

	// For each successor Y of block n
	const auto &outEdges = bb->getOutEdges();
	for (const auto &Ybb : outEdges) {
		// Suppose n is the jth predecessor of Y
		int j = Ybb->whichPred(bb);
		// For each phi-function in Y
		for (Statement *S = Ybb->getFirstStmt(rit, sit); S; S = Ybb->getNextStmt(rit, sit)) {
			if (auto pa = dynamic_cast<PhiAssign *>(S)) {
				// Suppose the jth operand of the phi is a
				// For now, just get the LHS
				Exp *a = pa->getLeft();
				// Only consider variables that can be renamed
				if (!canRename(a, proc)) continue;
				Statement *def;
				if (STACKS_EMPTY(a))
					def = nullptr;  // No reaching definition
				else
					def = Stacks[a].top();
				// "Replace jth operand with a_i"
				pa->putAt(j, def, a);
			}
		}
	}

	// For each child X of n
	// Note: linear search!
	unsigned numBB = proc->getCFG()->getNumBBs();
	for (unsigned X = 0; X < numBB; ++X) {
		if (idom[X] == n)
			renameBlockVars(proc, X);
	}

	// For each statement S in block n
	// NOTE: Because of the need to pop childless calls from the Stacks, it is important in my algorithm to process the
	// statments in the BB *backwards*. (It is not important in Appel's algorithm, since he always pushes a definition
	// for every variable defined on the Stacks).
	BasicBlock::rtlrit rrit;
	BasicBlock::stlrit srit;
	for (S = bb->getLastStmt(rrit, srit); S; S = bb->getPrevStmt(rrit, srit)) {
		// For each definition of some variable a in S
		LocationSet defs;
		S->getDefinitions(defs);
		for (const auto &def : defs) {
			if (canRename(def, proc)) {
				// if (def->getMemDepth() == memDepth)
				auto ss = Stacks.find(def);
				if (ss == Stacks.end()) {
					std::cerr << "Tried to pop " << *def << " from Stacks; does not exist\n";
					assert(0);
				}
				ss->second.pop();
			}
		}
		// Pop all defs due to childless calls
		auto cs = dynamic_cast<CallStatement *>(S);
		if (cs && cs->isChildless()) {
			for (auto &stack : Stacks) {
				if (!stack.second.empty() && stack.second.top() == S) {
					stack.second.pop();
				}
			}
		}
	}
	return changed;
}

void
DefCollector::updateDefs(std::map<Exp *, std::stack<Statement *>, lessExpStar> &Stacks, UserProc *proc)
{
	for (const auto &stack : Stacks) {
		if (stack.second.empty())
			continue;  // This variable's definition doesn't reach here
		// Create an assignment of the form loc := loc{def}
		auto re = new RefExp(stack.first->clone(), stack.second.top());
		auto as = new Assign(stack.first->clone(), re);
		as->setProc(proc);  // Simplify sometimes needs this
		insert(as);
	}
	initialised = true;
}

// Find the definition for e that reaches this Collector. If none reaches here, return nullptr
Exp *
DefCollector::findDefFor(Exp *e) const
{
	for (const auto &def : defs) {
		Exp *lhs = def->getLeft();
		if (*lhs == *e)
			return def->getRight();
	}
	return nullptr;  // Not explicitly defined here
}

void
UseCollector::print(std::ostream &os, bool html) const
{
	bool first = true;
	for (const auto &loc : locs) {
		if (first)
			first = false;
		else
			os << ",  ";
		loc->print(os, html);
	}
}

#define DEFCOL_COLS 120
void
DefCollector::print(std::ostream &os, bool html) const
{
	unsigned col = 36;
	bool first = true;
	for (const auto &def : defs) {
		std::ostringstream ost;
		def->getLeft()->print(ost, html);
		ost << "=";
		def->getRight()->print(ost, html);
		unsigned len = ost.str().length();
		if (first)
			first = false;
		else if (col + 4 + len >= DEFCOL_COLS) {  // 4 for a comma and three spaces
			if (col != DEFCOL_COLS - 1) os << ",";  // Comma at end of line
			os << "\n                ";
			col = 16;
		} else {
			os << ",   ";
			col += 4;
		}
		os << ost.str();
		col += len;
	}
}

std::string
UseCollector::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

std::string
DefCollector::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

void
UseCollector::makeCloneOf(const UseCollector &other)
{
	initialised = other.initialised;
	locs.clear();
	for (const auto &o : other)
		locs.insert(o->clone());
}

void
DefCollector::makeCloneOf(const DefCollector &other)
{
	initialised = other.initialised;
	defs.clear();
	for (const auto &o : other)
		defs.insert((Assign *)o->clone());
}

void
DefCollector::searchReplaceAll(Exp *from, Exp *to, bool &change)
{
	for (const auto &def : defs)
		def->searchAndReplace(from, to);
}

// Called from CallStatement::fromSSAform. The UserProc is needed for the symbol map
void
UseCollector::fromSSAform(UserProc *proc, Statement *def)
{
	LocationSet removes, inserts;
	ExpSsaXformer esx(proc);
	for (const auto &loc : locs) {
		auto ref = new RefExp(loc, def);  // Wrap it in a def
		Exp *ret = ref->accept(esx);
		// If there is no change, ret will equal loc again (i.e. fromSSAform just removed the subscript)
		if (ret != loc) {  // Pointer comparison
			// There was a change; we want to replace loc with ret
			removes.insert(loc);
			inserts.insert(ret);
		}
	}
	locs.makeDiff(removes);
	locs.makeUnion(inserts);
}

bool
UseCollector::operator ==(const UseCollector &other) const
{
	if (other.initialised != initialised) return false;
	if (other.locs.size() != locs.size()) return false;
	for (auto it1 = locs.begin(), it2 = other.locs.begin(); it1 != locs.end(); ++it1, ++it2)
		if (!(**it1 == **it2)) return false;
	return true;
}

void
DefCollector::insert(Assign *a)
{
	Exp *l = a->getLeft();
	if (existsOnLeft(l)) return;
	defs.insert(a);
}

void
DataFlow::convertImplicits(Cfg *cfg)
{
	// Convert statements in A_phi from m[...]{-} to m[...]{0}
	ImplicitConverter ic(cfg);
	auto A_phi_copy = std::map<Exp *, std::set<int>, lessExpStar>();
	A_phi_copy.swap(A_phi);
	for (const auto &dd : A_phi_copy) {
		Exp *e = dd.first->clone();
		e = e->accept(ic);
		A_phi[e] = dd.second;  // Copy the set (doesn't have to be deep)
	}

	auto defsites_copy = std::map<Exp *, std::set<int>, lessExpStar>();
	defsites_copy.swap(defsites);
	for (const auto &dd : defsites_copy) {
		Exp *e = dd.first->clone();
		e = e->accept(ic);
		defsites[e] = dd.second;  // Copy the set (doesn't have to be deep)
	}

	auto A_orig_copy = std::vector<std::set<Exp *, lessExpStar> >();
	A_orig_copy.swap(A_orig);
	for (const auto &se : A_orig_copy) {
		std::set<Exp *, lessExpStar> se_new;
		for (const auto &ee : se) {
			Exp *e = ee->clone();
			e = e->accept(ic);
			se_new.insert(e);
		}
		A_orig.push_back(se_new);  // Copy the set (doesn't have to be a deep copy)
	}
}


// Helper function for UserProc::propagateStatements()
// Works on basic block n; call from UserProc with n=0 (entry BB)
// If an SSA location is in usedByDomPhi it means it is used in a phi that dominates its assignment
// However, it could turn out that the phi is dead, in which case we don't want to keep the associated entries in
// usedByDomPhi. So we maintain the map defdByPhi which maps locations defined at a phi to the phi statements. Every
// time we see a use of a location in defdByPhi, we remove that map entry. At the end of the procedure we therefore have
// only dead phi statements in the map, so we can delete the associated entries in defdByPhi and also remove the dead
// phi statements.
// We add to the set usedByDomPhi0 whenever we see a location referenced by a phi parameter. When we see a definition
// for such a location, we remove it from the usedByDomPhi0 set (to save memory) and add it to the usedByDomPhi set.
// For locations defined before they are used in a phi parameter, there will be no entry in usedByDomPhi, so we ignore
// it. Remember that each location is defined only once, so that's the time to decide if it is dominated by a phi use or
// not.
void
DataFlow::findLiveAtDomPhi(int n, LocationSet &usedByDomPhi, LocationSet &usedByDomPhi0, std::map<Exp *, PhiAssign *, lessExpStar> &defdByPhi)
{
	// For each statement this BB
	BasicBlock::rtlit rit;
	BasicBlock::stlit sit;
	BasicBlock *bb = BBs[n];
	Statement *S;
	for (S = bb->getFirstStmt(rit, sit); S; S = bb->getNextStmt(rit, sit)) {
		if (auto pa = dynamic_cast<PhiAssign *>(S)) {
			// For each phi parameter, insert an entry into usedByDomPhi0
			for (const auto &pp : *pa) {
				if (pp.e) {
					usedByDomPhi0.insert(new RefExp(pp.e, pp.def));
				}
			}
			// Insert an entry into the defdByPhi map
			auto wrappedLhs = new RefExp(pa->getLeft(), pa);
			defdByPhi[wrappedLhs] = pa;
			// Fall through to the below, because phi uses are also legitimate uses
		}
		LocationSet ls;
		S->addUsedLocs(ls);
		// Consider uses of this statement
		for (const auto &loc : ls) {
			// Remove this entry from the map, since it is not unused
			defdByPhi.erase(loc);
		}
		// Now process any definitions
		ls.clear();
		S->getDefinitions(ls);
		for (const auto &loc : ls) {
			auto wrappedDef = new RefExp(loc, S);
			// If this definition is in the usedByDomPhi0 set, then it is in fact dominated by a phi use, so move it to
			// the final usedByDomPhi set
			if (usedByDomPhi0.exists(wrappedDef)) {
				usedByDomPhi0.remove(wrappedDef);
				usedByDomPhi.insert(wrappedDef);
			}
		}
	}

	// Visit each child in the dominator graph
	// Note: this is a linear search!
	// Note also that usedByDomPhi0 may have some irrelevant entries, but this will do no harm, and attempting to erase
	// the irrelevant ones would probably cost more than leaving them alone
	int sz = idom.size();
	for (int c = 0; c < sz; ++c) {
		if (idom[c] != n) continue;
		// Recurse to the child
		findLiveAtDomPhi(c, usedByDomPhi, usedByDomPhi0, defdByPhi);
	}
}

#if USE_DOMINANCE_NUMS
void
DataFlow::setDominanceNums(int n, int &currNum)
{
	BasicBlock::rtlit rit;
	BasicBlock::stlit sit;
	BasicBlock *bb = BBs[n];
	Statement *S;
	for (S = bb->getFirstStmt(rit, sit); S; S = bb->getNextStmt(rit, sit))
		S->setDomNumber(currNum++);
	int sz = idom.size();
	for (int c = 0; c < sz; ++c) {
		if (idom[c] != n) continue;
		// Recurse to the child
		setDominanceNums(c, currNum);
	}
}
#endif
