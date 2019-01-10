/**
 * \file
 * \brief Implementation of objects related to type constraints.
 *
 * \authors
 * Copyright (C) 2003, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "constraint.h"

#include "boomerang.h"
#include "exp.h"
#include "log.h"
#include "managed.h"
#include "type.h"

#include <sstream>

void
ConstraintMap::print(std::ostream &os) const
{
	bool first = true;
	for (const auto &con : cmap) {
		if (first) first = false;
		else os << ", ";
		os << *con.first << " = " << *con.second;
	}
	os << "\n";
}

std::string
ConstraintMap::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

void
ConstraintMap::makeUnion(const ConstraintMap &o)
{
	for (const auto &oth : o.cmap) {
		auto ret = cmap.insert(oth);
		// If an insertion occured, ret will be std::pair<where, true>
		// If no insertion occured, ret will be std::pair<where, false>
		if (!ret.second) {
			//std::cerr << "ConstraintMap::makeUnion: want to overwrite "
			//          << ret.first->first << " -> " << ret.first->second
			//          << " with " << oth.first << " -> " << oth.second << "\n";
			auto Tret = (TypeVal *)ret.first->second;
			Type *ty1 = Tret->getType();
			auto Toth = (TypeVal *)oth.second;
			Type *ty2 = Toth->getType();
			if (ty1 && ty2 && *ty1 != *ty2) {
				Tret->setType(ty1->mergeWith(ty2));
				//std::cerr << "Now " << ret.first->first << " -> " << ret.first->second << "\n";
			}
		}
	}
}

void
ConstraintMap::insert(Exp *term)
{
	assert(term->isEquality());
	Exp *lhs = ((Binary *)term)->getSubExp1();
	Exp *rhs = ((Binary *)term)->getSubExp2();
	cmap[lhs] = rhs;
}


void
EquateMap::print(std::ostream &os) const
{
	for (const auto &eq : emap) {
		os << "\t " << *eq.first << " = " << eq.second.prints();
	}
	os << "\n";
}

std::string
EquateMap::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

// Substitute the given constraints into this map
void
ConstraintMap::substitute(const ConstraintMap &other)
{
	for (const auto &oth : other.cmap) {
		bool ch;
		for (auto cc = cmap.begin(); cc != cmap.end(); ++cc) {
			Exp *newVal = cc->second->searchReplaceAll(oth.first, oth.second, ch);
			if (ch) {
				if (*cc->first == *newVal)
					// e.g. was <char*> = <alpha6> now <char*> = <char*>
					cmap.erase(cc);
				else
					cmap[cc->first] = newVal;
			} else
				// The existing value
				newVal = cc->second;
			Exp *newKey = cc->first->searchReplaceAll(oth.first, oth.second, ch);
			if (ch) {
				cmap.erase(cc->first);
				// Often end up with <char*> = <char*>
				if (!(*newKey == *newVal))
					cmap[newKey] = newVal;
			}
		}
	}
}

void
ConstraintMap::substAlpha()
{
	ConstraintMap alphaDefs;
	for (const auto &con : cmap) {
		// Looking for entries with two TypeVals, where exactly one is an alpha
		if (!con.first->isTypeVal() || !con.second->isTypeVal())
			continue;
		Type *t1, *t2;
		t1 = ((TypeVal *)con.first)->getType();
		t2 = ((TypeVal *)con.second)->getType();
		int numAlpha = 0;
		if (t1->isPointerToAlpha()) ++numAlpha;
		if (t2->isPointerToAlpha()) ++numAlpha;
		if (numAlpha != 1)
			continue;
		// This is such an equality. Copy it to alphaDefs
		if (t1->isPointerToAlpha())
			alphaDefs.cmap[con.first] = con.second;
		else
			alphaDefs.cmap[con.second] = con.first;
	}

	// Remove these from the solution
	for (const auto &con : alphaDefs)
		cmap.erase(con.first);

	// Now substitute into the remainder
	substitute(alphaDefs);
}



Constraints::~Constraints()
{
	for (const auto &con : conSet) {
		delete con;
	}
}


void
Constraints::substIntoDisjuncts(ConstraintMap &in)
{
	for (const auto &con : in) {
		Exp *from = con.first;
		Exp *to = con.second;
		bool ch;
		for (auto &dis : disjunctions) {
			dis->searchReplaceAll(from, to, ch);
			dis = dis->simplifyConstraint();
		}
	}
	// Now do alpha substitution
	alphaSubst();
}

void
Constraints::substIntoEquates(ConstraintMap &in)
{
	// Substitute the fixed types into the equates. This may generate more
	// fixed types
	ConstraintMap extra;
	ConstraintMap cur = in;
	while (!cur.empty()) {
		extra.clear();
		for (const auto &con : cur) {
			Exp *lhs = con.first;
			auto it = equates.find(lhs);
			if (it != equates.end()) {
				// Possibly new constraints that
				// typeof(elements in it->second) == val
				Exp *val = con.second;
				LocationSet &ls = it->second;
				for (const auto &loc : ls) {
					auto ff = fixed.find(loc);
					if (ff != fixed.end()) {
						if (!unify(val, ff->second, extra)) {
							if (VERBOSE || DEBUG_TA)
								LOG << "Constraint failure: " << *loc << " constrained to be "
								    << ((TypeVal *)val)->getType()->getCtype() << " and "
								    << ((TypeVal *)ff->second)->getType()->getCtype() << "\n";
							return;
						}
					} else
						extra[loc] = val;  // A new constant constraint
				}
				if (((TypeVal *)val)->getType()->isComplete()) {
					// We have a complete type equal to one or more variables
					// Remove the equate, and generate more fixed
					// e.g. Ta = Tb,Tc and Ta = K => Tb=K, Tc=K
					for (const auto &loc : ls) {
						Exp *newFixed = new Binary(opEquals,
						                           loc,     // e.g. Tb
						                           val);    // e.g. K
						extra.insert(newFixed);
					}
					equates.erase(it);
				}
			}
		}
		fixed.makeUnion(extra);
		cur = extra;  // Take care of any "ripple effect"
	}  // Repeat until no ripples
}

// Get the next disjunct from this disjunction
// Assumes that the remainder is of the for a or (b or c), or (a or b) or c
// But NOT (a or b) or (c or d)
// Could also be just a (a conjunction, or a single constraint)
// Note: remainder is changed by this function
static Exp *
nextDisjunct(Exp *&remainder)
{
	if (!remainder) return nullptr;
	if (remainder->isDisjunction()) {
		Exp *d1 = ((Binary *)remainder)->getSubExp1();
		Exp *d2 = ((Binary *)remainder)->getSubExp2();
		if (d1->isDisjunction()) {
			remainder = d1;
			return d2;
		}
		remainder = d2;
		return d1;
	}
	// Else, we have one disjunct. Return it
	Exp *ret = remainder;
	remainder = nullptr;
	return ret;
}

static Exp *
nextConjunct(Exp *&remainder)
{
	if (!remainder) return nullptr;
	if (remainder->isConjunction()) {
		Exp *c1 = ((Binary *)remainder)->getSubExp1();
		Exp *c2 = ((Binary *)remainder)->getSubExp2();
		if (c1->isConjunction()) {
			remainder = c1;
			return c2;
		}
		remainder = c2;
		return c1;
	}
	// Else, we have one conjunct. Return it
	Exp *ret = remainder;
	remainder = nullptr;
	return ret;
}

bool
Constraints::solve(std::list<ConstraintMap> &solns)
{
	LOG << conSet.size() << " constraints:";
	LOG << conSet.prints();
	// Replace Ta[loc] = ptr(alpha) with
	//         Tloc = alpha
	for (const auto &con : conSet) {
		if (!con->isEquality()) continue;
		Exp *left = ((Binary *)con)->getSubExp1();
		if (!left->isTypeOf()) continue;
		Exp *leftSub = ((Unary *)left)->getSubExp1();
		if (!leftSub->isAddrOf()) continue;
		Exp *right = ((Binary *)con)->getSubExp2();
		if (!right->isTypeVal()) continue;
		Type *t = ((TypeVal *)right)->getType();
		if (!t->isPointer()) continue;
		// Don't modify a key in a map
		Exp *clone = con->clone();
		// left is typeof(addressof(something)) -> typeof(something)
		left = ((Binary *)clone)->getSubExp1();
		leftSub = ((Unary *)left)->getSubExp1();
		Exp *something = ((Unary *)leftSub)->getSubExp1();
		((Unary *)left)->setSubExp1ND(something);
		((Unary *)leftSub)->setSubExp1ND(nullptr);
		delete leftSub;
		// right is <alpha*> -> <alpha>
		right = ((Binary *)clone)->getSubExp2();
		t = ((TypeVal *)right)->getType();
		((TypeVal *)right)->setType(((PointerType *)t)->getPointsTo()->clone());
		delete t;
		conSet.remove(con);  // FIXME:  Invalidates iterators
		conSet.insert(clone);
		delete con;
	}

	// Sort constraints into a few categories. Disjunctions go to a special
	// list, always true is just ignored, and constraints of the form
	// typeof(x) = y (where y is a type value) go to a map called fixed.
	// Constraint terms of the form Tx = Ty go into a map of LocationSets
	// called equates for fast lookup
	for (const auto &con : conSet) {
		if (con->isTrue()) continue;
		if (con->isFalse()) {
			if (VERBOSE || DEBUG_TA)
				LOG << "Constraint failure: always false constraint\n";
			return false;
		}
		if (con->isDisjunction()) {
			disjunctions.push_back(con);
			continue;
		}
		// Break up conjunctions into terms
		Exp *rem = con, *term;
		while (!!(term = nextConjunct(rem))) {
			assert(term->isEquality());
			Exp *lhs = ((Binary *)term)->getSubExp1();
			Exp *rhs = ((Binary *)term)->getSubExp2();
			if (rhs->isTypeOf()) {
				// Of the form typeof(x) = typeof(z)
				// Insert into equates
				equates.addEquate(lhs, rhs);
			} else {
				// Of the form typeof(x) = <typeval>
				// Insert into fixed
				assert(rhs->isTypeVal());
				fixed[lhs] = rhs;
			}
		}
	}

	LOG << "\n" << (unsigned)disjunctions.size() << " disjunctions: ";
	for (const auto &dis : disjunctions)
		LOG << *dis << ",\n";
	LOG << "\n";
	LOG << fixed.size() << " fixed: " << fixed.prints();
	LOG << equates.size() << " equates: " << equates.prints();

	// Substitute the fixed types into the disjunctions
	substIntoDisjuncts(fixed);

	// Substitute the fixed types into the equates. This may generate more
	// fixed types
	substIntoEquates(fixed);

	LOG << "\nAfter substitute fixed into equates:\n";
	LOG << "\n" << (unsigned)disjunctions.size() << " disjunctions: ";
	for (const auto &dis : disjunctions)
		LOG << *dis << ",\n";
	LOG << "\n";
	LOG << fixed.size() << " fixed: " << fixed.prints();
	LOG << equates.size() << " equates: " << equates.prints();

	// Substitute again the fixed types into the disjunctions
	// (since there may be more fixed types from the above)
	substIntoDisjuncts(fixed);

	LOG << "\nAfter second substitute fixed into disjunctions:\n";
	LOG << "\n" << (unsigned)disjunctions.size() << " disjunctions: ";
	for (const auto &dis : disjunctions)
		LOG << *dis << ",\n";
	LOG << "\n";
	LOG << fixed.size() << " fixed: " << fixed.prints();
	LOG << equates.size() << " equates: " << equates.prints();

	ConstraintMap soln;
	bool ret = doSolve(disjunctions.begin(), soln, solns);
	if (ret) {
		// For each solution, we need to find disjunctions of the form
		// <alphaN> = <type>      or
		// <type>   = <alphaN>
		// and substitute these into each part of the solution
		for (auto &sol : solns)
			sol.substAlpha();
	}
	return ret;
}

static int level = 0;
// Constraints up to but not including iterator it have been unified.
// The current solution is soln
// The set of all solutions is in solns
bool
Constraints::doSolve(std::list<Exp *>::iterator it, ConstraintMap &soln, std::list<ConstraintMap> &solns)
{
	LOG << "Begin doSolve at level " << ++level << "\n";
	LOG << "Soln now: " << soln.prints() << "\n";
	if (it == disjunctions.end()) {
		// We have gotten to the end with no unification failures
		// Copy the current set of constraints as a solution
		//if (soln.empty())
			// Awkward. There is a trivial solution, but we have no constraints
			// So make a constraint of always-true
			//soln.insert(new Terminal(opTrue));
		// Copy the fixed constraints
		soln.makeUnion(fixed);
		solns.push_back(soln);
		LOG << "Exiting doSolve at level " << level-- << " returning true\n";
		return true;
	}

	Exp *dj = *it;
	// Iterate through each disjunction d of dj
	Exp *rem1 = dj;  // Remainder
	bool anyUnified = false;
	Exp *d;
	while (!!(d = nextDisjunct(rem1))) {
		LOG << " $$ d is " << *d << ", rem1 is " << (!rem1 ? "NULL" : rem1->prints()) << " $$\n";
		// Match disjunct d against the fixed types; it could be compatible,
		// compatible and generate an additional constraint, or be
		// incompatible
		ConstraintMap extra;  // Just for this disjunct
		Exp *c;
		Exp *rem2 = d;
		bool unified = true;
		while (!!(c = nextConjunct(rem2))) {
			LOG << "   $$ c is " << *c << ", rem2 is " << (!rem2 ? "NULL" : rem2->prints()) << " $$\n";
			if (c->isFalse()) {
				unified = false;
				break;
			}
			assert(c->isEquality());
			Exp *lhs = ((Binary *)c)->getSubExp1();
			Exp *rhs = ((Binary *)c)->getSubExp2();
			extra.insert(lhs, rhs);
			auto kk = fixed.find(lhs);
			if (kk != fixed.end()) {
				unified &= unify(rhs, kk->second, extra);
				LOG << "Unified now " << unified << "; extra now " << extra.prints() << "\n";
				if (!unified) break;
			}
		}
		if (unified)
			// True if any disjuncts had all the conjuncts satisfied
			anyUnified = true;
		if (!unified) continue;
		// Use this disjunct
		// We can't just difference out extra if this fails; it may remove
		// elements from soln that should not be removed
		// So need a copy of the old set in oldSoln
		ConstraintMap oldSoln = soln;
		soln.makeUnion(extra);
		doSolve(++it, soln, solns);
		// Revert to the previous soln (whether doSolve returned true or not)
		// If this recursion did any good, it will have gotten to the end and
		// added the resultant soln to solns
		soln = oldSoln;
		LOG << "After doSolve returned: soln back to: " << soln.prints() << "\n";
		// Back to the current disjunction
		--it;
		// Continue for more disjuncts this disjunction
	}
	// We have run out of disjuncts. Return true if any disjuncts had no
	// unification failures
	LOG << "Exiting doSolve at level " << level-- << " returning " << anyUnified << "\n";
	return anyUnified;
}

bool
Constraints::unify(Exp *x, Exp *y, ConstraintMap &extra)
{
	LOG << "Unifying " << *x << " with " << *y << " result ";
	assert(x->isTypeVal());
	assert(y->isTypeVal());
	Type *xtype = ((TypeVal *)x)->getType();
	Type *ytype = ((TypeVal *)y)->getType();
	if (xtype->isPointer() && ytype->isPointer()) {
		Type *xPointsTo = ((PointerType *)xtype)->getPointsTo();
		Type *yPointsTo = ((PointerType *)ytype)->getPointsTo();
		if (((PointerType *)xtype)->pointsToAlpha()
		 || ((PointerType *)ytype)->pointsToAlpha()) {
			// A new constraint: xtype must be equal to ytype; at least
			// one of these is a variable type
			if (((PointerType *)xtype)->pointsToAlpha())
				extra.constrain(xPointsTo, yPointsTo);
			else
				extra.constrain(yPointsTo, xPointsTo);
			LOG << "true\n";
			return true;
		}
		LOG << (*xPointsTo == *yPointsTo) << "\n";
		return *xPointsTo == *yPointsTo;
	} else if (xtype->isSize()) {
		if (ytype->getSize() == 0) {   // Assume size=0 means unknown
			LOG << "true\n";
			return true;
		} else {
			LOG << (xtype->getSize() == ytype->getSize()) << "\n";
			return xtype->getSize() == ytype->getSize();
		}
	} else if (ytype->isSize()) {
		if (xtype->getSize() == 0) {   // Assume size=0 means unknown
			LOG << "true\n";
			return true;
		} else {
			LOG << (xtype->getSize() == ytype->getSize()) << "\n";
			return xtype->getSize() == ytype->getSize();
		}
	}
	// Otherwise, just compare the sizes
	LOG << (*xtype == *ytype) << "\n";
	return *xtype == *ytype;
}

void
Constraints::alphaSubst()
{
	for (auto &dis : disjunctions) {
		// This should be a conjuction of terms
		if (!dis->isConjunction())
			// A single term will do no good...
			continue;
		// Look for a term like alphaX* == fixedType*
		Exp *temp = dis->clone();
		Exp *term;
		bool found = false;
		Exp *trm1 = nullptr;
		Exp *trm2 = nullptr;
		Type *t1 = nullptr, *t2;
		while (!!(term = nextConjunct(temp))) {
			if (!term->isEquality())
				continue;
			trm1 = ((Binary *)term)->getSubExp1();
			if (!trm1->isTypeVal()) continue;
			trm2 = ((Binary *)term)->getSubExp2();
			if (!trm2->isTypeVal()) continue;
			// One of them has to be a pointer to an alpha
			t1 = ((TypeVal *)trm1)->getType();
			if (t1->isPointerToAlpha()) {
				found = true;
				break;
			}
			t2 = ((TypeVal *)trm2)->getType();
			if (t2->isPointerToAlpha()) {
				found = true;
				break;
			}
		}
		if (!found) continue;
		// We have a alpha value; get the value
		Exp *val, *alpha;
		if (t1->isPointerToAlpha()) {
			alpha = trm1;
			val = trm2;
		} else {
			val = trm1;
			alpha = trm2;
		}
		// Now substitute
		bool change;
		dis = dis->searchReplaceAll(alpha, val, change);
		dis = dis->simplifyConstraint();
	}
}

void
Constraints::print(std::ostream &os) const
{
	os << "\n" << (int)disjunctions.size() << " disjunctions: ";
	for (const auto &dis : disjunctions)
		os << *dis << ",\n";
	os << "\n";
	os << (int)fixed.size() << " fixed: ";
	fixed.print(os);
	os << (int)equates.size() << " equates: ";
	equates.print(os);
}

std::string
Constraints::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}
