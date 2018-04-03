/**
 * \file
 * \brief Definition of the classes that describe an RTL, a low-level register
 *        transfer list.
 *
 * Higher-level RTLs (instance of class HLJump, HLCall, etc.) represent
 * information about a control transfer instruction (CTI) in the source
 * program.  Analysis code adds information to existing higher-level RTLs and
 * sometimes creates new higher-level RTLs (e.g. for switch statements).
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

#ifndef RTL_H
#define RTL_H

#include "register.h"
#include "type.h"
#include "types.h"

#include <iostream>
#include <ostream>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class BasicBlock;
class Exp;
class HLLCode;
class Statement;
class StmtVisitor;

/**
 * Describes low level register transfer lists (actually lists of statements).
 *
 * \todo When time permits, this class could be removed, replaced with new
 * Statements that mark the current native address.
 */
class RTL {
	        ADDRESS     nativeAddr = 0;           ///< RTL's source program instruction address.
	        std::list<Statement *> stmtList;  ///< List of expressions in this RTL.
public:
	                    RTL();
	                    RTL(ADDRESS instNativeAddr, std::list<Statement *> *listStmt = nullptr);
	                    RTL(const RTL &other);
	                   ~RTL();

	typedef std::list<Statement *>::iterator iterator;
	typedef std::list<Statement *>::reverse_iterator reverse_iterator;

	        RTL        *clone();
	        RTL        &operator =(RTL &other);

	        bool        accept(StmtVisitor *visitor);

	/**
	 * \name Common enquiry methods
	 * \{
	 */
	        ADDRESS     getAddress() const { return nativeAddr; }  ///< Return RTL's native address.
	        void        setAddress(ADDRESS a) { nativeAddr = a; }  ///< Set the address.
	        Type       *getType();
	        bool        areFlagsAffected();
	/** \} */

	/**
	 * \name Statement list enquiry methods
	 * \{
	 */
	        int         getNumStmt() const;
	        Statement  *elementAt(unsigned i);
	/** \} */

	/**
	 * \name Statement list editing methods
	 * \{
	 */
	        void        appendStmt(Statement *s);
	        void        prependStmt(Statement *s);
	        void        insertStmt(Statement *s, unsigned i);
	        void        insertStmt(Statement *s, iterator it);
	        void        updateStmt(Statement *s, unsigned i);
	        void        deleteStmt(unsigned int);
	        void        deleteLastStmt();
	        void        replaceLastStmt(Statement *repl);
	        void        clear();
	        void        appendListStmt(std::list<Statement *> &le);
	        void        appendRTL(RTL &rtl);
	        void        deepCopyList(std::list<Statement *> &dest);
	/** \} */

	        std::list<Statement *> &getList() { return stmtList; }  ///< Direct access to the list of expressions.

	        void        print(std::ostream &os = std::cout, bool html = false);

	        void        updateAddress(ADDRESS addr);

	        bool        isCompare(int &iReg, Exp *&pTerm);

#if 0 // Cruft?
	// Return true if RTL loads the high half of an immediate constant into anything. If so, loads the already
	// shifted high value into the parameter.
	        bool        isHiImmedLoad(ADDRESS &uHiHalf);

	// As above for low half. Extra parameters are required for SPARC, where bits are potentially transferred from
	// one register to another.
	        bool        isLoImmedLoad(ADDRESS &uLoHalf, bool &bTrans, int &iSrc);

	// Do a machine dependent, and a standard simplification of the RTL.
	        void        allSimplify();

	// Perform forward substitutions of temps, if possible. Called from the above
	        void        forwardSubs();
#endif

	        void        insertAssign(Exp *ssLhs, Exp *ssRhs, bool prep, Type *type = nullptr);
	        void        insertAfterTemps(Exp *ssLhs, Exp *ssRhs, Type *type = nullptr);

	        bool        searchAndReplace(Exp *search, Exp *replace);
	        bool        searchAll(Exp *search, std::list<Exp *> &result);

	        void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel);

	        void        simplify();

	        bool        isGoto();
	        bool        isCall();
	        bool        isBranch();

	        Statement  *getHlStmt();

	        std::string prints();

	        int         setConscripts(int n, bool bClear);
protected:

	friend class XMLProgParser;
};


/**
 * Represents a single instruction - a string/RTL pair.
 *
 * \todo This class plus ParamEntry and RTLInstDict should be moved to a
 * separate header file...
 */
class TableEntry {
public:
	TableEntry();
	TableEntry(std::list<std::string> &p, RTL &rtl);

	const TableEntry &operator =(const TableEntry &other);

	void setParam(std::list<std::string> &p);
	void setRTL(RTL &rtl);

	int appendRTL(std::list<std::string> &p, RTL &rtl);

public:
	std::list<std::string> params;
	RTL rtl;

#define TEF_NEXTPC 1
	int flags = 0;  // aka required capabilities
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

	std::pair<std::string, unsigned> getSignature(const std::string &name);

	int appendToDict(const std::string &n, std::list<std::string> &p, RTL &rtl);

	std::list<Statement *> *instantiateRTL(std::string &name, ADDRESS natPC, std::vector<Exp *> &actuals);
	std::list<Statement *> *instantiateRTL(RTL &rtls, ADDRESS natPC, std::list<std::string> &params, std::vector<Exp *> &actuals);

	std::list<Statement *> *transformPostVars(std::list<Statement *> *rts, bool optimise);

	void print(std::ostream &os = std::cout);

	void addRegister(const std::string &name, int id, int size, bool flt);

	bool partialType(Exp *exp, Type &ty);

	void fixupParams();

public:
	/**
	 * A map from the symbolic representation of a register (e.g. "%g0")
	 * to its index within an array of registers.
	 */
	std::map<std::string, int, std::less<std::string> > RegMap;

	/**
	 * Similar to r_map but stores more info about a register such as its
	 * size, its addresss, etc. (see register.h).
	 */
	std::map<int, Register, std::less<int> > DetRegMap;

	/**
	 * A map from symbolic representation of a special (non-addressable)
	 * register to a Register object.
	 */
	std::map<std::string, Register, std::less<std::string> > SpecialRegMap;

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
	std::map<std::string, std::pair<int, void *> *> DefMap;
	std::map<int, Exp *> AliasMap;

	/**
	 * Map from ordinary instruction to fast pseudo instruction, for use
	 * with -f (fast but not as exact) switch.
	 */
	std::map<std::string, std::string> fastMap;

	/**
	 * True if this source is big endian.
	 */
	bool bigEndian;

	/**
	 * The actual dictionary.
	 */
	std::map<std::string, TableEntry, std::less<std::string> > idict;

	/**
	 * An RTL describing the machine's basic fetch-execute cycle.
	 */
	RTL *fetchExecCycle;

	void fixupParamsSub(std::string s, std::list<std::string> &funcParams, bool &haveCount, int mark);
};

#endif
