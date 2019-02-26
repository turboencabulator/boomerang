/**
 * \file
 * \brief Implementation of the classes that describe a procedure signature.
 *
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "signature.h"

#include "boomerang.h"
#include "exp.h"
#include "managed.h"
#include "proc.h"
#include "prog.h"
#include "type.h"

#include <sstream>

#include <cassert>
#include <cstring>

const char *
Signature::platformName(platform plat)
{
	switch (plat) {
	case PLAT_PENTIUM:  return "pentium";
	case PLAT_SPARC:    return "sparc";
	case PLAT_M68K:     return "m68k";
	case PLAT_PARISC:   return "parisc";
	case PLAT_PPC:      return "ppc";
	case PLAT_MIPS:     return "mips";
	case PLAT_ST20:     return "st20";
	default:            return "???";
	}
}

const char *
Signature::conventionName(callconv cc)
{
	switch (cc) {
	case CONV_C:        return "stdc";
	case CONV_PASCAL:   return "pascal";
	case CONV_THISCALL: return "thiscall";
	default:            return "??";
	}
}

namespace CallingConvention {

	// Win32Signature is for non-thiscall signatures: all parameters pushed
	class Win32Signature : public Signature {
	public:
		            Win32Signature(const char *nam);
		            Win32Signature(Signature &old);
		Signature  *clone() const override;
		bool        operator ==(const Signature &other) const override;
		static bool qualified(UserProc *p, Signature &candidate);

		void        addReturn(Type *type, Exp *e = nullptr) override;
		void        addParameter(Type *type, const char *nam = nullptr, Exp *e = nullptr, const char *boundMax = "") override;
		Exp        *getArgumentExp(int n) override;

		Signature  *promote(UserProc *p) override;
		Exp        *getStackWildcard() override;
		int         getStackRegister() throw (StackRegisterNotDefinedException) override { return 28; }
		Exp        *getProven(Exp *left) override;
		bool        isPreserved(Exp *e) override;  // Return whether e is preserved by this proc
		void        setLibraryDefines(StatementList *defs) override;  // Set list of locations def'd by library calls

		bool        isPromoted() override { return true; }
		platform    getPlatform() override { return PLAT_PENTIUM; }
		callconv    getConvention() override { return CONV_PASCAL; }
	};

	// Win32TcSignature is for "thiscall" signatures, i.e. those that have register ecx as the first parameter
	// Only needs to override a few member functions; the rest can inherit from Win32Signature
	class Win32TcSignature : public Win32Signature {
	public:
		            Win32TcSignature(const char *nam);
		            Win32TcSignature(Signature &old);
		Exp        *getArgumentExp(int n) override;
		Exp        *getProven(Exp *left) override;
		Signature  *clone() const override;
		platform    getPlatform() override { return PLAT_PENTIUM; }
		callconv    getConvention() override { return CONV_THISCALL; }
	};


	namespace StdC {
		class PentiumSignature : public Signature {
		public:
			            PentiumSignature(const char *nam);
			            PentiumSignature(Signature &old);
			Signature  *clone() const override;
			bool        operator ==(const Signature &other) const override;
			static bool qualified(UserProc *p, Signature &candidate);

			void        addReturn(Type *type, Exp *e = nullptr) override;
			void        addParameter(Type *type, const char *nam = nullptr, Exp *e = nullptr, const char *boundMax = "") override;
			Exp        *getArgumentExp(int n) override;

			Signature  *promote(UserProc *p) override;
			Exp        *getStackWildcard() override;
			int         getStackRegister() throw (StackRegisterNotDefinedException) override { return 28; }
			Exp        *getProven(Exp *left) override;
			bool        isPreserved(Exp *e) override;  // Return whether e is preserved by this proc
			void        setLibraryDefines(StatementList *defs) override;  // Set list of locations def'd by library calls
			bool        isPromoted() override { return true; }
			platform    getPlatform() override { return PLAT_PENTIUM; }
			callconv    getConvention() override { return CONV_C; }
			bool        returnCompare(const Assignment &a, const Assignment &b) override;
			bool        argumentCompare(const Assignment &a, const Assignment &b) override;
		};

		class SparcSignature : public Signature {
		public:
			            SparcSignature(const char *nam);
			            SparcSignature(Signature &old);
			Signature  *clone() const override;
			bool        operator ==(const Signature &other) const override;
			static bool qualified(UserProc *p, Signature &candidate);

			void        addReturn(Type *type, Exp *e = nullptr) override;
			void        addParameter(Type *type, const char *nam = nullptr, Exp *e = nullptr, const char *boundMax = "") override;
			Exp        *getArgumentExp(int n) override;

			Signature  *promote(UserProc *p) override;
			Exp        *getStackWildcard() override;
			int         getStackRegister() throw (StackRegisterNotDefinedException) override { return 14; }
			Exp        *getProven(Exp *left) override;
			bool        isPreserved(Exp *e) override;  // Return whether e is preserved by this proc
			void        setLibraryDefines(StatementList *defs) override;  // Set list of locations def'd by library calls
			// Stack offsets can be negative (inherited) or positive:
			bool        isLocalOffsetPositive() override { return true; }
			// An override for testing locals
			bool        isAddrOfStackLocal(Prog *prog, Exp *e) override;
			bool        isPromoted() override { return true; }
			platform    getPlatform() override { return PLAT_SPARC; }
			callconv    getConvention() override { return CONV_C; }
			bool        returnCompare(const Assignment &a, const Assignment &b) override;
			bool        argumentCompare(const Assignment &a, const Assignment &b) override;
		};

		class SparcLibSignature : public SparcSignature {
		public:
			            SparcLibSignature(const char *nam) : SparcSignature(nam) { }
			            SparcLibSignature(Signature &old);
			Signature  *clone() const override;
			Exp        *getProven(Exp *left) override;
		};

		class PPCSignature : public Signature {
		public:
			            PPCSignature(const char *name);
			            PPCSignature(Signature &old);
			Signature  *clone() const override;
			static bool qualified(UserProc *p, Signature &candidate);
			void        addReturn(Type *type, Exp *e = nullptr) override;
			Exp        *getArgumentExp(int n) override;
			void        addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/) override;
			Exp        *getStackWildcard() override;
			int         getStackRegister() throw (StackRegisterNotDefinedException) override { return 1; }
			Exp        *getProven(Exp *left) override;
			bool        isPreserved(Exp *e) override;  // Return whether e is preserved by this proc
			void        setLibraryDefines(StatementList *defs) override;  // Set list of locations def'd by library calls
			bool        isLocalOffsetPositive() override { return true; }
			bool        isPromoted() override { return true; }
			platform    getPlatform() override { return PLAT_PPC; }
			callconv    getConvention() override { return CONV_C; }
		};

		class MIPSSignature : public Signature {
		public:
			            MIPSSignature(const char *name);
			            MIPSSignature(Signature &old);
			Signature  *clone() const override;
			static bool qualified(UserProc *p, Signature &candidate);
			void        addReturn(Type *type, Exp *e = nullptr) override;
			Exp        *getArgumentExp(int n) override;
			void        addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/) override;
			Exp        *getStackWildcard() override;
			int         getStackRegister() throw (StackRegisterNotDefinedException) override { return 1; }
			Exp        *getProven(Exp *left) override;
			bool        isPreserved(Exp *e) override;  // Return whether e is preserved by this proc
			void        setLibraryDefines(StatementList *defs) override;  // Set list of locations def'd by library calls
			bool        isLocalOffsetPositive() override { return true; }
			bool        isPromoted() override { return true; }
			platform    getPlatform() override { return PLAT_MIPS; }
			callconv    getConvention() override { return CONV_C; }
		};

		class ST20Signature : public Signature {
		public:
			            ST20Signature(const char *name);
			            ST20Signature(Signature &old);
			Signature  *clone() const override;
			bool        operator ==(const Signature &other) const override;
			static bool qualified(UserProc *p, Signature &candidate);

			void        addReturn(Type *type, Exp *e = nullptr) override;
			void        addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/) override;
			Exp        *getArgumentExp(int n) override;

			Signature  *promote(UserProc *p) override;
			Exp        *getStackWildcard() override;
			int         getStackRegister() throw (StackRegisterNotDefinedException) override { return 3; }
			Exp        *getProven(Exp *left) override;
			bool        isPromoted() override { return true; }
			//bool        isLocalOffsetPositive() override { return true; }
			platform    getPlatform() override { return PLAT_ST20; }
			callconv    getConvention() override { return CONV_C; }
		};
	}
}

CallingConvention::Win32Signature::Win32Signature(const char *nam) :
	Signature(nam)
{
	Signature::addReturn(Location::regOf(28));
	// Signature::addImplicitParameter(new PointerType(new IntegerType()), "esp", Location::regOf(28), nullptr);
}

CallingConvention::Win32Signature::Win32Signature(Signature &old) :
	Signature(old)
{
}

CallingConvention::Win32TcSignature::Win32TcSignature(const char *nam) :
	Win32Signature(nam)
{
	Signature::addReturn(Location::regOf(28));
	// Signature::addImplicitParameter(new PointerType(new IntegerType()), "esp", Location::regOf(28), nullptr);
}

CallingConvention::Win32TcSignature::Win32TcSignature(Signature &old) :
	Win32Signature(old)
{
}

static void
cloneVec(const std::vector<Parameter *> &from, std::vector<Parameter *> &to)
{
	to.clear();
	to.reserve(from.size());
	for (const auto &param : from)
		to.push_back(param->clone());
}

static void
cloneVec(const std::vector<Return *> &from, std::vector<Return *> &to)
{
	to.clear();
	to.reserve(from.size());
	for (const auto &ret : from)
		to.push_back(ret->clone());
}

Parameter *
Parameter::clone() const
{
	return new Parameter(type->clone(), name, exp->clone(), boundMax);
}

void
Parameter::setBoundMax(const std::string &nam)
{
	boundMax = nam;
}

Signature *
CallingConvention::Win32Signature::clone() const
{
	auto n = new Win32Signature(name.c_str());
	cloneVec(params, n->params);
	// cloneVec(implicitParams, n->implicitParams);
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	return n;
}

Signature *
CallingConvention::Win32TcSignature::clone() const
{
	auto n = new Win32TcSignature(name.c_str());
	cloneVec(params, n->params);
	// cloneVec(implicitParams, n->implicitParams);
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	return n;
}

bool
CallingConvention::Win32Signature::operator ==(const Signature &other) const
{
	return Signature::operator ==(other);
}

static Exp *savedReturnLocation = Location::memOf(Location::regOf(28));
static Exp *stackPlusFour = new Binary(opPlus, Location::regOf(28), new Const(4));

bool
CallingConvention::Win32Signature::qualified(UserProc *p, Signature &candidate)
{
	platform plat = p->getProg()->getFrontEndId();
	if (plat != PLAT_PENTIUM || !p->getProg()->isWin32()) return false;

	if (VERBOSE)
		LOG << "consider promotion to stdc win32 signature for " << p->getName() << "\n";

	bool gotcorrectret1, gotcorrectret2 = false;
	Exp *provenPC = p->getProven(new Terminal(opPC));
	gotcorrectret1 = provenPC && (*provenPC == *savedReturnLocation);
	if (gotcorrectret1) {
		if (VERBOSE)
			LOG << "got pc = m[r[28]]\n";
		Exp *provenSP = p->getProven(Location::regOf(28));
		gotcorrectret2 = provenSP && *provenSP == *stackPlusFour;
		if (gotcorrectret2 && VERBOSE)
			LOG << "got r[28] = r[28] + 4\n";
	}
	if (VERBOSE)
		LOG << "qualified: " << (gotcorrectret1 && gotcorrectret2) << "\n";
	return gotcorrectret1 && gotcorrectret2;
}

void
CallingConvention::Win32Signature::addReturn(Type *type, Exp *e)
{
	if (type->isVoid())
		return;
	if (!e)
		e = Location::regOf(type->isFloat() ? 32 : 24);
	Signature::addReturn(type, e);
}

void
CallingConvention::Win32Signature::addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/)
{
	if (!e)
		e = getArgumentExp(params.size());
	Signature::addParameter(type, nam, e, boundMax);
}

Exp *
CallingConvention::Win32Signature::getArgumentExp(int n)
{
	if (n < (int)params.size())
		return Signature::getArgumentExp(n);
	Exp *esp = Location::regOf(28);
	if (!params.empty() && *params[0]->getExp() == *esp)
		--n;
	return Location::memOf(new Binary(opPlus, esp, new Const((n + 1) * 4)));
}

Exp *
CallingConvention::Win32TcSignature::getArgumentExp(int n)
{
	if (n < (int)params.size())
		return Signature::getArgumentExp(n);
	Exp *esp = Location::regOf(28);
	if (!params.empty() && *params[0]->getExp() == *esp)
		--n;
	if (n == 0)
		// It's the first parameter, register ecx
		return Location::regOf(25);
	// Else, it is m[esp+4n)]
	return Location::memOf(new Binary(opPlus, esp, new Const(n * 4)));
}

Signature *
CallingConvention::Win32Signature::promote(UserProc *p)
{
	// no promotions from win32 signature up, yet.
	// a possible thing to investigate would be COM objects
	return this;
}

Exp *
CallingConvention::Win32Signature::getStackWildcard()
{
	// Note: m[esp + -8] is simplified to m[esp - 8] now
	return Location::memOf(new Binary(opMinus, Location::regOf(28), new Terminal(opWild)));
}

Exp *
CallingConvention::Win32Signature::getProven(Exp *left)
{
	int nparams = params.size();
	if (nparams > 0 && params[0]->getExp()->isRegN(28)) {
		--nparams;
	}
	if (left->isRegOfK()) {
		switch (((Const *)left->getSubExp1())->getInt()) {
		case 28:  // esp
			// Note: assumes callee pop... not true for cdecl functions!
			return new Binary(opPlus, Location::regOf(28), new Const(4 + nparams * 4));
		case 27:  // ebx
			return Location::regOf(27);
		case 29:  // ebp
			return Location::regOf(29);
		case 30:  // esi
			return Location::regOf(30);
		case 31:  // edi
			return Location::regOf(31);
		// there are other things that must be preserved here, look at calling convention
		}
	}
	return nullptr;
}

bool
CallingConvention::Win32Signature::isPreserved(Exp *e)
{
	if (e->isRegOfK()) {
		switch (((Const *)e->getSubExp1())->getInt()) {
		case 29:  // ebp
		case 27:  // ebx
		case 30:  // esi
		case 31:  // edi
		case  3:  // bx
		case  5:  // bp
		case  6:  // si
		case  7:  // di
		case 11:  // bl
		case 15:  // bh
			return true;
		default:
			return false;
		}
	}
	return false;
}

// Return a list of locations defined by library calls
void
CallingConvention::Win32Signature::setLibraryDefines(StatementList *defs)
{
	if (!defs->empty()) return;  // Do only once
	Location *r24 = Location::regOf(24);  // eax
	Type *ty = new SizeType(32);
	if (returns.size() > 1) {  // Ugh - note the stack pointer is the first return still
		ty = returns[1]->type;
#if 0  // ADHOC TA
		if (ty->isFloat()) {
			Location *r32 = Location::regOf(32);  // Top of FP stack
			r32->setType(ty);
		} else
			r24->setType(ty);  // All others return in r24 (check!)
#endif
	}
	defs->append(new ImplicitAssign(ty, r24));              // eax
	defs->append(new ImplicitAssign(Location::regOf(25)));  // ecx
	defs->append(new ImplicitAssign(Location::regOf(26)));  // edx
	defs->append(new ImplicitAssign(Location::regOf(28)));  // esp
}

Exp *
CallingConvention::Win32TcSignature::getProven(Exp *left)
{
	if (left->isRegN(28)) {
		int nparams = params.size();
		if (nparams > 0 && params[0]->getExp()->isRegN(28)) {
			--nparams;
		}
		// r28 += 4 + nparams*4 - 4     (-4 because ecx is register param)
		return new Binary(opPlus, Location::regOf(28), new Const(4 + nparams * 4 - 4));
	}
	// Else same as for standard Win32 signature
	return Win32Signature::getProven(left);
}

CallingConvention::StdC::PentiumSignature::PentiumSignature(const char *nam) :
	Signature(nam)
{
	Signature::addReturn(Location::regOf(28));
	// Signature::addImplicitParameter(new PointerType(new IntegerType()), "esp", Location::regOf(28), nullptr);
}

CallingConvention::StdC::PentiumSignature::PentiumSignature(Signature &old) :
	Signature(old)
{
}

Signature *
CallingConvention::StdC::PentiumSignature::clone() const
{
	auto n = new PentiumSignature(name.c_str());
	cloneVec(params, n->params);
	// cloneVec(implicitParams, n->implicitParams);
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	n->unknown = unknown;
	return n;
}

bool
CallingConvention::StdC::PentiumSignature::operator ==(const Signature &other) const
{
	return Signature::operator ==(other);
}

// FIXME: This needs changing. Would like to check that pc=pc and sp=sp
// (or maybe sp=sp+4) for qualifying procs. Need work to get there
bool
CallingConvention::StdC::PentiumSignature::qualified(UserProc *p, Signature &candidate)
{
	platform plat = p->getProg()->getFrontEndId();
	if (plat != PLAT_PENTIUM) return false;

	if (VERBOSE)
		LOG << "consider promotion to stdc pentium signature for " << p->getName() << "\n";

#if 1
	if (VERBOSE)
		LOG << "qualified: always true\n";
	return true;  // For now, always pass
#else
	bool gotcorrectret1 = false;
	bool gotcorrectret2 = false;
	StatementList internal;
	//p->getInternalStatements(internal);
	internal.append(*p->getCFG()->getReachExit());
	StmtListIter it;
	for (Statement *s = internal.getFirst(it); s; s = internal.getNext(it)) {
		if (auto e = dynamic_cast<Assign *>(s)) {
			if (e->getLeft()->isPC()) {
				if (e->getRight()->isMemOf() && e->getRight()->getSubExp1()->isRegOfN(28)) {
					if (VERBOSE)
						std::cerr << "got pc = m[r[28]]" << std::endl;
					gotcorrectret1 = true;
				}
			} else if (e->getLeft()->isRegN(28)) {
				if (e->getRight()->getOper() == opPlus
				 && e->getRight()->getSubExp1()->isRegOfN(28)
				 && e->getRight()->getSubExp2()->isIntConst()
				 && ((Const *)e->getRight()->getSubExp2())->getInt() == 4) {
					if (VERBOSE)
						std::cerr << "got r[28] = r[28] + 4" << std::endl;
					gotcorrectret2 = true;
				}
			}
		}
	}
	if (VERBOSE)
		LOG << "promotion: " << gotcorrectret1 && gotcorrectret2 << "\n";
	return gotcorrectret1 && gotcorrectret2;
#endif
}

void
CallingConvention::StdC::PentiumSignature::addReturn(Type *type, Exp *e)
{
	if (type->isVoid())
		return;
	if (!e)
		e = Location::regOf(type->isFloat() ? 32 : 24);
	Signature::addReturn(type, e);
}

void
CallingConvention::StdC::PentiumSignature::addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/)
{
	if (!e)
		e = getArgumentExp(params.size());
	Signature::addParameter(type, nam, e, boundMax);
}

Exp *
CallingConvention::StdC::PentiumSignature::getArgumentExp(int n)
{
	if (n < (int)params.size())
		return Signature::getArgumentExp(n);
	Exp *esp = Location::regOf(28);
	if (!params.empty() && *params[0]->getExp() == *esp)
		--n;
	return Location::memOf(new Binary(opPlus, esp, new Const((n + 1) * 4)));
}

Signature *
CallingConvention::StdC::PentiumSignature::promote(UserProc *p)
{
	// No promotions from here up, obvious idea would be c++ name mangling
	return this;
}

Exp *
CallingConvention::StdC::PentiumSignature::getStackWildcard()
{
	// Note: m[esp + -8] is simplified to m[esp - 8] now
	return Location::memOf(new Binary(opMinus, Location::regOf(28), new Terminal(opWild)));
}

Exp *
CallingConvention::StdC::PentiumSignature::getProven(Exp *left)
{
	if (left->isRegOfK()) {
		int r = ((Const *)left->getSubExp1())->getInt();
		switch (r) {
		case 28:  // esp
			return new Binary(opPlus, Location::regOf(28), new Const(4));  // esp+4
		case 29: case 30: case 31: case 27:  // ebp, esi, edi, ebx
			return Location::regOf(r);
		}
	}
	return nullptr;
}

bool
CallingConvention::StdC::PentiumSignature::isPreserved(Exp *e)
{
	if (e->isRegOfK()) {
		switch (((Const *)e->getSubExp1())->getInt()) {
		case 29:  // ebp
		case 27:  // ebx
		case 30:  // esi
		case 31:  // edi
		case  3:  // bx
		case  5:  // bp
		case  6:  // si
		case  7:  // di
		case 11:  // bl
		case 15:  // bh
			return true;
		default:
			return false;
		}
	}
	return false;
}

// Return a list of locations defined by library calls
void
CallingConvention::StdC::PentiumSignature::setLibraryDefines(StatementList *defs)
{
	if (!defs->empty()) return;  // Do only once
	Location *r24 = Location::regOf(24);  // eax
	Type *ty = new SizeType(32);
	if (returns.size() > 1) {  // Ugh - note the stack pointer is the first return still
		ty = returns[1]->type;
#if 0  // ADHOC TA
		if (ty->isFloat()) {
			Location *r32 = Location::regOf(32);  // Top of FP stack
			r32->setType(ty);
		} else
			r24->setType(ty);  // All others return in r24 (check!)
#endif
	}
	defs->append(new ImplicitAssign(ty, r24));              // eax
	defs->append(new ImplicitAssign(Location::regOf(25)));  // ecx
	defs->append(new ImplicitAssign(Location::regOf(26)));  // edx
	defs->append(new ImplicitAssign(Location::regOf(28)));  // esp
}

CallingConvention::StdC::PPCSignature::PPCSignature(const char *nam) :
	Signature(nam)
{
	Signature::addReturn(Location::regOf(1));
	// Signature::addImplicitParameter(new PointerType(new IntegerType()), "r1", Location::regOf(1), nullptr);
	// FIXME: Should also add m[r1+4] as an implicit parameter? Holds return address
}

CallingConvention::StdC::PPCSignature::PPCSignature(Signature &old) :
	Signature(old)
{
}

Signature *
CallingConvention::StdC::PPCSignature::clone() const
{
	auto n = new PPCSignature(name.c_str());
	cloneVec(params, n->params);
	// n->implicitParams = implicitParams;
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	n->unknown = unknown;
	return n;
}

Exp *
CallingConvention::StdC::PPCSignature::getArgumentExp(int n)
{
	if (n < (int)params.size())
		return Signature::getArgumentExp(n);
	if (n >= 8)
		// PPCs pass the ninth and subsequent parameters at m[%r1+8],
		// m[%r1+12], etc.
		return Location::memOf(new Binary(opPlus, Location::regOf(1), new Const(8 + (n - 8) * 4)));
	return Location::regOf((int)(3 + n));
}

void
CallingConvention::StdC::PPCSignature::addReturn(Type *type, Exp *e)
{
	if (type->isVoid())
		return;
	if (!e)
		e = Location::regOf(3);
	Signature::addReturn(type, e);
}

void
CallingConvention::StdC::PPCSignature::addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/)
{
	if (!e)
		e = getArgumentExp(params.size());
	Signature::addParameter(type, nam, e, boundMax);
}

Exp *
CallingConvention::StdC::PPCSignature::getStackWildcard()
{
	// m[r1 - WILD]
	return Location::memOf(new Binary(opMinus, Location::regOf(1), new Terminal(opWild)));
}

Exp *
CallingConvention::StdC::PPCSignature::getProven(Exp *left)
{
	if (left->isRegOfK()) {
		int r = ((Const *)((Location *)left)->getSubExp1())->getInt();
		switch (r) {
		case 1: // stack
			return left;
		}
	}
	return nullptr;
}

bool
CallingConvention::StdC::PPCSignature::isPreserved(Exp *e)
{
	if (e->isRegOfK()) {
		int r = ((Const *)e->getSubExp1())->getInt();
		return r == 1;
	}
	return false;
}

// Return a list of locations defined by library calls
void
CallingConvention::StdC::PPCSignature::setLibraryDefines(StatementList *defs)
{
	if (!defs->empty()) return;  // Do only once
	for (int r = 3; r <= 12; ++r)
		defs->append(new ImplicitAssign(Location::regOf(r)));  // Registers 3-12 are volatile (caller save)
}

/// ST20 signatures

CallingConvention::StdC::ST20Signature::ST20Signature(const char *nam) :
	Signature(nam)
{
	Signature::addReturn(Location::regOf(3));
	// Signature::addImplicitParameter(new PointerType(new IntegerType()), "sp", Location::regOf(3), nullptr);
	// FIXME: Should also add m[sp+0] as an implicit parameter? Holds return address
}

CallingConvention::StdC::ST20Signature::ST20Signature(Signature &old) :
	Signature(old)
{
}

Signature *
CallingConvention::StdC::ST20Signature::clone() const
{
	auto n = new ST20Signature(name.c_str());
	n->params = params;
	n->returns = returns;
	n->ellipsis = ellipsis;
	n->rettype = rettype;
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn;
	n->preferedParams = preferedParams;
	n->unknown = unknown;
	return n;
}

bool
CallingConvention::StdC::ST20Signature::operator ==(const Signature &other) const
{
	return Signature::operator ==(other);
}

Exp *
CallingConvention::StdC::ST20Signature::getArgumentExp(int n)
{
	if (n < (int)params.size())
		return Signature::getArgumentExp(n);
	// m[%sp+4], etc.
	Exp *sp = Location::regOf(3);
	if (!params.empty() && *params[0]->getExp() == *sp)
		--n;
	return Location::memOf(new Binary(opPlus, sp, new Const((n + 1) * 4)));
}

void
CallingConvention::StdC::ST20Signature::addReturn(Type *type, Exp *e)
{
	if (type->isVoid())
		return;
	if (!e)
		e = Location::regOf(0);
	Signature::addReturn(type, e);
}

Signature *
CallingConvention::StdC::ST20Signature::promote(UserProc *p)
{
	// No promotions from here up, obvious idea would be c++ name mangling
	return this;
}

void
CallingConvention::StdC::ST20Signature::addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/)
{
	if (!e)
		e = getArgumentExp(params.size());
	Signature::addParameter(type, nam, e, boundMax);
}

Exp *
CallingConvention::StdC::ST20Signature::getStackWildcard()
{
	// m[r1 - WILD]
	return Location::memOf(new Binary(opMinus, Location::regOf(3), new Terminal(opWild)));
}

Exp *
CallingConvention::StdC::ST20Signature::getProven(Exp *left)
{
	if (left->isRegOfK()) {
#if 1
		int r = ((Const *)left->getSubExp1())->getInt();
		switch (r) {
		case 3:
			//return new Binary(opPlus, Location::regOf(3), new Const(4));
			return left;
		case 0: case 1: case 2:
			//Registers A, B, and C are callee save
			return Location::regOf(r);
		}
#else
		int r = ((Const *)((Location *)left)->getSubExp1())->getInt();
		switch (r) {
		case 3: // stack
			return left;
		}
#endif
	}
	return nullptr;
}

bool
CallingConvention::StdC::ST20Signature::qualified(UserProc *p, Signature &candidate)
{
	platform plat = p->getProg()->getFrontEndId();
	if (plat != PLAT_ST20) return false;

	if (VERBOSE)
		LOG << "consider promotion to stdc st20 signature for " << p->getName() << "\n";

	return true;
}

#if 0
bool
CallingConvention::StdC::PPCSignature::isAddrOfStackLocal(Prog *prog, Exp *e)
{
	LOG << "doing PPC specific check on " << e << "\n";
	// special case for m[r1{-} + 4] which is used to store the return address in non-leaf procs.
	if (e->getOper() == opPlus
	 && e->getSubExp1()->isSubscript()
	 && ((RefExp *)(e->getSubExp1()))->isImplicitDef()
	 && e->getSubExp1()->getSubExp1()->isRegN(1)
	 && e->getSubExp2()->isIntConst()
	 && ((Const *)e->getSubExp2())->getInt() == 4)
		return true;
	return Signature::isAddrOfStackLocal(prog, e);
}
#endif

CallingConvention::StdC::SparcSignature::SparcSignature(const char *nam) :
	Signature(nam)
{
	Signature::addReturn(Location::regOf(14));
	// Signature::addImplicitParameter(new PointerType(new IntegerType()), "sp", Location::regOf(14), nullptr);
}

CallingConvention::StdC::SparcSignature::SparcSignature(Signature &old) :
	Signature(old)
{
}

Signature *
CallingConvention::StdC::SparcSignature::clone() const
{
	auto n = new SparcSignature(name.c_str());
	cloneVec(params, n->params);
	// cloneVec(implicitParams, n->implicitParams);
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	n->unknown = unknown;
	return n;
}

Signature *
CallingConvention::StdC::SparcLibSignature::clone() const
{
	auto n = new SparcLibSignature(name.c_str());
	cloneVec(params, n->params);
	// cloneVec(implicitParams, n->implicitParams);
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	return n;
}

bool
CallingConvention::StdC::SparcSignature::operator ==(const Signature &other) const
{
	return Signature::operator ==(other);
}

bool
CallingConvention::StdC::SparcSignature::qualified(UserProc *p, Signature &candidate)
{
	if (VERBOSE)
		LOG << "consider promotion to stdc sparc signature for " << p->getName() << "\n";

	platform plat = p->getProg()->getFrontEndId();
	if (plat != PLAT_SPARC) return false;

	if (VERBOSE)
		LOG << "Promoted to StdC::SparcSignature\n";

	return true;
}

bool
CallingConvention::StdC::PPCSignature::qualified(UserProc *p, Signature &candidate)
{
	if (VERBOSE)
		LOG << "consider promotion to stdc PPC signature for " << p->getName() << "\n";

	platform plat = p->getProg()->getFrontEndId();
	if (plat != PLAT_PPC) return false;

	if (VERBOSE)
		LOG << "Promoted to StdC::PPCSignature (always qualifies)\n";

	return true;
}

bool
CallingConvention::StdC::MIPSSignature::qualified(UserProc *p, Signature &candidate)
{
	if (VERBOSE)
		LOG << "consider promotion to stdc MIPS signature for " << p->getName() << "\n";

	platform plat = p->getProg()->getFrontEndId();
	if (plat != PLAT_MIPS) return false;

	if (VERBOSE)
		LOG << "Promoted to StdC::MIPSSignature (always qualifies)\n";

	return true;
}

void
CallingConvention::StdC::SparcSignature::addReturn(Type *type, Exp *e)
{
	if (type->isVoid())
		return;
	if (!e)
		e = Location::regOf(8);
	Signature::addReturn(type, e);
}

void
CallingConvention::StdC::SparcSignature::addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/)
{
	if (!e)
		e = getArgumentExp(params.size());
	Signature::addParameter(type, nam, e, boundMax);
}

Exp *
CallingConvention::StdC::SparcSignature::getArgumentExp(int n)
{
	if (n < (int)params.size())
		return Signature::getArgumentExp(n);
	if (n >= 6)
		// SPARCs pass the seventh and subsequent parameters at m[%sp+92],
		// m[%esp+96], etc.
		return Location::memOf(new Binary(opPlus,
		                                  Location::regOf(14), // %o6 == %sp
		                                  new Const(92 + (n - 6) * 4)));
	return Location::regOf((int)(8 + n));
}

Signature *
CallingConvention::StdC::SparcSignature::promote(UserProc *p)
{
	// no promotions from here up, obvious example would be name mangling
	return this;
}

Exp *
CallingConvention::StdC::SparcSignature::getStackWildcard()
{
	return Location::memOf(new Binary(opPlus, Location::regOf(14), new Terminal(opWild)));
}

Exp *
CallingConvention::StdC::SparcSignature::getProven(Exp *left)
{
	if (left->isRegOfK()) {
		int r = ((Const *)((Location *)left)->getSubExp1())->getInt();
		switch (r) {
		// These registers are preserved in Sparc: i0-i7 (24-31), sp (14)
		case 14:                             // sp
		case 24: case 25: case 26: case 27:  // i0-i3
		case 28: case 29: case 30: case 31:  // i4-i7
		// NOTE: Registers %g2 to %g4 are NOT preserved in ordinary application (non library) code
			return left;
		}
	}
	return nullptr;
}

bool
CallingConvention::StdC::SparcSignature::isPreserved(Exp *e)
{
	if (e->isRegOfK()) {
		int r = ((Const *)((Location *)e)->getSubExp1())->getInt();
		switch (r) {
		// These registers are preserved in Sparc: i0-i7 (24-31), sp (14)
		case 14:                             // sp
		case 24: case 25: case 26: case 27:  // i0-i3
		case 28: case 29: case 30: case 31:  // i4-i7
		// NOTE: Registers %g2 to %g4 are NOT preserved in ordinary application (non library) code
			return true;
		default:
			return false;
		}
	}
	return false;
}

// Return a list of locations defined by library calls
void
CallingConvention::StdC::SparcSignature::setLibraryDefines(StatementList *defs)
{
	if (!defs->empty()) return;  // Do only once
	for (int r = 8; r <= 15; ++r)
		defs->append(new ImplicitAssign(Location::regOf(r)));  // o0-o7 (r8-r15) modified
}

Exp *
CallingConvention::StdC::SparcLibSignature::getProven(Exp *left)
{
	if (left->isRegOfK()) {
		int r = ((Const *)((Location *)left)->getSubExp1())->getInt();
		switch (r) {
		// These registers are preserved in Sparc: i0-i7 (24-31), sp (14)
		case 14:
		case 24: case 25: case 26: case 27:
		case 28: case 29: case 30: case 31:
		// Also the "application global registers" g2-g4 (2-4) (preserved
		// by library functions, but apparently don't have to be preserved
		// by application code)
		case  2: case  3: case  4:  // g2-g4
		// The system global registers (g5-g7) are also preserved, but
		// should never be changed in an application anyway
			return left;
		}
	}
	return nullptr;
}

Signature::Signature(const char *nam) :
	rettype(new VoidType())
{
	name = nam ? nam : "<ANON>";
}

CustomSignature::CustomSignature(const char *nam) :
	Signature(nam)
{
}

void
CustomSignature::setSP(int nsp)
{
	sp = nsp;
	if (sp) {
		addReturn(Location::regOf(sp));
		// addImplicitParameter(new PointerType(new IntegerType()), "sp", Location::regOf(sp), nullptr);
	}
}

Signature *
Signature::clone() const
{
	auto n = new Signature(name.c_str());
	cloneVec(params, n->params);
	// cloneVec(implicitParams, n->implicitParams);
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	n->unknown = unknown;
	n->sigFile = sigFile;
	return n;
}

Signature *
CustomSignature::clone() const
{
	auto n = new CustomSignature(name.c_str());
	cloneVec(params, n->params);
	// cloneVec(implicitParams, n->implicitParams);
	cloneVec(returns, n->returns);
	n->ellipsis = ellipsis;
	n->rettype = rettype->clone();
	n->sp = sp;
	n->preferedName = preferedName;
	n->preferedReturn = preferedReturn ? preferedReturn->clone() : nullptr;
	n->preferedParams = preferedParams;
	return n;
}

bool
Signature::operator ==(const Signature &other) const
{
	//if (name != other.name) return false;  // MVE: should the name be significant? I'm thinking no
	if (params.size() != other.params.size()) return false;
	// Only care about the first return location (at present)
	for (auto it1 = params.begin(), it2 = other.params.begin(); it1 != params.end(); ++it1, ++it2)
		if (!(**it1 == **it2)) return false;
	if (returns.size() != other.returns.size()) return false;
	for (auto rr1 = returns.begin(), rr2 = other.returns.begin(); rr1 != returns.end(); ++rr1, ++rr2)
		if (!(**rr1 == **rr2)) return false;
	return true;
}

const std::string &
Signature::getName()
{
	return name;
}

void
Signature::setName(const std::string &nam)
{
	name = nam;
}

void
Signature::addParameter(const char *nam /*= nullptr*/)
{
	addParameter(new VoidType(), nam);
}

void
Signature::addParameter(Exp *e, Type *ty)
{
	addParameter(ty, nullptr, e);
}

void
Signature::addParameter(Type *type, const char *nam /*= nullptr*/, Exp *e /*= nullptr*/, const char *boundMax /*= ""*/)
{
	if (!e) {
		std::cerr << "No expression for parameter ";
		if (!type)
			std::cerr << "<notype>";
		else
			std::cerr << type->getCtype();
		std::cerr << " ";
		if (!nam)
			std::cerr << "<noname>";
		else
			std::cerr << nam;
		std::cerr << "\n";
		assert(e);  // Else get infinite mutual recursion with the below proc
	}

	std::string s;
	if (!nam) {
		int n = params.size();
		do {
			std::stringstream os;
			os << "param" << ++n;
			s = os.str();
		} while (findParam(s) != -1);
		nam = s.c_str();
	}
	auto p = new Parameter(type, nam, e, boundMax);
	addParameter(p);
	// addImplicitParametersFor(p);
}

void
Signature::addParameter(Parameter *param)
{
	Type *ty = param->getType();
	const char *nam = param->getName().c_str();
	Exp *e = param->getExp();

	if (strlen(nam) == 0)
		nam = nullptr;

	if (!ty || !e || !nam) {
		addParameter(ty, nam, e, param->getBoundMax().c_str());
	} else
		params.push_back(param);
}

void
Signature::removeParameter(Exp *e)
{
	int i = findParam(e);
	if (i != -1)
		removeParameter(i);
}

void
Signature::removeParameter(int i)
{
	params.erase(params.begin() + i);
}

void
Signature::setNumParams(int n)
{
	if (n < (int)params.size()) {
		params.resize(n);
	} else {
		for (int i = params.size(); i < n; ++i)
			addParameter();
	}
}

const char *
Signature::getParamName(int n)
{
	assert(n < (int)params.size());
	return params[n]->getName().c_str();
}

Exp *
Signature::getParamExp(int n)
{
	assert(n < (int)params.size());
	return params[n]->getExp();
}

Type *
Signature::getParamType(int n)
{
	//assert(n < (int)params.size() || ellipsis);
	// With recursion, parameters not set yet. Hack for now:
	if (n >= (int)params.size()) return nullptr;
	return params[n]->getType();
}

const char *
Signature::getParamBoundMax(int n)
{
	if (n >= (int)params.size()) return nullptr;
	const auto &s = params[n]->getBoundMax();
	if (s.empty())
		return nullptr;
	return s.c_str();
}

void
Signature::setParamType(int n, Type *ty)
{
	params[n]->setType(ty);
}

void
Signature::setParamType(const std::string &nam, Type *ty)
{
	int idx = findParam(nam);
	if (idx == -1) {
		LOG << "could not set type for unknown parameter " << nam << "\n";
		return;
	}
	params[idx]->setType(ty);
}

void
Signature::setParamType(Exp *e, Type *ty)
{
	int idx = findParam(e);
	if (idx == -1) {
		LOG << "could not set type for unknown parameter expression " << *e << "\n";
		return;
	}
	params[idx]->setType(ty);
}

void
Signature::setParamName(int n, const std::string &name)
{
	params[n]->setName(name);
}

void
Signature::setParamExp(int n, Exp *e)
{
	params[n]->setExp(e);
}

// Return the index for the given expression, or -1 if not found
int
Signature::findParam(Exp *e)
{
	unsigned i = 0;
	for (const auto &param : params) {
		if (*param->getExp() == *e)
			return (int)i;
		++i;
	}
	return -1;
}

void
Signature::renameParam(const std::string &oldName, const std::string &newName)
{
	for (const auto &param : params) {
		if (param->getName() == oldName) {
			param->setName(newName);
			break;
		}
	}
}

int
Signature::findParam(const std::string &nam)
{
	unsigned i = 0;
	for (const auto &param : params) {
		if (param->getName() == nam)
			return (int)i;
		++i;
	}
	return -1;
}

int
Signature::findReturn(Exp *e)
{
	unsigned i = 0;
	for (const auto &ret : returns) {
		if (*ret->exp == *e)
			return (int)i;
		++i;
	}
	return -1;
}

void
Signature::addReturn(Type *type, Exp *exp)
{
	assert(exp);
	addReturn(new Return(type, exp));
}

// Deprecated. Use the above version.
void
Signature::addReturn(Exp *exp)
{
	//addReturn(exp->getType() ? exp->getType() : new IntegerType(), exp);
	addReturn(new VoidType(), exp);
}

void
Signature::removeReturn(Exp *e)
{
	int i = findReturn(e);
	if (i != -1) {
		returns.erase(returns.begin() + i);
	}
}

void
Signature::setReturnType(int n, Type *ty)
{
	if (n < (int)returns.size())
		returns[n]->type = ty;
}

Exp *
Signature::getArgumentExp(int n)
{
	return getParamExp(n);
}

Signature *
Signature::promote(UserProc *p)
{
	// FIXME: the whole promotion idea needs a redesign...
	if (CallingConvention::Win32Signature::qualified(p, *this)) {
		Signature *sig = new CallingConvention::Win32Signature(*this);
		//sig->analyse(p);
		delete this;
		return sig;
	}

	if (CallingConvention::StdC::PentiumSignature::qualified(p, *this)) {
		Signature *sig = new CallingConvention::StdC::PentiumSignature(*this);
		//sig->analyse(p);
		delete this;
		return sig;
	}

	if (CallingConvention::StdC::SparcSignature::qualified(p, *this)) {
		Signature *sig = new CallingConvention::StdC::SparcSignature(*this);
		//sig->analyse(p);
		delete this;
		return sig;
	}

	if (CallingConvention::StdC::PPCSignature::qualified(p, *this)) {
		Signature *sig = new CallingConvention::StdC::PPCSignature(*this);
		//sig->analyse(p);
		delete this;
		return sig;
	}

	if (CallingConvention::StdC::ST20Signature::qualified(p, *this)) {
		Signature *sig = new CallingConvention::StdC::ST20Signature(*this);
		//sig->analyse(p);
		delete this;
		return sig;
	}

	return this;
}

Signature *
Signature::instantiate(platform plat, callconv cc, const char *nam)
{
	switch (plat) {
	case PLAT_PENTIUM:
		if (cc == CONV_PASCAL)
			// For now, assume the only pascal calling convention pentium signatures will be Windows
			return new CallingConvention::Win32Signature(nam);
		else if (cc == CONV_THISCALL)
			return new CallingConvention::Win32TcSignature(nam);
		else
			return new CallingConvention::StdC::PentiumSignature(nam);
	case PLAT_SPARC:
		assert(cc == CONV_C);
		return new CallingConvention::StdC::SparcSignature(nam);
	case PLAT_PPC:
		return new CallingConvention::StdC::PPCSignature(nam);
	case PLAT_ST20:
		return new CallingConvention::StdC::ST20Signature(nam);
	// insert other conventions here
	default:
		std::cerr << "unknown signature: " << conventionName(cc) << " " << platformName(plat) << "\n";
		assert(false);
	}
	return nullptr;
}

void
Signature::print(std::ostream &out, bool html) const
{
	if (isForced())
		out << "*forced* ";
	if (!returns.empty()) {
		out << "{ ";
		bool first = true;
		for (const auto &ret : returns) {
			if (first)
				first = false;
			else
				out << ", ";
			out << ret->type->getCtype() << " " << *ret->exp;
		}
		out << " } ";
	} else
		out << "void ";
	out << name << "(";
	bool first = true;
	for (const auto &param : params) {
		if (first)
			first = false;
		else
			out << ", ";
		out << param->getType()->getCtype() << " " << param->getName() << " " << *param->getExp();
	}
	out << ")\n";
}

std::string
Signature::prints() const
{
	std::ostringstream ost;
	print(ost);
	return ost.str();
}

bool
Signature::usesNewParam(UserProc *p, Statement *stmt, bool checkreach, int &n)
{
	n = getNumParams() - 1;
	if (VERBOSE) {
		std::cerr << "searching ";
		stmt->printAsUse(std::cerr);
		std::cerr << std::endl;
	}
	StatementSet reachin;
	//stmt->getReachIn(reachin, 2);
	for (int i = getNumParams(); i < 10; ++i) {
		if (stmt->usesExp(getParamExp(i))) {
			bool ok = true;
			if (checkreach) {
				bool hasDef = false;
				for (const auto &ri : reachin) {
					auto as = dynamic_cast<Assignment *>(ri);
					if (as && *as->getLeft() == *getParamExp(i)) {
						hasDef = true;
						break;
					}
				}
				if (hasDef) ok = false;
			}
			if (ok) {
				n = i;
			}
		}
	}
	return n > ((int)getNumParams() - 1);
}

// Not very satisfying to do things this way. Problem is that the polymorphic CallingConvention objects are set up
// very late in the decompilation. Get the set of registers that are not saved in library functions (or any
// procedures that follow the calling convention)
void
Signature::setABIdefines(Prog *prog, StatementList *defs)
{
	if (!defs->empty()) return;  // Do only once
	MACHINE mach = prog->getMachine();
	switch (mach) {
	case MACHINE_PENTIUM:
		defs->append(new ImplicitAssign(Location::regOf(24)));  // eax
		defs->append(new ImplicitAssign(Location::regOf(25)));  // ecx
		defs->append(new ImplicitAssign(Location::regOf(26)));  // edx
		break;
	case MACHINE_SPARC:
		for (int r = 8; r <= 13; ++r)
			defs->append(new ImplicitAssign(Location::regOf(r)));  // %o0-o5
		defs->append(new ImplicitAssign(Location::regOf(1)));  // %g1
		break;
	case MACHINE_PPC:
		for (int r = 3; r <= 12; ++r)
			defs->append(new ImplicitAssign(Location::regOf(r)));  // r3-r12
		break;
	case MACHINE_ST20:
		defs->append(new ImplicitAssign(Location::regOf(0)));  // A
		defs->append(new ImplicitAssign(Location::regOf(1)));  // B
		defs->append(new ImplicitAssign(Location::regOf(2)));  // C
		break;
	default:
		break;
	}
}

int
Signature::getStackRegister() throw (StackRegisterNotDefinedException)
{
	if (VERBOSE)
		LOG << "thowing StackRegisterNotDefinedException\n";
	throw StackRegisterNotDefinedException();
}

// Needed before the signature is promoted
int
Signature::getStackRegister(Prog *prog) throw (StackRegisterNotDefinedException)
{
	MACHINE mach = prog->getMachine();
	switch (mach) {
	case MACHINE_SPARC:
		return 14;
	case MACHINE_PENTIUM:
		return 28;
	case MACHINE_PPC:
		return 1;
	case MACHINE_ST20:
		return 3;
	default:
		throw StackRegisterNotDefinedException();
	}
}

bool
Signature::isStackLocal(Prog *prog, Exp *e)
{
	// e must be m[...]
	if (auto re = dynamic_cast<RefExp *>(e))
		return isStackLocal(prog, re->getSubExp1());
	if (!e->isMemOf()) return false;
	Exp *addr = ((Location *)e)->getSubExp1();
	return isAddrOfStackLocal(prog, addr);
}

bool
Signature::isAddrOfStackLocal(Prog *prog, Exp *e)
{
	OPER op = e->getOper();
	if (op == opAddrOf)
		return isStackLocal(prog, e->getSubExp1());
	// e must be sp -/+ K or just sp
	static Exp *sp = Location::regOf(getStackRegister(prog));
	if (op != opMinus && op != opPlus) {
		// Matches if e is sp or sp{0} or sp{-}
		return (*e == *sp || (e->isSubscript() && ((RefExp *)e)->isImplicitDef() && *((RefExp *)e)->getSubExp1() == *sp));
	}
	if (op == opMinus && !isLocalOffsetNegative()) return false;
	if (op == opPlus  && !isLocalOffsetPositive()) return false;
	Exp *sub1 = ((Binary *)e)->getSubExp1();
	Exp *sub2 = ((Binary *)e)->getSubExp2();
	// e must be <sub1> +- K
	if (!sub2->isIntConst()) return false;
	// first operand must be sp or sp{0} or sp{-}
	if (auto re = dynamic_cast<RefExp *>(sub1)) {
		if (!re->isImplicitDef()) return false;
		sub1 = re->getSubExp1();
	}
	return *sub1 == *sp;
}

// An override for the SPARC: [sp+0] .. [sp+88] are local variables (effectively), but [sp + >=92] are memory parameters
bool
CallingConvention::StdC::SparcSignature::isAddrOfStackLocal(Prog *prog, Exp *e)
{
	OPER op = e->getOper();
	if (op == opAddrOf)
		return isStackLocal(prog, e->getSubExp1());
	// e must be sp -/+ K or just sp
	static Exp *sp = Location::regOf(14);
	if (op != opMinus && op != opPlus) {
		// Matches if e is sp or sp{0} or sp{-}
		return (*e == *sp || (e->isSubscript() && ((RefExp *)e)->isImplicitDef() && *((RefExp *)e)->getSubExp1() == *sp));
	}
	Exp *sub1 = ((Binary *)e)->getSubExp1();
	Exp *sub2 = ((Binary *)e)->getSubExp2();
	// e must be <sub1> +- K
	if (!sub2->isIntConst()) return false;
	// first operand must be sp or sp{0} or sp{-}
	if (auto re = dynamic_cast<RefExp *>(sub1)) {
		if (!re->isImplicitDef()) return false;
		sub1 = re->getSubExp1();
	}
	if (!(*sub1 == *sp)) return false;
	// SPARC specific test: K must be < 92; else it is a parameter
	int K = ((Const *)sub2)->getInt();
	return K < 92;
}

bool
Parameter::operator ==(const Parameter &other) const
{
	if (!(*type == *other.type)) return false;
	// Do we really care about a parameter's name?
	if (!(name == other.name)) return false;
	if (!(*exp == *other.exp)) return false;
	return true;
}

#if 0
bool
CallingConvention::StdC::HppaSignature::isLocalOffsetPositive()
{
	return true;
}
#endif

#ifdef USING_MEMO
class SignatureMemo : public Memo {
public:
	SignatureMemo(int m) : Memo(m) { }

	std::string name;  // name of procedure
	std::vector<Parameter *> params;
	// std::vector<ImplicitParameter *> implicitParams;
	std::vector<Return *> returns;
	Type *rettype;
	bool ellipsis;
	Type *preferedReturn;
	std::string preferedName;
	std::vector<int> preferedParams;
};

Memo *
Signature::makeMemo(int mId)
{
	auto m = new SignatureMemo(mId);
	m->name = name;
	m->params = params;
	// m->implicitParams = implicitParams;
	m->returns = returns;
	m->rettype = rettype;
	m->ellipsis = ellipsis;
	m->preferedReturn = preferedReturn;
	m->preferedName = preferedName;
	m->preferedParams = preferedParams;

	for (const auto &param : params)
		param->takeMemo(mId);
#if 0
	for (const auto &param : implicitParams)
		param->takeMemo(mId);
#endif
	for (const auto &ret : returns)
		ret->takeMemo(mId);
	if (rettype)
		rettype->takeMemo(mId);
	if (preferedReturn)
		preferedReturn->takeMemo(mId);
	return m;
}

void
Signature::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<SignatureMemo *>(mm);

	name = m->name;
	params = m->params;
	// implicitParams = m->implicitParams;
	returns = m->returns;
	rettype = m->rettype;
	ellipsis = m->ellipsis;
	preferedReturn = m->preferedReturn;
	preferedName = m->preferedName;
	preferedParams = m->preferedParams;

	for (const auto &param : params)
		param->restoreMemo(m->mId, dec);
#if 0
	for (const auto &param : implicitParams)
		param->restoreMemo(m->mId, dec);
#endif
	for (const auto &ret : returns)
		ret->restoreMemo(m->mId, dec);
	if (rettype)
		rettype->restoreMemo(m->mId, dec);
	if (preferedReturn)
		preferedReturn->restoreMemo(m->mId, dec);
}

class ParameterMemo : public Memo {
public:
	ParameterMemo(int m) : Memo(m) { }

	Type *type;
	std::string name;
	Exp *exp;
};

Memo *
Parameter::makeMemo(int mId)
{
	auto m = new ParameterMemo(mId);

	m->type = type;
	m->name = name;
	m->exp = exp;

	type->takeMemo(mId);
	exp->takeMemo(mId);

	return m;
}

void
Parameter::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<ParameterMemo *>(mm);
	type = m->type;
	name = m->name;
	exp = m->exp;

	type->restoreMemo(m->mId, dec);
	exp->restoreMemo(m->mId, dec);
}

class ImplicitParameterMemo : public ParameterMemo {
public:
	ImplicitParameterMemo(int m) : ParameterMemo(m) { }

	Parameter *parent;
};

Memo *
ImplicitParameter::makeMemo(int mId)
{
	auto m = new ImplicitParameterMemo(mId);

	m->type = getType();
	m->name = getName();
	m->exp = getExp();
	m->parent = parent;

	m->type->takeMemo(mId);
	m->exp->takeMemo(mId);

	return m;
}

void
ImplicitParameter::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<ImplicitParameterMemo *>(mm);
	setType(m->type);
	setName(m->name);
	setExp(m->exp);
	parent = m->parent;

	m->type->restoreMemo(m->mId, dec);
	m->exp->restoreMemo(m->mId, dec);
}
#endif

bool
Signature::isOpCompatStackLocal(OPER op)
{
	if (op == opMinus) return isLocalOffsetNegative();
	if (op == opPlus) return isLocalOffsetPositive();
	return false;
}

bool
Signature::returnCompare(const Assignment &a, const Assignment &b)
{
	return *a.getLeft() < *b.getLeft();  // Default: sort by expression only, no explicit ordering
}

bool
Signature::argumentCompare(const Assignment &a, const Assignment &b)
{
	return *a.getLeft() < *b.getLeft();  // Default: sort by expression only, no explicit ordering
}

bool
CallingConvention::StdC::PentiumSignature::returnCompare(const Assignment &a, const Assignment &b)
{
	Exp *la = a.getLeft();
	Exp *lb = b.getLeft();
	// Eax is the preferred return location
	if (la->isRegN(24)) return true;   // r24 is less than anything
	if (lb->isRegN(24)) return false;  // Nothing is less than r24

	// Next best is r30 (floating point %st)
	if (la->isRegN(30)) return true;   // r30 is less than anything that's left
	if (lb->isRegN(30)) return false;  // Nothing left is less than r30

	// Else don't care about the order
	return *la < *lb;
}

static Location spPlus64(opMemOf, new Binary(opPlus, Location::regOf(14), new Const(64)), nullptr);
bool
CallingConvention::StdC::SparcSignature::returnCompare(const Assignment &a, const Assignment &b)
{
	Exp *la = a.getLeft();
	Exp *lb = b.getLeft();
	// %o0 (r8) is the preferred return location
	if (la->isRegN(8)) return true;   // r24 is less than anything
	if (lb->isRegN(8)) return false;  // Nothing is less than r24

	// Next best is %f0 (r32)
	if (la->isRegN(32)) return true;   // r32 is less than anything that's left
	if (lb->isRegN(32)) return false;  // Nothing left is less than r32

	// Next best is %f0-1 (r64)
	if (la->isRegN(64)) return true;   // r64 is less than anything that's left
	if (lb->isRegN(64)) return false;  // Nothing left is less than r64

	// Next best is m[esp{-}+64]
	if (*la == spPlus64) return true;   // m[esp{-}+64] is less than anything that's left
	if (*lb == spPlus64) return false;  // Nothing left is less than m[esp{-}+64]

	// Else don't care about the order
	return *la < *lb;
}

// From m[sp +- K] return K (or -K for subtract). sp could be subscripted with {-}
// Helper function for the below
int
stackOffset(Exp *e, int sp)
{
	int ret = 0;
	if (e->isMemOf()) {
		Exp *sub = ((Location *)e)->getSubExp1();
		OPER op = sub->getOper();
		if (op == opPlus || op == opMinus) {
			Exp *op1 = ((Binary *)sub)->getSubExp1();
			if (auto re = dynamic_cast<RefExp *>(op1))
				op1 = re->getSubExp1();
			if (op1->isRegN(sp)) {
				Exp *op2 = ((Binary *)sub)->getSubExp2();
				if (op2->isIntConst())
					ret = ((Const *)op2)->getInt();
				if (op == opMinus)
					ret = -ret;
			}
		}
	}
	return ret;
}

bool
CallingConvention::StdC::PentiumSignature::argumentCompare(const Assignment &a, const Assignment &b)
{
	Exp *la = a.getLeft();
	Exp *lb = b.getLeft();
	int ma = stackOffset(la, 28);
	int mb = stackOffset(lb, 28);

	if (ma && mb)
		return ma < mb;
	if (ma && !mb)
		return true;   // m[sp-K] is less than anything else
	if (mb && !ma)
		return false;  // Nothing else is less than m[sp-K]

	// Else don't care about the order
	return *la < *lb;
}

bool
CallingConvention::StdC::SparcSignature::argumentCompare(const Assignment &a, const Assignment &b)
{
	Exp *la = a.getLeft();
	Exp *lb = b.getLeft();
	// %o0-$o5 (r8-r13) are the preferred argument locations
	int ra = 0, rb = 0;
	if (la->isRegOf()) {
		int r = ((Const *)((Location *)la)->getSubExp1())->getInt();
		if (r >= 8 && r <= 13)
			ra = r;
	}
	if (lb->isRegOf()) {
		int r = ((Const *)((Location *)lb)->getSubExp1())->getInt();
		if (r >= 8 && r <= 13)
			rb = r;
	}
	if (ra && rb)
		return ra < rb;  // Both r8-r13: compare within this set
	if (ra && rb == 0)
		return true;     // r8-r13 less than anything else
	if (rb && ra == 0)
		return false;    // Nothing else is less than r8-r13

	int ma = stackOffset(la, 30);
	int mb = stackOffset(lb, 30);

	if (ma && mb)
		return ma < mb;  // Both m[sp + K]: order by memory offset
	if (ma && !mb)
		return true;     // m[sp+K] less than anything left
	if (mb && !ma)
		return false;    // nothing left is less than m[sp+K]

	return *la < *lb;  // Else order arbitrarily
}

// Class Return methods
Return *
Return::clone() const
{
	return new Return(type->clone(), exp->clone());
}

bool
Return::operator ==(const Return &other) const
{
	if (!(*type == *other.type)) return false;
	if (!(*exp == *other.exp)) return false;
	return true;
}

#ifdef USING_MEMO
class ReturnMemo : public Memo {
public:
	ReturnMemo(int m) : Memo(m) { }

	Type *type;
	Exp *exp;
};

Memo *
Return::makeMemo(int mId)
{
	auto m = new ReturnMemo(mId);

	m->type = type;
	m->exp = exp;

	type->takeMemo(mId);
	exp->takeMemo(mId);

	return m;
}

void
Return::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<ReturnMemo *>(mm);
	type = m->type;
	exp = m->exp;

	type->restoreMemo(m->mId, dec);
	exp->restoreMemo(m->mId, dec);
}
#endif

Type *
Signature::getTypeFor(Exp *e)
{
	for (const auto &ret : returns)
		if (*ret->exp == *e)
			return ret->type;
	return nullptr;
}
