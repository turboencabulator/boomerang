/**
 * \file
 * \brief Definition of the class that describes an RTL, a low-level register
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

#include "types.h"

#include <iostream>
#include <ostream>
#include <list>
#include <string>

class BasicBlock;
class Exp;
class HLLCode;
class Statement;
class StmtVisitor;
class Type;

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
	                    RTL(ADDRESS);
	                    RTL(ADDRESS, Statement *);
	                    RTL(const RTL &);
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
	typedef std::list<Statement *>::const_iterator const_iterator;
	typedef std::list<Statement *>::reverse_iterator reverse_iterator;
	        const_iterator begin() const { return stmtList.begin(); }
	        const_iterator end() const   { return stmtList.end(); }
	        iterator    begin()          { return stmtList.begin(); }
	        iterator    end()            { return stmtList.end(); }
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
	/** \} */

	        void        print(std::ostream &os = std::cout, bool html = false) const;

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

#endif
