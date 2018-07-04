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
	                    RTL(ADDRESS instNativeAddr, const std::list<Statement *> *listStmt = nullptr);
	                    RTL(const RTL &other);
	                   ~RTL();

	        RTL        *clone() const;
	        RTL        &operator =(const RTL &other);

	        bool        accept(StmtVisitor &);

	/**
	 * \name Common enquiry methods
	 * \{
	 */
	        ADDRESS     getAddress() const { return nativeAddr; }  ///< Return RTL's native address.
	        void        setAddress(ADDRESS a) { nativeAddr = a; }  ///< Set the address.
	        Type       *getType() const;
	        bool        areFlagsAffected() const;
	/** \} */

	/**
	 * \name Statement list enquiry methods
	 * \{
	 */
	        int         getNumStmt() const;
	        Statement  *elementAt(unsigned i) const;
	        std::list<Statement *> &getList() { return stmtList; }  ///< Direct access to the list of expressions.
	typedef std::list<Statement *>::iterator iterator;
	typedef std::list<Statement *>::reverse_iterator reverse_iterator;

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
	        void        append(const std::list<Statement *> &);
	        void        append(const RTL &);
	        void        deepCopyList(std::list<Statement *> &dest) const;
	/** \} */

	        void        print(std::ostream &os = std::cout, bool html = false) const;

	        void        updateAddress(ADDRESS addr);

	        void        insertAssign(Exp *ssLhs, Exp *ssRhs, bool prep, Type *type = nullptr);
	        void        insertAfterTemps(Exp *ssLhs, Exp *ssRhs, Type *type = nullptr);

	        bool        searchAndReplace(Exp *search, Exp *replace);
	        bool        searchAll(Exp *search, std::list<Exp *> &result);

	        void        generateCode(HLLCode *hll, BasicBlock *pbb, int indLevel) const;

	        void        simplify();

	        bool        isCompare(int &iReg, Exp *&pTerm) const;
	        bool        isGoto() const;
	        bool        isCall() const;
	        bool        isBranch() const;

	        Statement  *getHlStmt() const;

	        std::string prints() const;

	        int         setConscripts(int n, bool bClear);
protected:

	friend class XMLProgParser;
};

std::ostream & operator <<(std::ostream &, const RTL *) __attribute__((deprecated));
std::ostream & operator <<(std::ostream &, const RTL &);


/**
 * Represents a single instruction - a string/RTL pair.
 *
 * \todo This class plus ParamEntry and RTLInstDict should be moved to a
 * separate header file...
 */
class TableEntry {
public:
	TableEntry();
	TableEntry(const std::list<std::string> &p, const RTL &rtl);

	const TableEntry &operator =(const TableEntry &other);

	void setParam(const std::list<std::string> &p);
	void setRTL(const RTL &rtl);

	bool appendRTL(const std::list<std::string> &, const RTL &);

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

	std::pair<std::string, unsigned> getSignature(const std::string &name) const;

	bool appendToDict(const std::string &, const std::list<std::string> &, const RTL &);

	std::list<Statement *> *instantiateRTL(const std::string &name, ADDRESS natPC, const std::vector<Exp *> &actuals);
	std::list<Statement *> *instantiateRTL(const RTL &rtls, ADDRESS natPC, const std::list<std::string> &params, const std::vector<Exp *> &actuals);

	std::list<Statement *> *transformPostVars(std::list<Statement *> *rts, bool optimise);

	void print(std::ostream &os = std::cout) const;

	void addRegister(const std::string &name, int id, int size, bool flt);

	bool partialType(Exp *exp, Type &ty);

	void fixupParams();

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
	 * True if this source is big endian.
	 */
	bool bigEndian;

	/**
	 * The actual dictionary.
	 */
	std::map<std::string, TableEntry> idict;

	/**
	 * An RTL describing the machine's basic fetch-execute cycle.
	 */
	RTL *fetchExecCycle;

	void fixupParamsSub(std::string s, std::list<std::string> &funcParams, bool &haveCount, int mark);
};

#endif
