/**
 * \file
 * \brief Provides the definition for the signature classes.
 *
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef SIGNATURE_H
#define SIGNATURE_H

#include "exp.h"
#include "sigenum.h"  // For enums platform and cc
#include "type.h"

#include <ostream>
#include <exception>
#include <string>
#include <vector>

class Assignment;
class Cfg;
class Prog;
class Statement;
class StatementList;
class UserProc;

class Parameter {
	friend class XMLProgParser;

private:
	        Type       *type = nullptr;
	        std::string name;
	        Exp        *exp = nullptr;
	        std::string boundMax;

protected:
	                    Parameter() = default;
public:
	                    Parameter(Type *type, const std::string &name, Exp *exp = nullptr, const std::string &boundMax = "") : type(type), name(name), exp(exp), boundMax(boundMax) { }
	virtual            ~Parameter() { delete type; delete exp; }
	        bool        operator ==(const Parameter &other) const;
	        Parameter  *clone() const;

	        Type       *getType() const { return type; }
	        void        setType(Type *ty) { type = ty; }
	        const std::string &getName() const { return name; }
	        void        setName(const std::string &nam) { name = nam; }
	        Exp        *getExp() const { return exp; }
	        void        setExp(Exp *e) { exp = e; }

	// this parameter is the bound of another parameter with name nam
	        const std::string &getBoundMax() const { return boundMax; }
	        void        setBoundMax(const std::string &nam);
};

class Return {
	friend class XMLProgParser;

public:
	        Type       *type = nullptr;
	        Exp        *exp = nullptr;

	                    Return() = default;
	                    Return(Type *type, Exp *exp) : type(type), exp(exp) { }
	virtual            ~Return() = default;
	        bool        operator ==(const Return &other) const;
	        Return     *clone() const;
};

class Signature {
	friend class XMLProgParser;

protected:
	        std::string name;  // name of procedure
	        std::string sigFile;  // signature file this signature was read from (for libprocs)
	        std::vector<Parameter *> params;
	        //std::vector<ImplicitParameter *> implicitParams;
	        std::vector<Return *> returns;
	        Type       *rettype = nullptr;
	        bool        ellipsis = false;
	        bool        unknown = true;
	        //bool        bFullSig;  // True if have a full signature from a signature file etc
	        // True if the signature is forced with a -sf entry, or is otherwise known, e.g. WinMain
	        bool        forced = false;
	        Type       *preferedReturn = nullptr;
	        std::string preferedName;
	        std::vector<int> preferedParams;

	        //void        updateParams(UserProc *p, Statement *stmt, bool checkreach = true);
	        bool        usesNewParam(UserProc *p, Statement *stmt, bool checkreach, int &n);

	        //void        addImplicitParametersFor(Parameter *p);
	        //void        addImplicitParameter(Type *type, const char *name, Exp *e, Parameter *parent);

	                    Signature() = default;
public:
	                    Signature(const char *nam);
	// Platform plat, calling convention cc (both enums)
	// nam is name of the procedure (no longer stored in the Proc)
	static  Signature  *instantiate(platform plat, callconv cc, const char *nam);
	virtual            ~Signature() = default;

	virtual bool        operator ==(const Signature &other) const;

	// clone this signature
	virtual Signature  *clone() const;

	        bool        isUnknown() const { return unknown; }
	        void        setUnknown(bool b) { unknown = b; }
	        //void        setFullSig(bool full) { bFullSig = full; }
	        bool        isForced() const { return forced; }
	        void        setForced(bool f) { forced = f; }

	// get the return location
	virtual void        addReturn(Type *type, Exp *e = nullptr);
	virtual void        addReturn(Exp *e);
	virtual void        addReturn(Return *ret) { returns.push_back(ret); }
	virtual void        removeReturn(Exp *e);
	virtual unsigned    getNumReturns() const { return returns.size(); }
	virtual Exp        *getReturnExp(int n) { return returns[n]->exp; }
	        void        setReturnExp(int n, Exp *e) { returns[n]->exp = e; }
	virtual Type       *getReturnType(int n) { return returns[n]->type; }
	virtual void        setReturnType(int n, Type *ty);
	        int         findReturn(Exp *e);
	        //void        fixReturnsWithParameters();  // Needs description
	        void        setRetType(Type *t) { rettype = t; }
	        std::vector<Return *> &getReturns() { return returns; }
	        Type       *getTypeFor(Exp *e);

	// get/set the name
	virtual const std::string &getName();
	virtual void        setName(const std::string &);
	// get/set the signature file
	        const std::string &getSigFile() { return sigFile; }
	        void        setSigFile(const std::string &nam) { sigFile = nam; }

	// add a new parameter to this signature
	virtual void        addParameter(const char *nam = nullptr);
	virtual void        addParameter(Type *type, const char *nam = nullptr, Exp *e = nullptr, const char *boundMax = "");
	virtual void        addParameter(Exp *e, Type *ty);
	virtual void        addParameter(Parameter *param);
	        void        addEllipsis() { ellipsis = true; }
	        void        killEllipsis() { ellipsis = false; }
	virtual void        removeParameter(Exp *e);
	virtual void        removeParameter(int i);
	// set the number of parameters using defaults
	virtual void        setNumParams(int n);

	// accessors for parameters
	virtual unsigned    getNumParams() const { return params.size(); }
	virtual const char *getParamName(int n);
	virtual Exp        *getParamExp(int n);
	virtual Type       *getParamType(int n);
	virtual const char *getParamBoundMax(int n);
	virtual void        setParamType(int, Type *);
	virtual void        setParamType(const std::string &, Type *);
	virtual void        setParamType(Exp *, Type *);
	virtual void        setParamName(int n, const std::string &nam);
	virtual void        setParamExp(int n, Exp *e);
	virtual int         findParam(Exp *e);
	virtual int         findParam(const std::string &nam);
	// accessor for argument expressions
	virtual Exp        *getArgumentExp(int n);
	virtual bool        hasEllipsis() const { return ellipsis; }

	        void        renameParam(const std::string &oldName, const std::string &newName);

	// analysis determines parameters / return type
	//virtual void        analyse(UserProc *p);

	// Data flow based type analysis. Meet the parameters with their current types.  Returns true if a change
	        bool        dfaTypeAnalysis(Cfg *cfg);

	// any signature can be promoted to a higher level signature, if available
	virtual Signature  *promote(UserProc *p);
	        void        print(std::ostream &out, bool html = false) const;
	        std::string prints() const;  // For debugging

	// Get a wildcard to find stack locations
	virtual Exp        *getStackWildcard() { return nullptr; }
	class StackRegisterNotDefinedException : public std::exception {
	public:
		StackRegisterNotDefinedException() { }
	};
	virtual int         getStackRegister() throw (StackRegisterNotDefinedException);
	static  int         getStackRegister(Prog *prog) throw (StackRegisterNotDefinedException);
	// Does expression e represent a local stack-based variable?
	// Result can be ABI specific, e.g. sparc has locals in the parent's stack frame, at POSITIVE offsets from the
	// stack pointer register
	// Also, I believe that the PA/RISC stack grows away from 0
	        bool        isStackLocal(Prog *prog, Exp *e);
	// Similar to the above, but checks for address of a local (i.e. sp{0} -/+ K)
	virtual bool        isAddrOfStackLocal(Prog *prog, Exp *e);
	// For most machines, local variables are always NEGATIVE offsets from sp
	virtual bool        isLocalOffsetNegative() { return true; }
	// For most machines, local variables are not POSITIVE offsets from sp
	virtual bool        isLocalOffsetPositive() { return false; }
	// Is this operator (between the stack pointer and a constant) compatible with a stack local pattern?
	        bool        isOpCompatStackLocal(OPER op);

	// get anything that can be proven as a result of the signature
	virtual Exp        *getProven(Exp *left) { return nullptr; }
	virtual bool        isPreserved(Exp *e) { return false; }  // Return whether e is preserved by this proc
	virtual void        setLibraryDefines(StatementList *defs) { }  // Set the locations defined by library calls
	static  void        setABIdefines(Prog *prog, StatementList *defs);

	// Return true if this is a known machine (e.g. SparcSignature as opposed to Signature)
	virtual bool        isPromoted() { return false; }
	// Return true if this has a full blown signature, e.g. main/WinMain etc.
	// Note that many calls to isFullSignature were incorrectly calls to isPromoted()
	        //bool        isFullSignature() { return bFullSig; }

	// ascii versions of platform, calling convention name
	static  const char *platformName(platform plat);
	static  const char *conventionName(callconv cc);
	virtual platform    getPlatform() { return PLAT_GENERIC; }
	virtual callconv    getConvention() { return CONV_NONE; }

	// prefered format
	        void        setPreferedReturn(Type *ty) { preferedReturn = ty; }
	        void        setPreferedName(const std::string &nam) { preferedName = nam; }
	        void        addPreferedParameter(int n) { preferedParams.push_back(n); }
	        Type       *getPreferedReturn() { return preferedReturn; }
	        const std::string &getPreferedName() { return preferedName; }
	        unsigned int getNumPreferedParams() const { return preferedParams.size(); }
	        int         getPreferedParam(int n) { return preferedParams[n]; }

	// A compare function for arguments and returns. Used for sorting returns in calcReturn() etc
	virtual bool        argumentCompare(const Assignment &a, const Assignment &b);
	virtual bool        returnCompare(const Assignment &a, const Assignment &b);

protected:
	        void        appendParameter(Parameter *p) { params.push_back(p); }
	        //void        appendImplicitParameter(ImplicitParameter *p) { implicitParams.push_back(p); }
	        void        appendReturn(Return *r) { returns.push_back(r); }
};

class CustomSignature : public Signature {
protected:
	int         sp = 0;
public:
	            CustomSignature(const char *nam);
	bool        isPromoted() override { return true; }
	Signature  *clone() const override;
	void        setSP(int nsp);
	int         getStackRegister() throw (StackRegisterNotDefinedException) override { return sp; };
};

#endif
