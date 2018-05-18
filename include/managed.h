/**
 * \file
 * \brief Definition of "managed" classes such as StatementSet, which feature
 *        makeUnion, etc.
 *
 * Classes:
 * - StatementSet
 * - AssignSet
 * - StatementList
 * - StatementVec
 * - LocationSet
 * - //LocationList
 * - ConnectionGraph
 *
 * \authors
 * Copyright (C) 2003, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef MANAGED_H
#define MANAGED_H

#include "exphelp.h"  // For lessExpStar

#include <ostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

class Assign;
class Assignment;
//class Cfg;
class Exp;
class LocationSet;
class RefExp;
class Statement;

// A class to implement sets of statements
class StatementSet {
	std::set<Statement *> sset;  // For now, use use standard sets

public:
	typedef std::set<Statement *>::iterator iterator;
	typedef std::set<Statement *>::const_iterator const_iterator;
	typedef std::set<Statement *>::size_type size_type;

	           ~StatementSet() { }
	void        makeUnion(const StatementSet &other);   // Set union
	void        makeDiff(const StatementSet &other);    // Set difference
	void        makeIsect(const StatementSet &other);   // Set intersection
	bool        isSubSetOf(const StatementSet &other);  // Subset relation

	size_type   size() const     { return sset.size(); }   // Number of elements
	bool        empty() const    { return sset.empty(); }
	const_iterator begin() const { return sset.begin(); }
	const_iterator end() const   { return sset.end(); }
	iterator    begin()          { return sset.begin(); }
	iterator    end()            { return sset.end(); }

	void        insert(Statement *s) { sset.insert(s); }  // Insertion
	bool        remove(Statement *s);                     // Removal; rets false if not found
	bool        exists(Statement *s) const;               // Search; returns false if !found
	bool        definesLoc(Exp *loc) const;               // Search; returns true if any
	                                                      // statement defines loc
	void        clear() { sset.clear(); }                 // Clear the set
	bool        operator ==(const StatementSet &o) const { return sset == o.sset; }  // Compare if equal
	bool        operator <(const StatementSet &o) const;  // Compare if less

	friend std::ostream &operator <<(std::ostream &, const StatementSet &);
	void        printNums(std::ostream &os) const;        // Print statements as numbers
	std::string prints() const;                           // Print to string (for debug)
};

// As above, but the Statements are known to be Assigns, and are sorted sensibly
class AssignSet {
	std::set<Assign *, lessAssign> aset;  // For now, use use standard sets

public:
	typedef std::set<Assign *, lessAssign>::iterator iterator;
	typedef std::set<Assign *, lessAssign>::const_iterator const_iterator;
	typedef std::set<Assign *, lessAssign>::size_type size_type;

	           ~AssignSet() { }
	void        makeUnion(const AssignSet &other);     // Set union
	void        makeDiff(const AssignSet &other);      // Set difference
	void        makeIsect(const AssignSet &other);     // Set intersection
	bool        isSubSetOf(const AssignSet &other) const;  // Subset relation

	size_type   size() const     { return aset.size(); }   // Number of elements
	const_iterator begin() const { return aset.begin(); }
	const_iterator end() const   { return aset.end(); }
	iterator    begin()          { return aset.begin(); }
	iterator    end()            { return aset.end(); }

	void        insert(Assign *a) { aset.insert(a); }  // Insertion
	bool        remove(Assign *a);                     // Removal; rets false if not found
	bool        exists(Assign *s) const;               // Search; returns false if !found
	bool        definesLoc(Exp *loc) const;            // Search; returns true if any assignment defines loc
	Assign     *lookupLoc(Exp *loc) const;             // Search for loc on LHS, return ptr to Assign if found

	void        clear() { aset.clear(); }              // Clear the set
	bool        operator ==(const AssignSet &o) const { return aset == o.aset; }  // Compare if equal
	bool        operator <(const AssignSet &o) const;  // Compare if less

	friend std::ostream &operator <<(std::ostream &, const AssignSet &);
	void        printNums(std::ostream &os) const;     // Print statements as numbers
	std::string prints() const;                        // Print to string (for debug)
};

class StatementList {
	std::list<Statement *> slist;  // For now, use use standard list

public:
	typedef std::list<Statement *>::iterator iterator;
	typedef std::list<Statement *>::const_iterator const_iterator;
	typedef std::list<Statement *>::reverse_iterator reverse_iterator;
	typedef std::list<Statement *>::size_type size_type;

	           ~StatementList() { }
	size_type   size() const     { return slist.size(); }   // Number of elements
	bool        empty() const    { return slist.empty(); }
	const_iterator begin() const { return slist.begin(); }
	const_iterator end() const   { return slist.end(); }
	iterator    begin()          { return slist.begin(); }
	iterator    end()            { return slist.end(); }

	// A special intersection operator; this becomes the intersection of StatementList a (assumed to be a list of
	// Assignment*s) with the LocationSet b.
	// Used for calculating returns for a CallStatement
	void        makeIsect(const StatementList &a, const LocationSet &b);

	void        append(Statement *s) { slist.push_back(s); } // Insert at end
	void        append(const StatementList &sl);    // Append whole StatementList
	void        append(StatementSet &sl);           // Append whole StatementSet
	bool        remove(Statement *s);               // Removal; rets false if not found
	void        removeDefOf(Exp *loc);              // Remove definitions of loc
	// This one is needed where you remove in the middle of a loop
	// Use like this: it = mystatementlist.erase(it);
	iterator    erase(const_iterator it) { return slist.erase(it); }
	void        resize(size_type n) { slist.resize(n); }
	iterator    insert(iterator it, Statement *s) { return slist.insert(it, s); }
	std::string prints() const;                     // Print to string (for debugging)
	void        clear() { slist.clear(); }
	void        swap(StatementList &o) { slist.swap(o.slist); }
	void        makeCloneOf(const StatementList &o);// Make this a clone of o
	bool        existsOnLeft(Exp *loc) const;       // True if loc exists on the LHS of any Assignment in this list
	Assignment *findOnLeft(Exp *loc) const;         // Return the first stmt with loc on the LHS
};

class StatementVec {
	std::vector<Statement *> svec;  // For now, use use standard vector

public:
	typedef std::vector<Statement *>::iterator iterator;
	typedef std::vector<Statement *>::const_iterator const_iterator;
	typedef std::vector<Statement *>::size_type size_type;

	size_type   size() const { return svec.size(); }   // Number of elements
	// Get/put at position idx (0 based)
	Statement  *operator [](size_type idx) { return svec[idx]; }
	void        putAt(size_type idx, Statement *s);
	iterator    remove(const_iterator it) { return svec.erase(it); }
	std::string prints() const;                     // Print to string (for debugging)
	void        printNums(std::ostream &os) const;
	void        clear() { svec.clear(); }
	bool        operator ==(const StatementVec &o) const { return svec == o.svec; }  // Compare if equal
	bool        operator <(const StatementVec &o) const { return svec < o.svec; }    // Compare if less
	void        append(Statement *s) { svec.push_back(s); }
	iterator    erase(const_iterator it) { return svec.erase(it); }
};

// For various purposes, we need sets of locations (registers or memory)
class LocationSet {
	// We use a standard set, but with a special "less than" operator so that the sets are ordered
	// by expression value. If this is not done, then two expressions with the same value (say r[10])
	// but that happen to have different addresses (because they came from different statements)
	// would both be stored in the set (instead of the required set behaviour, where only one is stored)
	std::set<Exp *, lessExpStar> lset;
public:
	typedef std::set<Exp *, lessExpStar>::iterator iterator;
	typedef std::set<Exp *, lessExpStar>::const_iterator const_iterator;
	typedef std::set<Exp *, lessExpStar>::size_type size_type;

	            LocationSet() { }                       // Default constructor
	           ~LocationSet() { }                       // virtual destructor kills warning
	            LocationSet(const LocationSet &o);      // Copy constructor
	LocationSet &operator =(const LocationSet &o);      // Assignment
	void        makeUnion(const LocationSet &other);    // Set union
	void        makeDiff (const LocationSet &other);    // Set difference
	void        clear() { lset.clear(); }               // Clear the set
	const_iterator begin() const { return lset.begin(); }
	const_iterator end() const   { return lset.end(); }
	iterator    begin()          { return lset.begin(); }
	iterator    end()            { return lset.end(); }
	void        insert(Exp *loc) { lset.insert(loc); }  // Insert the given location
	void        remove(Exp *loc) { lset.erase(loc); }   // Remove the given location
	iterator    remove(const_iterator ll) { return lset.erase(ll); } // Remove location, given iterator
	void        removeIfDefines(const StatementSet &given);  // Remove locs defined in given
	size_type   size() const  { return lset.size(); }   // Number of elements
	bool        empty() const { return lset.empty(); }
	bool        operator ==(const LocationSet &o) const;// Compare
	void        substitute(Assign &a);                  // Substitute the given assignment to all

	friend std::ostream &operator <<(std::ostream &, const LocationSet &);
	std::string prints() const;                         // Print to string for debugging
	void        diff(LocationSet *o);                   // Diff 2 location sets to std::cerr
	bool        exists(Exp *e) const;                   // Return true if the location exists in the set
	Exp        *findNS(Exp *e) const;                   // Find location e (no subscripts); nullptr if not found
	bool        existsImplicit(Exp *e) const;           // Search for location e{-} or e{0} (e has no subscripts)
	// Return an iterator to the found item (or end() if not). Only really makes sense if e has a wildcard
	iterator    find(Exp *e) { return lset.find(e); }
	// Find a location with a different def, but same expression. For example, pass r28{10},
	// return true if r28{20} in the set. If return true, dr points to the first different ref
	bool        findDifferentRef(RefExp *e, Exp *&dr) const;
	void        addSubscript(Statement *def /* , Cfg *cfg */);  // Add a subscript to all elements
};

class Range {
public:
	static const int MAX = 2147483647;
	static const int MIN = -2147483647;

protected:
	int stride = 1;
	int lowerBound = MIN;
	int upperBound = MAX;
	Exp *base;

public:
	Range();
	Range(int stride, int lowerBound, int upperBound, Exp *base);

	Exp        *getBase() const { return base; }
	int         getStride() const { return stride; }
	int         getLowerBound() const { return lowerBound; }
	int         getUpperBound() const { return upperBound; }
	void        unionWith(const Range &r);
	void        widenWith(const Range &r);
	friend std::ostream &operator <<(std::ostream &, const Range &);
	bool        operator ==(const Range &) const;
};

class RangeMap {
protected:
	std::map<Exp *, Range, lessExpStar> ranges;

public:
	            RangeMap() { }
	void        addRange(Exp *loc, Range &r) { ranges[loc] = r; }
	bool        hasRange(Exp *loc) const { return !!ranges.count(loc); }
	Range      &getRange(Exp *loc);
	void        unionwith(const RangeMap &other);
	void        widenwith(const RangeMap &other);
	friend std::ostream &operator <<(std::ostream &, const RangeMap &);
	Exp        *substInto(Exp *e, std::set<Exp *, lessExpStar> *only = nullptr);
	void        killAllMemOfs();
	void        clear() { ranges.clear(); }
	bool        isSubset(RangeMap &other) const;
	bool        empty() const { return ranges.empty(); }
};

/// A class to store connections in a graph, e.g. for interferences of types or live ranges, or the phi_unite relation
/// that phi statements imply
/// If a is connected to b, then b is automatically connected to a
// This is implemented in a std::multimap, even though Appel suggests a bitmap (e.g. std::vector<bool> does this in a
// space efficient manner), but then you still need maps from expression to bit number. So here a standard map is used,
// and when a -> b is inserted, b->a is redundantly inserted.
class ConnectionGraph {
	std::multimap<Exp *, Exp *, lessExpStar> emap;  // The map
public:
	typedef std::multimap<Exp *, Exp *, lessExpStar>::iterator iterator;
	            ConnectionGraph() { }

	void        add(Exp *a, Exp *b);            // Add pair with check for existing
	void        connect(Exp *a, Exp *b);
	iterator    begin() { return emap.begin(); }
	iterator    end()   { return emap.end(); }
	int         count(Exp *a) const { return emap.count(a); }  // Return a count of locations connected to a
	bool        isConnected(Exp *a, Exp *b) const;  // Return true if a is connected to b
	// Update the map that used to be a <-> b, now it is a <-> c
	void        update(Exp *a, Exp *b, Exp *c);
	iterator    remove(iterator aa);            // Remove the mapping at *aa
};

#endif
