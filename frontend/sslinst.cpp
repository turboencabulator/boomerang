/**
 * \file
 * \brief Defines the classes used to represent the semantic definitions of
 *        instructions and given in a .ssl file.
 *
 * \authors
 * Copyright (C) 1997, Shane Sendall
 * \authors
 * Copyright (C) 1998-1999, David Ung
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 * \authors
 * Copyright (C) 2014-2016, Kyle Guinn
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sslinst.h"

#include "boomerang.h"
#include "exp.h"
#include "register.h"
#include "sslparser.h"
#include "statement.h"
#include "type.h"
#include "types.h"
#include "util.h"

#include <algorithm>    // For std::remove(), std::transform()
#include <fstream>

#include <cassert>
#include <cctype>

//#define DEBUG_SSLPARSER 1
//#define DEBUG_POSTVAR 1

TableEntry::TableEntry(const std::list<std::string> &p) :
	params(p)
{
}

/**
 * Sets the contents of this object with a deepcopy from another TableEntry
 * object.  Note that this is different from the semantics of operator = for an
 * RTL which only does a shallow copy!
 *
 * \param other  The object to copy.
 *
 * \returns A reference to this object.
 */
TableEntry &
TableEntry::operator =(const TableEntry &other)
{
	if (this != &other) {
		params = other.params;
		rtl = other.rtl;
	}
	return *this;
}

/**
 * \brief Check if the parameters in this TableEntry match the given list.
 *
 * \param[in] p  List of formal parameters (as strings).
 *
 * \returns true if the parameter lists match.
 */
bool
TableEntry::compareParam(const std::list<std::string> &p)
{
	return params.size() == p.size()
	    && std::equal(params.cbegin(), params.cend(), p.cbegin());
}

/**
 * \brief Transfers Statements in an RTL to the end of this TableEntry.
 *
 * \param[in] r  RTL with list of Statements to move.
 */
void
TableEntry::appendRTL(RTL &r)
{
	rtl.splice(r);
}

/**
 * \brief Appends one RTL to the dictionary.
 *
 * Adds the RTL to idict if an entry does not already exist, otherwise
 * moves the RTL's Statements to the existing entry.
 *
 * \param[in] n  Name of the instruction to add to.
 * \param[in] p  List of formal parameters (as strings) for the RTL to add.
 * \param[in] r  The RTL to add.
 *
 * \returns true for success, false for failure.
 */
bool
RTLInstDict::appendToDict(const std::string &n, const std::list<std::string> &p, RTL &r)
{
	std::string opcode(n);
	std::transform(n.begin(), n.end(), opcode.begin(), toupper);
	opcode.erase(std::remove(opcode.begin(), opcode.end(), '.'), opcode.end());

	if (!idict.count(opcode)) {
		idict[opcode] = TableEntry(p);
	} else if (!idict[opcode].compareParam(p)) {
		return false;
	}
	idict[opcode].appendRTL(r);
	return true;
}

/**
 * \brief Parse a file containing a list of instructions definitions in SSL
 * format and build the contents of this dictionary.
 *
 * Read and parse the SSL file, and initialise the expanded instruction
 * dictionary (this object).  This also reads and sets up the register map and
 * flag functions.
 *
 * \param SSLFileName  The name of the file containing the SSL specification.
 *
 * \returns The file was successfully read.
 */
bool
RTLInstDict::readSSLFile(const std::string &SSLFileName)
{
	// Clear all state
	reset();

	addRegister("%CTI", -1, 1, false);
	addRegister("%NEXT", -1, 32, false);

	// Attempt to parse the SSL file
	std::ifstream ssl(SSLFileName);
	if (!ssl) {
		std::cerr << "can't open `" << SSLFileName << "' for reading\n";
		return false;
	}

#ifdef DEBUG_SSLPARSER
	SSLParser theParser(ssl, true);
#else
	SSLParser theParser(ssl, false);
#endif
	theParser.yyparse(*this);
	ssl.close();

	//fixupParams();

	if (Boomerang::get().debugDecoder) {
		std::cout << "\n=======Expanded RTL template dictionary=======\n";
		print();
		std::cout << "\n==============================================\n\n";
	}

	return true;
}

/**
 * \brief Add a new register to the machine.
 *
 * Add a new register definition to the dictionary.
 */
void
RTLInstDict::addRegister(const std::string &name, int id, int size, bool flt, int mappedIndex, int mappedOffset)
{
	RegMap[name] = id;
	if (id == -1) {
		auto &reg = SpecialRegMap[name];
		reg.s_name(name);
		reg.s_size(size);
		reg.s_float(flt);
		reg.s_mappedIndex(mappedIndex);
		reg.s_mappedOffset(mappedOffset);
	} else {
		auto &reg = DetRegMap[id];
		reg.s_name(name);
		reg.s_size(size);
		reg.s_float(flt);
		reg.s_mappedIndex(mappedIndex);
		reg.s_mappedOffset(mappedOffset);
	}
}

/**
 * Print a textual representation of the dictionary.
 *
 * \param os  Stream used for printing.
 */
void
RTLInstDict::print(std::ostream &os /*= std::cout*/) const
{
	for (const auto &p : idict) {
		// print the instruction name
		os << p.first;

		// print the parameters
		const std::list<std::string> &params = p.second.params;
		bool first = true;
		for (const auto &s : params) {
			os << (first ? "  " : ",") << s;
			first = false;
		}
		os << "\n";

		// print the RTL
		const RTL &rtlist = p.second.rtl;
		rtlist.print(os);
		os << "\n";
	}

#if 0
	// Detailed register map
	os << "\nDetailed register map\n";
	for (const auto &rr : DetRegMap) {
		int n = rr.first;
		auto &pr = rr.second;
		os << "number " << n
		   << " name " << pr.g_name()
		   << " size " << pr.g_size()
		   << " flt " << pr.g_float()
		   << " mappedIndex " << pr.g_mappedIndex()
		   << " mappedOffset " << pr.g_mappedOffset()
		   << "\n";
	}
#endif
}

#if 0 // Cruft?  Lambdas might be obsolete.
/**
 * \brief Go through the params and fixup any lambda functions.
 *
 * Runs after the ssl file is parsed to fix up variant params where the arms
 * are lambdas.
 */
void
RTLInstDict::fixupParams()
{
	for (auto &param : DetParamMap) {
		param.second.mark = 0;
	}
	int mark = 0;
	for (const auto &param : DetParamMap) {
		if (param.second.kind == ParamEntry::VARIANT) {
			std::list<std::string> funcParams;
			bool haveCount = false;
			fixupParamsSub(param.first, funcParams, haveCount, ++mark);
		}
	}
}

void
RTLInstDict::fixupParamsSub(std::string s, std::list<std::string> &funcParams, bool &haveCount, int mark)
{
	auto &param = DetParamMap[s];

	if (param.params.empty()) {
		std::cerr << "Error in SSL File: Variant operand " << s << " has no branches. Well that's really useful...\n";
		return;
	}
	if (param.mark == mark)
		return; /* Already seen this round. May indicate a cycle, but may not */

	param.mark = mark;

	for (const auto &p : param.params) {
		auto &sub = DetParamMap[p];
		if (sub.kind == ParamEntry::VARIANT) {
			fixupParamsSub(p, funcParams, haveCount, mark);
			if (!haveCount) { /* Empty branch? */
				continue;
			}
		} else if (!haveCount) {
			haveCount = true;
			for (unsigned i = 1; i <= sub.funcParams.size(); ++i) {
				char buf[10];
				sprintf(buf, "__lp%d", i);
				funcParams.push_back(buf);
			}
		}

		if (funcParams.size() != sub.funcParams.size()) {
			std::cerr << "Error in SSL File: Variant operand " << s
			          << " does not have a fixed number of functional parameters:\n"
			          << "Expected " << funcParams.size() << ", but branch " << p
			          << " has " << sub.funcParams.size() << ".\n";
		} else if (funcParams != sub.funcParams && sub.asgn) {
			/* Rename so all the parameter names match */
			for (auto i = funcParams.begin(), j = sub.funcParams.begin(); i != funcParams.end(); ++i, ++j) {
				auto match = Location::param(j->c_str());
				auto replace = Location::param(i->c_str());
				sub.asgn->searchAndReplace(match, replace);
			}
			sub.funcParams = funcParams;
		}
	}

#if 0
	if (param.funcParams.size() != funcParams.size())
		theSemTable.setItem(n, cFUNCTION, 0, 0, funcParams.size(), theSemTable[n].sName.c_str());
#endif
	param.funcParams = funcParams;
}
#endif

/**
 * \brief Returns the signature of the given instruction.
 *
 * \returns The signature (name + number of operands).
 */
std::pair<std::string, unsigned>
RTLInstDict::getSignature(const std::string &name) const
{
	// Take the argument, convert it to upper case and remove any .'s
	std::string opcode(name);
	std::transform(name.begin(), name.end(), opcode.begin(), toupper);
	opcode.erase(std::remove(opcode.begin(), opcode.end(), '.'), opcode.end());

	// Look up the dictionary
	auto it = idict.find(opcode);
	if (it == idict.end()) {
		std::cerr << "Error: no entry for `" << name << "' in RTL dictionary\n";
		it = idict.find("NOP");  // At least, don't cause segfault
	}

	std::pair<std::string, unsigned> ret;
	ret = std::pair<std::string, unsigned>(opcode, (it->second).params.size());
	return ret;
}

/**
 * \brief If the top level operator of the given expression indicates any kind
 * of type, update ty to match.
 *
 * Scan the Exp* pointed to by exp; if its top level operator indicates even a
 * partial type, then set the expression's type, and return true.
 *
 * \note This version only inspects one expression.
 *
 * \param exp  Points to a Exp* to be scanned.
 * \param ty   Ref to a Type object to put the partial type into.
 *
 * \returns true if a partial type is found.
 */
bool
RTLInstDict::partialType(Exp *exp, Type &ty)
{
	if (exp->isSizeCast()) {
		ty = IntegerType(((Const *)((Binary *)exp)->getSubExp1())->getInt());
		return true;
	}
	if (exp->isFltConst()) {
		ty = FloatType(64);
		return true;
	}
	return false;
}

/**
 * \brief Given an instruction name and list of actual parameters, return an
 * instantiated RTL for the corresponding instruction entry.
 *
 * Returns an instance of a register transfer list for the instruction named
 * 'name' with the actuals given as the second parameter.
 *
 * \param name     The name of the instruction
 *                 (must correspond to one defined in the SSL file).
 * \param actuals  The actual values.
 *
 * \returns The instantiated list of Exps.
 */
RTL *
RTLInstDict::instantiateRTL(ADDRESS natPC, const std::string &name, const std::vector<Exp *> &actuals)
{
	// If -f is in force, use the fast (but not as precise) name instead
	const std::string *lname = &name;
	// FIXME: settings
	//if (progOptions.fastInstr) {
	if (0) {
		auto itf = fastMap.find(name);
		if (itf != fastMap.end())
			lname = &itf->second;
	}
	// Retrieve the dictionary entry for the named instruction
	auto it = idict.find(*lname);
	if (it == idict.end()) { /* lname is not in dictionary */
		std::cerr << "ERROR: unknown instruction " << *lname << " at 0x" << std::hex << natPC << std::dec << ", ignoring.\n";
		return new RTL(natPC);  // FIXME:  Just return nullptr instead?
	}
	auto &entry = it->second;

	return instantiateRTL(natPC, entry.rtl, entry.params, actuals);
}

/**
 * \brief As above, but takes an RTL & param list directly rather than doing a
 * table lookup by name.
 *
 * Returns an instance of a register transfer list for the parameterized
 * rtlist with the given formals replaced with the actuals given as the third
 * parameter.
 *
 * \param tmpl     A register transfer list template.
 * \param params   A list of formal parameters.
 * \param actuals  The actual parameter values.
 *
 * \returns The instantiated list of Exps.
 */
RTL *
RTLInstDict::instantiateRTL(ADDRESS natPC, const RTL &tmpl, const std::list<std::string> &params, const std::vector<Exp *> &actuals)
{
	assert(params.size() == actuals.size());

	// Get a deep copy of the template RTL
	auto rtl = tmpl.clone();
	rtl->setAddress(natPC);

	// Search for the formals and replace them with the actuals
	auto actual = actuals.cbegin();
	for (const auto &param : params) {
		/* Simple parameter - just construct the formal to search for */
		Exp *formal = Location::param(param);
		rtl->searchAndReplace(formal, *actual++);
		//delete formal;
	}

	for (const auto &ss : *rtl) {
		ss->fixSuccessor();
		if (Boomerang::get().debugDecoder)
			std::cout << "\t\t\t" << *ss << "\n";
	}

	transformPostVars(*rtl, true);

	// Perform simplifications, e.g. *1 in Pentium addressing modes
	rtl->simplify();

	return rtl;
}

/**
 * Small struct for RTLInstDict::transformPostVars()
 */
class transPost {
public:
	bool used;      ///< If the base expression (e.g. r[0]) is used.
	                /**< Important because if not, we don't have to make any substitutions at all. */
	bool isNew;     ///< Not sure (MVE)
	Exp *tmp;       ///< The temp to replace r[0]' with.
	Exp *post;      ///< The whole postvar expression. e.g. r[0]'.
	Exp *base;      ///< The base expression (e.g. r[0]).
	Type *type;     ///< The type of the temporary (needed for the final assign).
};

/**
 * Transform the given list into another list which doesn't have
 * post-variables, by either adding temporaries or just removing them where
 * possible.  Modifies the list passed.
 * Second parameter indicates whether the routine should attempt to optimize
 * the resulting output, i.e. to minimize the number of temporaries.  This is
 * recommended for fully expanded expressions (i.e. within uqbt), but unsafe
 * otherwise.
 *
 * Note that the algorithm used expects to deal with simple expressions as
 * post vars, i.e.  r[22], m[r[1]], generally things which aren't
 * parameterized at a higher level.  This is ok for the translator (we do
 * substitution first anyway), but may miss some optimizations for the
 * emulator.  For the emulator, if parameters are detected within a postvar,
 * we just force the temporary, which is always safe to do.  (The parameter
 * optimise is set to false for the emulator to achieve this.)
 */
void
RTLInstDict::transformPostVars(RTL &rtl, bool optimise)
{
	// Map from var (could be any expression really) to details
	std::map<Exp *, transPost, lessExpStar> vars;
	int tmpcount = 1;  // For making temp names unique

#ifdef DEBUG_POSTVAR
	std::cout << "Transforming from:\n";
	rtl.print(std::cout);
#endif

	// First pass: Scan for post-variables and usages of their referents
	for (const auto &rt : rtl) {
		auto ss = std::list<Exp *>();
		if (auto as = dynamic_cast<Assign *>(rt)) {
			auto lhs = as->getLeft();
			auto rhs = as->getRight();

			// Look for assignments to post-variables
			if (lhs && lhs->isPostVar()) {
				if (!vars.count(lhs)) {
					// Add a record in the map for this postvar
					auto &el = vars[lhs];
					el.used = false;
					el.type = as->getType();

					// Constuct a temporary. We should probably be smarter and actually check that it's not otherwise
					// used here.
					std::string tmpname = el.type->getTempName() + (tmpcount++) + "post" ;
					el.tmp = Location::tempOf(new Const(tmpname));

					// Keep a copy of the referrent. For example, if the lhs is r[0]', base is r[0]
					el.base = lhs->getSubExp1();
					el.post = lhs;  // The whole post-var, e.g. r[0]'
					el.isNew = true;

					// The emulator generator sets optimise false
					// I think this forces always generating the temps (MVE)
					if (!optimise) {
						el.used = true;
						el.isNew = false;
					}

				}
			}
			// For an assignment, the two expressions to search are the left and right hand sides (could just put the
			// whole assignment on, I suppose)
			ss.push_back(lhs->clone());
			ss.push_back(rhs->clone());
		}

		/* Look for usages of post-variables' referents
		 * Trickier than you'd think, as we need to make sure to skip over the post-variables themselves. ie match
		 * r[0] but not r[0]'
		 * Note: back with SemStrs, we could use a match expression which was a wildcard prepended to the base
		 * expression; this would match either the base (r[0]) or the post-var (r[0]').
		 * Can't really use this with Exps, so we search twice; once for the base, and once for the post, and if we
		 * get more with the former, then we have a use of the base (consider r[0] + r[0]')
		 */
		for (auto &sr : vars) {
			if (sr.second.isNew) {
				// Make sure we don't match a var in its defining statement
				sr.second.isNew = false;
				continue;
			}
			if (sr.second.used)
				continue;  // Don't bother; already know it's used
			for (const auto &s : ss) {
				if (*s == *sr.second.base) {
					sr.second.used = true;
					break;
				}
				std::list<Exp *> res1, res2;
				s->searchAll(sr.second.base, res1);
				s->searchAll(sr.second.post, res2);
				// Each match of a post will also match the base.
				// But if there is a bare (non-post) use of the base, there will be a result in res1 that is not in res2
				if (res1.size() > res2.size()) {
					sr.second.used = true;
					break;
				}
			}
		}
	}

	// Second pass: Replace post-variables with temporaries where needed
	for (const auto &sr : vars) {
		if (sr.second.used) {
			rtl.searchAndReplace(sr.first, sr.second.tmp);
		} else {
			rtl.searchAndReplace(sr.first, sr.second.base);
		}
	}

	// Finally: Append assignments where needed from temps to base vars
	// Example: esp' = esp-4; m[esp'] = modrm; FLAG(esp)
	// all the esp' are replaced with say tmp1, you need a "esp = tmp1" at the end to actually make the change
	for (const auto &sr : vars) {
		if (sr.second.used) {
			auto te = new Assign(sr.second.type,
			                     sr.second.base->clone(),
			                     sr.second.tmp);
			rtl.getList().push_back(te);
		} else {
			// The temp is either used (uncloned) in the assignment, or is deleted here
			//delete sr.second.tmp;
		}
	}

#ifdef DEBUG_POSTVAR
	std::cout << "\nTo =>\n";
	rtl.print(std::cout);
	std::cout << "\n";
#endif
}

/**
 * \brief Reset the object to "undo" a readSSLFile().
 *
 * Call from test code if (e.g.) want to call readSSLFile() twice.
 */
void
RTLInstDict::reset()
{
	RegMap.clear();
	DetRegMap.clear();
	SpecialRegMap.clear();
	ParamSet.clear();
	//DetParamMap.clear();
	FlagFuncs.clear();
	fastMap.clear();
	idict.clear();
	fetchExecCycle = nullptr;
	bigEndian = false;
}
