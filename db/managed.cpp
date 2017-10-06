/**
 * \file
 * \brief Implementation of "managed" classes such as StatementSet, which
 *        feature makeUnion, etc.
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

#include "managed.h"

#include "boomerang.h"
#include "exp.h"
#include "log.h"
#include "proc.h"
#include "statement.h"

#include <algorithm>    // For std::min(), std::max()
#include <sstream>

std::ostream &
operator <<(std::ostream &os, const StatementSet &ss)
{
	for (auto it = ss.sset.cbegin(); it != ss.sset.cend(); ++it) {
		if (it != ss.sset.cbegin()) os << ",\t";
		os << *it;
	}
	return os << "\n";
}
std::ostream &
operator <<(std::ostream &os, const AssignSet &as)
{
	for (auto it = as.aset.cbegin(); it != as.aset.cend(); ++it) {
		if (it != as.aset.cbegin()) os << ",\t";
		os << *it;
	}
	return os << "\n";
}
std::ostream &
operator <<(std::ostream &os, const LocationSet &ls)
{
	for (auto it = ls.lset.cbegin(); it != ls.lset.cend(); ++it) {
		if (it != ls.lset.cbegin()) os << ",\t";
		os << *it;
	}
	return os;
}


//
// StatementSet methods
//

// Make this set the union of itself and other
void
StatementSet::makeUnion(const StatementSet &other)
{
	for (auto it = other.sset.begin(); it != other.sset.end(); ++it) {
		sset.insert(*it);
	}
}

// Make this set the difference of itself and other
void
StatementSet::makeDiff(const StatementSet &other)
{
	for (auto it = other.sset.begin(); it != other.sset.end(); ++it) {
		sset.erase(*it);
	}
}

// Make this set the intersection of itself and other
void
StatementSet::makeIsect(const StatementSet &other)
{
	for (auto it = sset.begin(); it != sset.end(); ) {
		auto ff = other.sset.find(*it);
		if (ff == other.sset.end()) {
			// Not in both sets
			it = sset.erase(it);
			continue;
		}
		++it;
	}
}

// Check for the subset relation, i.e. are all my elements also in the set
// other. Effectively (this intersect other) == this
bool
StatementSet::isSubSetOf(const StatementSet &other)
{
	return std::includes(other.sset.begin(), other.sset.end(), sset.begin(), sset.end());
}

// Remove this Statement. Return false if it was not found
bool
StatementSet::remove(Statement *s)
{
	return !!sset.erase(s);
}

// Search for s in this Statement set. Return true if found
bool
StatementSet::exists(Statement *s) const
{
	return sset.find(s) != sset.end();
}

// Find a definition for loc in this Statement set. Return true if found
bool
StatementSet::definesLoc(Exp *loc) const
{
	for (auto it = sset.begin(); it != sset.end(); ++it) {
		if ((*it)->definesLoc(loc))
			return true;
	}
	return false;
}

// Print to a string, for debugging
std::string
StatementSet::prints() const
{
	std::ostringstream ost;
	ost << *this;
	return ost.str();
}

// Print just the numbers to stream os
void
StatementSet::printNums(std::ostream &os) const
{
	os << std::dec;
	for (auto it = sset.begin(); it != sset.end(); ) {
		if (*it)
			(*it)->printNum(os);
		else
			os << "-";  // Special case for nullptr definition
		if (++it != sset.end())
			os << " ";
	}
}

bool
StatementSet::operator <(const StatementSet &o) const
{
	if (sset.size() < o.sset.size()) return true;
	if (sset.size() > o.sset.size()) return false;
	for (auto it1 = sset.cbegin(), it2 = o.sset.cbegin(); it1 != sset.cend(); ++it1, ++it2) {
		if (*it1 < *it2) return true;
		if (*it1 > *it2) return false;
	}
	return false;
}

//
// AssignSet methods
//

// Make this set the union of itself and other
void
AssignSet::makeUnion(const AssignSet &other)
{
	for (auto it = other.aset.begin(); it != other.aset.end(); ++it) {
		aset.insert(*it);
	}
}

// Make this set the difference of itself and other
void
AssignSet::makeDiff(const AssignSet &other)
{
	for (auto it = other.aset.begin(); it != other.aset.end(); ++it) {
		aset.erase(*it);
	}
}

// Make this set the intersection of itself and other
void
AssignSet::makeIsect(const AssignSet &other)
{
	for (auto it = aset.begin(); it != aset.end(); ) {
		auto ff = other.aset.find(*it);
		if (ff == other.aset.end()) {
			// Not in both sets
			it = aset.erase(it);
			continue;
		}
		++it;
	}
}

// Check for the subset relation, i.e. are all my elements also in the set
// other. Effectively (this intersect other) == this
bool
AssignSet::isSubSetOf(const AssignSet &other) const
{
	return std::includes(other.aset.begin(), other.aset.end(), aset.begin(), aset.end());
}

// Remove this Assign. Return false if it was not found
bool
AssignSet::remove(Assign *a)
{
	return !!aset.erase(a);
}

// Search for a in this Assign set. Return true if found
bool
AssignSet::exists(Assign *a) const
{
	return aset.find(a) != aset.end();
}

// Find a definition for loc in this Assign set. Return true if found
bool
AssignSet::definesLoc(Exp *loc) const
{
	Assign *as = new Assign(loc, new Terminal(opWild));
	auto ff = aset.find(as);
	return ff != aset.end();
}

// Find a definition for loc on the LHS in this Assign set. If found, return pointer to the Assign with that LHS
Assign *
AssignSet::lookupLoc(Exp *loc) const
{
	Assign *as = new Assign(loc, new Terminal(opWild));
	auto ff = aset.find(as);
	if (ff == aset.end()) return nullptr;
	return *ff;
}

// Print to a string, for debugging
std::string
AssignSet::prints() const
{
	std::ostringstream ost;
	ost << *this;
	return ost.str();
}

// Print just the numbers to stream os
void
AssignSet::printNums(std::ostream &os) const
{
	os << std::dec;
	for (auto it = aset.begin(); it != aset.end(); ) {
		if (*it)
			(*it)->printNum(os);
		else
			os << "-";  // Special case for nullptr definition
		if (++it != aset.end())
			os << " ";
	}
}

bool
AssignSet::operator <(const AssignSet &o) const
{
	if (aset.size() < o.aset.size()) return true;
	if (aset.size() > o.aset.size()) return false;
	for (auto it1 = aset.cbegin(), it2 = o.aset.cbegin(); it1 != aset.cend(); ++it1, ++it2) {
		if (*it1 < *it2) return true;
		if (*it1 > *it2) return false;
	}
	return false;
}

//
// LocationSet methods
//

// Assignment operator
LocationSet &
LocationSet::operator =(const LocationSet &o)
{
	lset.clear();
	for (auto it = o.lset.cbegin(); it != o.lset.cend(); ++it) {
		lset.insert((*it)->clone());
	}
	return *this;
}

// Copy constructor
LocationSet::LocationSet(const LocationSet &o)
{
	for (auto it = o.lset.cbegin(); it != o.lset.cend(); ++it)
		lset.insert((*it)->clone());
}

std::string
LocationSet::prints() const
{
	std::ostringstream ost;
	ost << *this;
	return ost.str();
}

void
LocationSet::remove(Exp *given)
{
	auto it = lset.find(given);
	if (it == lset.end()) return;
	//std::cerr << "LocationSet::remove at " << std::hex << (unsigned)this << " of " << *it << "\n";
	//std::cerr << "before: "; print();
	// NOTE: if the below uncommented, things go crazy. Valgrind says that
	// the deleted value gets used next in LocationSet::operator == ?!
	//delete *it;  // These expressions were cloned when created
	lset.erase(it);
	//std::cerr << "after : "; print();
}

// Remove locations defined by any of the given set of statements
// Used for killing in liveness sets
void
LocationSet::removeIfDefines(StatementSet &given)
{
	for (auto it = given.begin(); it != given.end(); ++it) {
		Statement *s = (Statement *)*it;
		LocationSet defs;
		s->getDefinitions(defs);
		for (auto dd = defs.begin(); dd != defs.end(); ++dd)
			lset.erase(*dd);
	}
}

// Make this set the union of itself and other
void
LocationSet::makeUnion(const LocationSet &other)
{
	for (auto it = other.lset.begin(); it != other.lset.end(); ++it) {
		lset.insert(*it);
	}
}

// Make this set the set difference of itself and other
void
LocationSet::makeDiff(const LocationSet &other)
{
	for (auto it = other.lset.begin(); it != other.lset.end(); ++it) {
		lset.erase(*it);
	}
}

bool
LocationSet::operator ==(const LocationSet &o) const
{
	// We want to compare the locations, not the pointers
	if (size() != o.size()) return false;
	for (auto it1 = lset.cbegin(), it2 = o.lset.cbegin(); it1 != lset.cend(); ++it1, ++it2) {
		if (!(**it1 == **it2)) return false;
	}
	return true;
}

bool
LocationSet::exists(Exp *e) const
{
	return lset.find(e) != lset.end();
}

// This set is assumed to be of subscripted locations (e.g. a Collector), and we want to find the unsubscripted
// location e in the set
Exp *
LocationSet::findNS(Exp *e) const
{
	// Note: can't search with a wildcard, since it doesn't have the weak ordering required (I think)
	RefExp r(e, nullptr);
	// Note: the below assumes that nullptr is less than any other pointer
	auto it = lset.lower_bound(&r);
	if (it == lset.end())
		return nullptr;
	if ((*((RefExp *)*it)->getSubExp1() == *e))
		return *it;
	else
		return nullptr;
}

// Given an unsubscripted location e, return true if e{-} or e{0} exists in the set
bool
LocationSet::existsImplicit(Exp *e) const
{
	RefExp r(e, nullptr);
	auto it = lset.lower_bound(&r);  // First element >= r
	// Note: the below relies on the fact that nullptr is less than any other pointer. Try later entries in the set:
	while (it != lset.end()) {
		if (!(*it)->isSubscript()) return false;        // Looking for e{something} (could be e.g. %pc)
		if (!(*((RefExp *)*it)->getSubExp1() == *e))    // Gone past e{anything}?
			return false;                               // Yes, then e{-} or e{0} cannot exist
		if (((RefExp *)*it)->isImplicitDef())           // Check for e{-} or e{0}
			return true;                                // Found
		++it;                                           // Else check next entry
	}
	return false;
}

// Find a location with a different def, but same expression. For example, pass r28{10},
// return true if r28{20} in the set. If return true, dr points to the first different ref
bool
LocationSet::findDifferentRef(RefExp *e, Exp *&dr) const
{
	RefExp search(e->getSubExp1()->clone(), (Statement *)-1);
	auto pos = lset.find(&search);
	if (pos == lset.end()) return false;
	while (pos != lset.end()) {
		// Exit if we've gone to a new base expression
		// E.g. searching for r13{10} and **pos is r14{0}
		// Note: we want a ref-sensitive compare, but with the outer refs stripped off
		// For example: m[r29{10} - 16]{any} is different from m[r29{20} - 16]{any}
		if (!(*(*pos)->getSubExp1() == *e->getSubExp1())) break;
		// Bases are the same; return true if only different ref
		if (!(**pos == *e)) {
			dr = *pos;
			return true;
		}
		++pos;
	}
	return false;
}

// Add a subscript (to definition d) to each element
void
LocationSet::addSubscript(Statement *d /* , Cfg *cfg */)
{
	std::set<Exp *, lessExpStar> newSet;
	for (auto it = lset.begin(); it != lset.end(); ++it)
		newSet.insert((*it)->expSubscriptVar(*it, d /* , cfg */));
	lset = newSet;  // Replace the old set!
	// Note: don't delete the old exps; they are copied in the new set
}

// Substitute s into all members of the set
void
LocationSet::substitute(Assign &a)
{
	Exp *lhs = a.getLeft();
	if (!lhs) return;
	Exp *rhs = a.getRight();
	if (!rhs) return;  // ? Will this ever happen?
	// Note: it's important not to change the pointer in the set of pointers to expressions, without removing and
	// inserting again. Otherwise, the set becomes out of order, and operations such as set comparison fail!
	// To avoid any funny behaviour when iterating the loop, we use the following two sets
	LocationSet removeSet;        // These will be removed after the loop
	LocationSet removeAndDelete;  // These will be removed then deleted
	LocationSet insertSet;        // These will be inserted after the loop
	bool change;
	for (auto it = lset.begin(); it != lset.end(); ++it) {
		Exp *loc = *it;
		Exp *replace;
		if (loc->search(lhs, replace)) {
			if (rhs->isTerminal()) {
				// This is no longer a location of interest (e.g. %pc)
				removeSet.insert(loc);
				continue;
			}
			loc = loc->clone()->searchReplaceAll(lhs, rhs, change);
			if (change) {
				loc = loc->simplifyArith();
				loc = loc->simplify();
				// If the result is no longer a register or memory (e.g.
				// r[28]-4), then delete this expression and insert any
				// components it uses (in the example, just r[28])
				if (!loc->isRegOf() && !loc->isMemOf()) {
					// Note: can't delete the expression yet, because the
					// act of insertion into the remove set requires silent
					// calls to the compare function
					removeAndDelete.insert(*it);
					loc->addUsedLocs(insertSet);
					continue;
				}
				// Else we just want to replace it
				// Regardless of whether the top level expression pointer has
				// changed, remove and insert it from the set of pointers
				removeSet.insert(*it);  // Note: remove the unmodified ptr
				insertSet.insert(loc);
			}
		}
	}
	makeDiff(removeSet);        // Remove the items to be removed
	makeDiff(removeAndDelete);  // These are to be removed as well
	makeUnion(insertSet);       // Insert the items to be added
	// Now delete the expressions that are no longer needed
	for (auto dd = removeAndDelete.lset.begin(); dd != removeAndDelete.lset.end(); ++dd)
		delete *dd;  // Plug that memory leak
}

//
// StatementList methods
//

bool
StatementList::remove(Statement *s)
{
	for (auto it = slist.begin(); it != slist.end(); ++it) {
		if (*it == s) {
			slist.erase(it);
			return true;
		}
	}
	return false;
}

void
StatementList::append(const StatementList &sl)
{
	for (auto it = sl.slist.begin(); it != sl.slist.end(); ++it) {
		slist.push_back(*it);
	}
}

void
StatementList::append(StatementSet &ss)
{
	for (auto it = ss.begin(); it != ss.end(); ++it) {
		slist.push_back(*it);
	}
}

std::string
StatementList::prints() const
{
	std::ostringstream ost;
	for (auto it = slist.begin(); it != slist.end(); ++it) {
		ost << *it << ",\t";
	}
	return ost.str();
}

//
// StatementVec methods
//

void
StatementVec::putAt(int idx, Statement *s)
{
	if (idx >= (int)svec.size())
		svec.resize(idx + 1, nullptr);
	svec[idx] = s;
}

std::string
StatementVec::prints() const
{
	std::ostringstream ost;
	for (auto it = svec.begin(); it != svec.end(); ++it) {
		ost << *it << ",\t";
	}
	return ost.str();
}

// Print just the numbers to stream os
void
StatementVec::printNums(std::ostream &os) const
{
	os << std::dec;
	for (auto it = svec.begin(); it != svec.end(); ) {
		if (*it)
			(*it)->printNum(os);
		else
			os << "-";  // Special case for no definition
		if (++it != svec.end())
			os << " ";
	}
}


// Special intersection method: this := a intersect b
void
StatementList::makeIsect(const StatementList &a, const LocationSet &b)
{
	slist.clear();
	for (auto it = a.slist.begin(); it != a.slist.end(); ++it) {
		Assignment *as = (Assignment *)*it;
		if (b.exists(as->getLeft()))
			slist.push_back(as);
	}
}

void
StatementList::makeCloneOf(const StatementList &o)
{
	slist.clear();
	for (auto it = o.slist.begin(); it != o.slist.end(); ++it)
		slist.push_back((*it)->clone());
}

// Return true if loc appears on the left of any statements in this list
// Note: statements in this list are assumed to be assignments
bool
StatementList::existsOnLeft(Exp *loc) const
{
	for (auto it = slist.begin(); it != slist.end(); ++it) {
		if (*((Assignment *)*it)->getLeft() == *loc)
			return true;
	}
	return false;
}

// Remove the first definition where loc appears on the left
// Note: statements in this list are assumed to be assignments
void
StatementList::removeDefOf(Exp *loc)
{
	for (auto it = slist.begin(); it != slist.end(); ++it) {
		if (*((Assignment *)*it)->getLeft() == *loc) {
			erase(it);
			return;
		}
	}
}

// Find the first Assignment with loc on the LHS
Assignment *
StatementList::findOnLeft(Exp *loc) const
{
	if (slist.empty())
		return nullptr;
	for (auto it = slist.begin(); it != slist.end(); ++it) {
		Exp *left = ((Assignment *)*it)->getLeft();
		if (*left == *loc)
			return (Assignment *)*it;
		if (left->isLocal()) {
			Location *l = (Location *)left;
			Exp *e = l->getProc()->expFromSymbol(((Const *)l->getSubExp1())->getStr());
			if (e && ((*e == *loc) || (e->isSubscript() && *e->getSubExp1() == *loc))) {
				return (Assignment *)*it;
			}
		}
	}
	return nullptr;
}

void
LocationSet::diff(LocationSet *o)
{
	bool printed2not1 = false;
	for (auto it = o->lset.begin(); it != o->lset.end(); ++it) {
		Exp *oe = *it;
		if (lset.find(oe) == lset.end()) {
			if (!printed2not1) {
				printed2not1 = true;
				std::cerr << "In set 2 but not set 1:\n";
			}
			std::cerr << oe << "\t";
		}
	}
	if (printed2not1)
		std::cerr << "\n";
	bool printed1not2 = false;
	for (auto it = lset.begin(); it != lset.end(); ++it) {
		Exp *e = *it;
		if (o->lset.find(e) == o->lset.end()) {
			if (!printed1not2) {
				printed1not2 = true;
				std::cerr << "In set 1 but not set 2:\n";
			}
			std::cerr << e << "\t";
		}
	}
	if (printed1not2)
		std::cerr << "\n";
}

Range::Range()
{
	base = new Const(0);
}

Range::Range(int stride, int lowerBound, int upperBound, Exp *base) :
	stride(stride),
	lowerBound(lowerBound),
	upperBound(upperBound),
	base(base)
{
	if (lowerBound == upperBound
	 && lowerBound == 0
	 && (base->getOper() == opMinus || base->getOper() == opPlus)
	 && base->getSubExp2()->isIntConst()) {
		this->lowerBound = ((Const *)base->getSubExp2())->getInt();
		if (base->getOper() == opMinus)
			this->lowerBound = -this->lowerBound;
		this->upperBound = this->lowerBound;
		this->base = base->getSubExp1();
	} else {
		if (!base)
			base = new Const(0);
		if (lowerBound > upperBound)
			this->upperBound = lowerBound;
		if (upperBound < lowerBound)
			this->lowerBound = upperBound;
	}
}

std::ostream &
operator <<(std::ostream &os, const Range &r)
{
	assert(r.lowerBound <= r.upperBound);
	if (r.base->isIntConst()
	 && ((Const *)r.base)->getInt() == 0
	 && r.lowerBound == r.MIN
	 && r.upperBound == r.MAX) {
		return os << "T";
	}
	bool needPlus = false;
	if (r.lowerBound == r.upperBound) {
		if (!r.base->isIntConst() || ((Const *)r.base)->getInt() != 0) {
			if (r.lowerBound != 0) {
				os << r.lowerBound;
				needPlus = true;
			}
		} else {
			needPlus = true;
			os << r.lowerBound;
		}
	} else {
		if (r.stride != 1)
			os << r.stride;
		os << "[";
		if (r.lowerBound == r.MIN)
			os << "-inf";
		else
			os << r.lowerBound;
		os << ", ";
		if (r.upperBound == r.MAX)
			os << "inf";
		else
			os << r.upperBound;
		os << "]";
		needPlus = true;
	}
	if (!r.base->isIntConst() || ((Const *)r.base)->getInt() != 0) {
		if (needPlus)
			os << " + ";
		r.base->print(os);
	}
	return os;
}

void
Range::unionWith(const Range &r)
{
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << "unioning " << *this << " with " << r << " got ";
	if (base->getOper() == opMinus
	 && r.base->getOper() == opMinus
	 && *base->getSubExp1() == *r.base->getSubExp1()
	 && base->getSubExp2()->isIntConst()
	 && r.base->getSubExp2()->isIntConst()) {
		int c1 = ((Const *)base->getSubExp2())->getInt();
		int c2 = ((Const *)r.base->getSubExp2())->getInt();
		if (c1 != c2) {
			if (lowerBound == r.lowerBound
			 && upperBound == r.upperBound
			 && lowerBound == 0) {
				lowerBound = std::min(-c1, -c2);
				upperBound = std::max(-c1, -c2);
				base = base->getSubExp1();
				if (VERBOSE && DEBUG_RANGE_ANALYSIS)
					LOG << *this << "\n";
				return;
			}
		}
	}
	if (!(*base == *r.base)) {
		stride = 1;
		lowerBound = MIN;
		upperBound = MAX;
		base = new Const(0);
		if (VERBOSE && DEBUG_RANGE_ANALYSIS)
			LOG << *this << "\n";
		return;
	}
	if (stride != r.stride)
		stride = std::min(stride, r.stride);
	if (lowerBound != r.lowerBound)
		lowerBound = std::min(lowerBound, r.lowerBound);
	if (upperBound != r.upperBound)
		upperBound = std::max(upperBound, r.upperBound);
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << *this << "\n";
}

void
Range::widenWith(const Range &r)
{
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << "widening " << *this << " with " << r << " got ";
	if (!(*base == *r.base)) {
		stride = 1;
		lowerBound = MIN;
		upperBound = MAX;
		base = new Const(0);
		if (VERBOSE && DEBUG_RANGE_ANALYSIS)
			LOG << *this << "\n";
		return;
	}
	// ignore stride for now
	if (r.getLowerBound() < lowerBound)
		lowerBound = MIN;
	if (r.getUpperBound() > upperBound)
		upperBound = MAX;
	if (VERBOSE && DEBUG_RANGE_ANALYSIS)
		LOG << *this << "\n";
}

Range &
RangeMap::getRange(Exp *loc)
{
	if (ranges.find(loc) == ranges.end()) {
		return *(new Range(1, Range::MIN, Range::MAX, new Const(0)));
	}
	return ranges[loc];
}

void
RangeMap::unionwith(const RangeMap &other)
{
	for (auto it = other.ranges.begin(); it != other.ranges.end(); ++it) {
		if (ranges.find(it->first) == ranges.end()) {
			ranges[it->first] = it->second;
		} else {
			ranges[it->first].unionWith(it->second);
		}
	}
}

void
RangeMap::widenwith(const RangeMap &other)
{
	for (auto it = other.ranges.begin(); it != other.ranges.end(); ++it) {
		if (ranges.find(it->first) == ranges.end()) {
			ranges[it->first] = it->second;
		} else {
			ranges[it->first].widenWith(it->second);
		}
	}
}

std::ostream &
operator <<(std::ostream &os, const RangeMap &rm)
{
	for (auto it = rm.ranges.begin(); it != rm.ranges.end(); ++it) {
		if (it != rm.ranges.begin())
			os << ", ";
		it->first->print(os);
		os << " -> " << it->second;
	}
	return os;
}

Exp *
RangeMap::substInto(Exp *e, std::set<Exp *, lessExpStar> *only)
{
	bool changes;
	int count = 0;
	do {
		changes = false;
		for (auto it = ranges.begin(); it != ranges.end(); ++it) {
			if (only && only->find(it->first) == only->end())
				continue;
			bool change = false;
			Exp *eold = e->clone();
			if (it->second.getLowerBound() == it->second.getUpperBound()) {
				e = e->searchReplaceAll(it->first, (new Binary(opPlus, it->second.getBase(), new Const(it->second.getLowerBound())))->simplify(), change);
			}
			if (change) {
				e = e->simplify()->simplifyArith();
				if (VERBOSE && DEBUG_RANGE_ANALYSIS)
					LOG << "applied " << it->first << " to " << eold << " to get " << e << "\n";
				changes = true;
			}
		}
		++count;
		assert(count < 5);
	} while (changes);
	return e;
}

void
RangeMap::killAllMemOfs()
{
	for (auto it = ranges.begin(); it != ranges.end(); ++it) {
		if (it->first->isMemOf()) {
			Range empty;
			it->second.unionWith(empty);
		}
	}
}

bool
Range::operator ==(const Range &other) const
{
	return stride == other.stride
	    && lowerBound == other.lowerBound
	    && upperBound == other.upperBound
	    && *base == *other.base;
}

// return true if this range map is a subset of the other range map
bool
RangeMap::isSubset(RangeMap &other) const
{
	for (auto it = ranges.begin(); it != ranges.end(); ++it) {
		if (other.ranges.find(it->first) == other.ranges.end()) {
			if (VERBOSE && DEBUG_RANGE_ANALYSIS)
				LOG << "did not find " << it->first << " in other, not a subset\n";
			return false;
		}
		Range &r = other.ranges[it->first];
		if (!(it->second == r)) {
			if (VERBOSE && DEBUG_RANGE_ANALYSIS)
				LOG << "range for " << it->first << " in other " << r << " is not equal to range in this " << it->second << ", not a subset\n";
			return false;
		}
	}
	return true;
}


// class ConnectionGraph

void
ConnectionGraph::add(Exp *a, Exp *b)
{
	auto ff = emap.find(a);
	while (ff != emap.end() && *ff->first == *a) {
		if (*ff->second == *b) return;  // Don't add a second entry
		++ff;
	}
	std::pair<Exp *, Exp *> pr;
	pr.first = a;
	pr.second = b;
	emap.insert(pr);
}

void
ConnectionGraph::connect(Exp *a, Exp *b)
{
	add(a, b);
	add(b, a);
}

int
ConnectionGraph::count(Exp *e) const
{
	auto ff = emap.find(e);
	int n = 0;
	while (ff != emap.end() && *ff->first == *e) {
		++n;
		++ff;
	}
	return n;
}

bool
ConnectionGraph::isConnected(Exp *a, Exp *b) const
{
	auto ff = emap.find(a);
	while (ff != emap.end() && *ff->first == *a) {
		if (*ff->second == *b)
			return true;  // Found the connection
		++ff;
	}
	return false;
}

// Modify the map so that a <-> b becomes a <-> c
void
ConnectionGraph::update(Exp *a, Exp *b, Exp *c)
{
	// find a->b
	auto ff = emap.find(a);
	while (ff != emap.end() && *ff->first == *a) {
		if (*ff->second == *b) {
			ff->second = c;  // Now a->c
			break;
		}
		++ff;
	}
	// find b -> a
	ff = emap.find(b);
	while (ff != emap.end() && *ff->first == *b) {
		if (*ff->second == *a) {
			emap.erase(ff);
			add(c, a);  // Now c->a
			break;
		}
		++ff;
	}
}

// Remove the mapping at *aa, and return a valid iterator for looping
ConnectionGraph::iterator
ConnectionGraph::remove(iterator aa)
{
	assert(aa != emap.end());
	Exp *b = aa->second;
	aa = emap.erase(aa);
	auto bb = emap.find(b);
	assert(bb != emap.end());
	if (bb == aa)
		++aa;
	emap.erase(bb);
	return aa;
}
