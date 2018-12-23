/**
 * \file
 * \brief Definitions of classes used in SSL parsing.
 *
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2001, The University of Queensland
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef SSLINST_H
#define SSLINST_H

#include "register.h"
#include "rtl.h"
#include "type.h"
#include "types.h"

#include <iostream>
#include <ostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class Exp;
class Statement;

/**
 * Represents a single instruction - a string/RTL pair.
 */
class TableEntry {
public:
	TableEntry();
	TableEntry(const std::list<std::string> &);

	const TableEntry &operator =(const TableEntry &other);

	bool compareParam(const std::list<std::string> &);
	void appendRTL(RTL &);

public:
	std::list<std::string> params;
	RTL rtl;
};


typedef enum { PARAM_SIMPLE, PARAM_ASGN, PARAM_LAMBDA, PARAM_VARIANT } ParamKind;

/**
 * Represents the details of a single parameter.
 */
class ParamEntry {
public:
	ParamEntry() { }
	~ParamEntry() {
		delete type;
		delete regType;
	}

	std::list<std::string> params;      ///< PARAM_VARIANT & PARAM_ASGN only.
	std::list<std::string> funcParams;  ///< PARAM_LAMBDA - late bound params.
	Statement  *asgn = nullptr;         ///< PARAM_ASGN only.
	bool        lhs = false;            ///< True if this param ever appears on the LHS of an expression.
	ParamKind   kind = PARAM_SIMPLE;
	Type       *type = nullptr;
	Type       *regType = nullptr;      ///< Type of r[this], if any (void otherwise).
	std::set<int> regIdx;               ///< Values this param can take as an r[param].
	int         mark = 0;               ///< Traversal mark. (free temporary use, basically)
};


/**
 * The RTLInstDict represents a dictionary that maps instruction names to the
 * parameters they take and a template for the Exp list describing their
 * semantics.  It handles both the parsing of the SSL file that fills in the
 * dictionary entries as well as instantiation of an Exp list for a given
 * instruction name and list of actual parameters.
 */
class RTLInstDict {
public:
	RTLInstDict();
	~RTLInstDict();

	bool readSSLFile(const std::string &SSLFileName);

	void reset();

	std::pair<std::string, unsigned> getSignature(const std::string &name) const;

	bool appendToDict(const std::string &, const std::list<std::string> &, RTL &);

	RTL *instantiateRTL(ADDRESS, const std::string &, const std::vector<Exp *> &);
	RTL *instantiateRTL(ADDRESS, const RTL &, const std::list<std::string> &, const std::vector<Exp *> &);

	void transformPostVars(RTL &, bool);

	void print(std::ostream &os = std::cout) const;

	void addRegister(const std::string &name, int id, int size, bool flt);

	bool partialType(Exp *exp, Type &ty);

	void fixupParams();
	void fixupParamsSub(std::string s, std::list<std::string> &funcParams, bool &haveCount, int mark);

public:
	/**
	 * A map from the symbolic representation of a register (e.g. "%g0")
	 * to its index within an array of registers.
	 */
	std::map<std::string, int> RegMap;

	/**
	 * Similar to r_map but stores more info about a register such as its
	 * size, its addresss, etc. (see register.h).
	 */
	std::map<int, Register> DetRegMap;

	/**
	 * A map from symbolic representation of a special (non-addressable)
	 * register to a Register object.
	 */
	std::map<std::string, Register> SpecialRegMap;

	/**
	 * A set of parameter names, to make sure they are declared (?).
	 * Was map from string to SemTable index.
	 */
	std::set<std::string> ParamSet;

	/**
	 * Parameter (instruction operand, more like addressing mode) details
	 * (where given).
	 */
	std::map<std::string, ParamEntry> DetParamMap;

	/**
	 * The maps which summarise the semantics (.ssl) file.
	 */
	std::map<std::string, Exp *> FlagFuncs;

	/**
	 * Map from ordinary instruction to fast pseudo instruction, for use
	 * with -f (fast but not as exact) switch.
	 */
	std::map<std::string, std::string> fastMap;

	/**
	 * The actual dictionary.
	 */
	std::map<std::string, TableEntry> idict;

	/**
	 * An RTL describing the machine's basic fetch-execute cycle.
	 */
	RTL *fetchExecCycle;

	/**
	 * True if this source is big endian.
	 */
	bool bigEndian;
};

#endif
