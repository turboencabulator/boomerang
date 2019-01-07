/**
 * \file
 * \brief Implementation of class Type functions related to solving type
 *        analysis in an iterative, data-flow-based manner.
 *
 * \authors
 * Copyright (C) 2004-2006, Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "boomerang.h"
#include "exp.h"
#include "log.h"
#include "proc.h"
#include "prog.h"
#include "signature.h"
#include "type.h"

#ifdef GARBAGE_COLLECTOR
#include <gc/gc.h>
#endif

#include <cstring>

static int nextUnionNumber = 0;

#define DFA_ITER_LIMIT 20

// m[idx*K1 + K2]; leave idx wild
static Exp *scaledArrayPat = Location::memOf(new Binary(opPlus,
                                                        new Binary(opMult,
                                                                   new Terminal(opWild),
                                                                   new Terminal(opWildIntConst)),
                                                        new Terminal(opWildIntConst)));
// idx + K; leave idx wild
static Exp *unscaledArrayPat = new Binary(opPlus,
                                          new Terminal(opWild),
                                          new Terminal(opWildIntConst));

#ifdef GARBAGE_COLLECTOR
/**
 * The purpose of this function and others like it is to establish safe static
 * roots for garbage collection purposes.  This is particularly important for
 * OS X where it is known that the collector can't see global variables, but
 * it is suspected that this is actually important for other architectures as
 * well.
 */
void
init_dfa()
{
	static Exp **gc_pointers = (Exp **)GC_MALLOC_UNCOLLECTABLE(2 * sizeof *gc_pointers);
	gc_pointers[0] = scaledArrayPat;
	gc_pointers[1] = unscaledArrayPat;
}
#endif


static int progress = 0;
void
UserProc::dfaTypeAnalysis()
{
	Boomerang::get()->alert_decompile_debug_point(this, "before dfa type analysis");

	// First use the type information from the signature. Sometimes needed to split variables (e.g. argc as a
	// int and char* in sparc/switch_gcc)
	bool ch = signature->dfaTypeAnalysis(cfg);
	StatementList stmts;
	getStatements(stmts);
	int iter;
	for (iter = 1; iter <= DFA_ITER_LIMIT; ++iter) {
		ch = false;
		for (const auto &stmt : stmts) {
			if (++progress >= 2000) {
				progress = 0;
				std::cerr << "t" << std::flush;
			}
			bool thisCh = false;
			stmt->dfaTypeAnalysis(thisCh);
			if (thisCh) {
				ch = true;
				if (DEBUG_TA)
					LOG << " caused change: " << *stmt << "\n";
			}
		}
		if (!ch)
			// No more changes: round robin algorithm terminates
			break;
	}
	if (ch)
		LOG << "### WARNING: iteration limit exceeded for dfaTypeAnalysis of procedure " << getName() << " ###\n";

	if (DEBUG_TA) {
		LOG << "\n ### results for data flow based type analysis for " << getName() << " ###\n";
		LOG << iter << " iterations\n";
		for (const auto &stmt : stmts) {
			LOG << *stmt << "\n";  // Print the statement; has dest type
			// Now print type for each constant in this Statement
			std::list<Const *> lc;
			stmt->findConstants(lc);
			if (!lc.empty()) {
				LOG << "       ";
				for (const auto &con : lc)
					LOG << con->getType()->getCtype() << " " << *con << "  ";
				LOG << "\n";
			}
			// If stmt is a call, also display its return types
			if (auto call = dynamic_cast<CallStatement *>(stmt)) {
				ReturnStatement *rs = call->getCalleeReturn();
				if (!rs) continue;
				UseCollector *uc = call->getUseCollector();
				bool first = true;
				for (const auto &ret : *rs) {
					// Intersect the callee's returns with the live locations at the call, i.e. make sure that they
					// exist in *uc
					Exp *lhs = ((Assignment *)ret)->getLeft();
					if (!uc->exists(lhs))
						continue;  // Intersection fails
					if (first)
						LOG << "       returns: ";
					else
						LOG << ", ";
					LOG << ((Assignment *)ret)->getType()->getCtype() << " " << *((Assignment *)ret)->getLeft();
				}
				LOG << "\n";
			}
		}
		LOG << "\n ### end results for Data flow based Type Analysis for " << getName() << " ###\n\n";
	}

	// Now use the type information gathered
#if 0
	Boomerang::get()->alert_decompile_debug_point(this, "before mapping locals from dfa type analysis");
	if (DEBUG_TA)
		LOG << " ### mapping expressions to local variables for " << getName() << " ###\n";
	for (const auto &stmt : stmts) {
		stmt->dfaMapLocals();
	}
	if (DEBUG_TA)
		LOG << " ### end mapping expressions to local variables for " << getName() << " ###\n";
#endif

	Boomerang::get()->alert_decompile_debug_point(this, "before other uses of dfa type analysis");

	Prog *prog = getProg();
	for (const auto &stmt : stmts) {
		// 1) constants
		std::list<Const *> lc;
		stmt->findConstants(lc);
		for (const auto &con : lc) {
			Type *t = con->getType();
			int val = con->getInt();
			if (t && t->resolvesToPointer()) {
				PointerType *pt = t->asPointer();
				Type *baseType = pt->getPointsTo();
				if (baseType->resolvesToChar()) {
					// Convert to a string  MVE: check for read-only?
					// Also, distinguish between pointer to one char, and ptr to many?
					if (auto str = prog->getStringConstant(val, true)) {
						// Make a string
						con->setStr(str);
					}
				} else if (baseType->resolvesToInteger()
				        || baseType->resolvesToFloat()
				        || baseType->resolvesToSize()) {
					ADDRESS addr = (ADDRESS) con->getInt();
					prog->globalUsed(addr, baseType);
					if (auto gloName = prog->getGlobalName(addr)) {
						auto gname = std::string(gloName);
						ADDRESS r = addr - prog->getGlobalAddr(gname);
						Exp *ne;
						if (r) {
							Location *g = Location::global(gname, this);
							ne = Location::memOf(new Binary(opPlus,
							                                new Unary(opAddrOf, g),
							                                new Const(r)), this);
						} else {
							Type *ty = prog->getGlobalType(gname);
							if (auto as = dynamic_cast<Assign *>(stmt)) {
								if (auto ast = as->getType()) {
									int bits = ast->getSize();
									if (!ty || ty->getSize() == 0)
										prog->setGlobalType(gname, new IntegerType(bits));
								}
							}
							Location *g = Location::global(gname, this);
							if (ty && ty->resolvesToArray())
								ne = new Binary(opArrayIndex, g, new Const(0));
							else
								ne = g;
						}
						Exp *memof = Location::memOf(con);
						stmt->searchAndReplace(memof->clone(), ne);
					}
				} else if (baseType->resolvesToArray()) {
					// We have found a constant in stmt which has type pointer to array of alpha. We can't get the parent
					// of con, but we can find it with the pattern unscaledArrayPat.
					std::list<Exp *> result;
					stmt->searchAll(unscaledArrayPat, result);
					for (const auto &res : result) {
						// idx + K
						Const *constK = (Const *)((Binary *)res)->getSubExp2();
						// Note: keep searching till we find the pattern with this constant, since other constants may
						// not be used as pointer to array type.
						if (constK != con) continue;
						ADDRESS K = (ADDRESS)constK->getInt();
						Exp *idx = ((Binary *)res)->getSubExp1();
						Exp *arr = new Unary(opAddrOf,
						                     new Binary(opArrayIndex,
						                                Location::global(prog->getGlobalName(K), this),
						                                idx));
						// Beware of changing expressions in implicit assignments... map can become invalid
						auto ia = dynamic_cast<ImplicitAssign *>(stmt);
						if (ia)
							cfg->removeImplicitAssign(ia->getLeft());
						stmt->searchAndReplace(unscaledArrayPat, arr);
						// stmt will likely have an m[a[array]], so simplify
						stmt->simplifyAddr();
						if (ia)
							// Replace the implicit assignment entry. Note that stmt's lhs has changed
							cfg->findImplicitAssign(ia->getLeft());
						// Ensure that the global is declared
						// Ugh... I think that arrays and pointers to arrays are muddled!
						prog->globalUsed(K, baseType);
					}
				}
			} else if (t->resolvesToFloat()) {
				if (con->isIntConst()) {
					// Reinterpret as a float (and convert to double)
					//con->setFlt(reinterpret_cast<float>(con->getInt()));
					int tmp = con->getInt();
					con->setFlt(*(float *)&tmp); // Reinterpret to float, then cast to double
					con->setType(new FloatType(64));
				}
				// MVE: more work if double?
			} else /* if (t->resolvesToArray()) */ {
				prog->globalUsed(val, t);
			}
		}

		// 2) Search for the scaled array pattern and replace it with an array use m[idx*K1 + K2]
		std::list<Exp *> result;
		stmt->searchAll(scaledArrayPat, result);
		for (const auto &res : result) {
			//Type* ty = stmt->getTypeFor(res);
			// FIXME: should check that we use with array type...
			// Find idx and K2
			Exp *t = ((Unary *)res)->getSubExp1();      // idx*K1 + K2
			Exp *l = ((Binary *)t)->getSubExp1();       // idx*K1
			Exp *r = ((Binary *)t)->getSubExp2();       // K2
			ADDRESS K2 = (ADDRESS)((Const *)r)->getInt();
			Exp *idx = ((Binary *)l)->getSubExp1();
			// Replace with the array expression
			auto nam = prog->newGlobalName(K2);
			Exp *arr = new Binary(opArrayIndex,
			                      Location::global(nam, this),
			                      idx);
			if (stmt->searchAndReplace(scaledArrayPat, arr)) {
				if (auto ia = dynamic_cast<ImplicitAssign *>(stmt))
					// Register an array of appropriate type
					prog->globalUsed(K2, new ArrayType(ia->getType()));
			}
		}

		// 3) Check implicit assigns for parameter and global types.
		if (auto ia = dynamic_cast<ImplicitAssign *>(stmt)) {
			Exp *lhs = ia->getLeft();
			Type *iType = ia->getType();
			// Note: parameters are not explicit any more
			//if (lhs->isParam()) { // }
			bool allZero;
			Exp *slhs = lhs->clone()->removeSubscripts(allZero);
			int i = signature->findParam(slhs);
			if (i != -1)
				setParamType(i, iType);
			else if (lhs->isMemOf()) {
				Exp *sub = ((Location *)lhs)->getSubExp1();
				if (sub->isIntConst()) {
					// We have a m[K] := -
					int K = ((Const *)sub)->getInt();
					prog->globalUsed(K, iType);
				}
			} else if (lhs->isGlobal()) {
				const char *gname = ((Const *)((Location *)lhs)->getSubExp1())->getStr();
				prog->setGlobalType(gname, iType);
			}
		}

		// 4) Add the locals (soon globals as well) to the localTable, to sort out the overlaps
		if (auto ts = dynamic_cast<TypingStatement *>(stmt)) {
			Exp *addrExp = nullptr;
			Type *typeExp = nullptr;
			if (auto as = dynamic_cast<Assignment *>(ts)) {
				auto lhs = as->getLeft();
				if (lhs->isMemOf()) {
					addrExp = ((Location *)lhs)->getSubExp1();
					typeExp = as->getType();
				}
			} else if (auto irs = dynamic_cast<ImpRefStatement *>(ts)) {
				// Assume an implicit reference
				addrExp = irs->getAddressExp();
				if (addrExp->isTypedExp() && ((TypedExp *)addrExp)->getType()->resolvesToPointer())
					addrExp = ((TypedExp *)addrExp)->getSubExp1();
				typeExp = irs->getType();
				// typeExp should be a pointer expression, or a union of pointer types
				if (typeExp->resolvesToUnion())
					typeExp = typeExp->asUnion()->dereferenceUnion();
				else {
					assert(typeExp->resolvesToPointer());
					typeExp = typeExp->asPointer()->getPointsTo();
				}
			}
			if (addrExp && signature->isAddrOfStackLocal(prog, addrExp)) {
				int addr = 0;
				if (addrExp->getArity() == 2 && signature->isOpCompatStackLocal(addrExp->getOper())) {
					Const *K = (Const *)((Binary *)addrExp)->getSubExp2();
					if (K->isConst()) {
						addr = K->getInt();
						if (addrExp->getOper() == opMinus)
							addr = -addr;
					}
				}
				LOG << "in proc " << getName() << " adding addrExp " << *addrExp << " to local table\n";
				Type *ty = ts->getType();
				localTable.addItem(addr, lookupSym(Location::memOf(addrExp), ty), typeExp);
			}
		}
	}


	if (VERBOSE) {
		LOG << "### after application of dfa type analysis for " << getName() << " ###\n";
		printToLog();
		LOG << "### end application of dfa type analysis for " << getName() << " ###\n";
	}

	Boomerang::get()->alert_decompile_debug_point(this, "after dfa type analysis");
}

// This is the core of the data-flow-based type analysis algorithm: implementing the meet operator.
// In classic lattice-based terms, the TOP type is void; there is no BOTTOM type since we handle overconstraints with
// unions.
// Consider various pieces of knowledge about the types. There could be:
// a) void: no information. Void meet x = x.
// b) size only: find a size large enough to contain the two types.
// c) broad type only, e.g. floating point
// d) signedness, no size
// e) size, no signedness
// f) broad type, size, and (for integer broad type), signedness

// ch set true if any change

Type *
VoidType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	// void meet x = x
	ch |= !other->resolvesToVoid();
	return other->clone();
}

Type *
FuncType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (*this == *other)
		return this;  // NOTE: at present, compares names as well as types and num parameters
	return createUnion(other, ch, bHighestPtr);
}

Type *
IntegerType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToInteger()) {
		auto otherInt = other->asInteger();
		// Signedness
		int oldSignedness = signedness;
		if (otherInt->signedness > 0)
			++signedness;
		else if (otherInt->signedness < 0)
			--signedness;
		ch |= (signedness > 0) != (oldSignedness > 0);  // Changed from signed to not necessarily signed
		ch |= (signedness < 0) != (oldSignedness < 0);  // Changed from unsigned to not necessarily unsigned
		// Size. Assume 0 indicates unknown size
		if (size < otherInt->size) {
			size = otherInt->size;
			ch = true;
		}
		return this;
	}
	if (other->resolvesToSize()) {
		unsigned otherSize = other->getSize();
		if (size == 0) {  // Doubt this will ever happen
			size = otherSize;  // FIXME:  Need to set ch = true here?
			return this;
		}
		if (size == otherSize) return this;
		LOG << "integer size " << size << " meet with SizeType size " << otherSize << "!\n";
		if (size < otherSize) {
			size = otherSize;
			ch = true;
		}
		return this;
	}
	return createUnion(other, ch, bHighestPtr);
}

Type *
FloatType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToFloat()) {
		auto otherFlt = other->asFloat();
		if (size < otherFlt->size) {
			size = otherFlt->size;
			ch = true;
		}
		return this;
	}
	if (other->resolvesToSize()) {
		unsigned otherSize = other->getSize();
		if (size < otherSize) {
			size = otherSize;
			ch = true;
		}
		return this;
	}
	return createUnion(other, ch, bHighestPtr);
}

Type *
BooleanType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToBoolean())
		return this;
	return createUnion(other, ch, bHighestPtr);
}

Type *
CharType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToChar())
		return this;
	// Also allow char to merge with integer
	if (other->resolvesToInteger()) {
		ch = true;
		return other->clone();
	}
	if (other->resolvesToSize() && other->getSize() == getSize())
		return this;
	return createUnion(other, ch, bHighestPtr);
}

Type *
PointerType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToSize() && other->getSize() == getSize())
		return this;
	if (other->resolvesToPointer()) {
		auto otherPtr = other->asPointer();
		if (pointsToAlpha() && !otherPtr->pointsToAlpha()) {
			setPointsTo(otherPtr->getPointsTo());
			ch = true;
		} else {
			// We have a meeting of two pointers.
			Type *thisBase = points_to;
			Type *otherBase = otherPtr->points_to;
			if (bHighestPtr) {
				// We want the greatest type of thisBase and otherBase
				if (thisBase->isSubTypeOrEqual(otherBase))
					return other->clone();
				if (otherBase->isSubTypeOrEqual(thisBase))
					return this;
				// There may be another type that is a superset of this and other; for now return void*
				return new PointerType(new VoidType);
			}
			// See if the base types will meet
			if (otherBase->resolvesToPointer()) {
				if (thisBase->resolvesToPointer() && thisBase->asPointer()->getPointsTo() == thisBase)
					std::cerr << "HACK! BAD POINTER 1\n";
				if (otherBase->resolvesToPointer() && otherBase->asPointer()->getPointsTo() == otherBase)
					std::cerr << "HACK! BAD POINTER 2\n";
				if (thisBase == otherBase)  // Note: compare pointers
					return this;  // Crude attempt to prevent stack overflow
				if (*thisBase == *otherBase)
					return this;
				if (pointerDepth() == otherPtr->pointerDepth()) {
					Type *fType = getFinalPointsTo();
					if (fType->resolvesToVoid()) return other->clone();
					Type *ofType = otherPtr->getFinalPointsTo();
					if (ofType->resolvesToVoid()) return this;
					if (*fType == *ofType) return this;
				}
			}
			if (thisBase->isCompatibleWith(otherBase)) {
				points_to = points_to->meetWith(otherBase, ch, bHighestPtr);
				return this;
			}
			// The bases did not meet successfully. Union the pointers.
			return createUnion(other, ch, bHighestPtr);
		}
		return this;
	}
	// Would be good to understand class hierarchys, so we know if a* is the same as b* when b is a subclass of a
	return createUnion(other, ch, bHighestPtr);
}

Type *
ArrayType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToArray()) {
		auto otherArr = other->asArray();
		Type *newBase = base_type->clone()->meetWith(otherArr->base_type, ch, bHighestPtr);
		if (*newBase != *base_type) {
			ch = true;
			// base_type = newBase;  // No: call setBaseType to adjust length
			setBaseType(newBase);
		}
		if (otherArr->getLength() < getLength()) {
			this->setLength(otherArr->getLength());
		}
		return this;
	}
	if (*base_type == *other)
		return this;
	// Needs work?
	return createUnion(other, ch, bHighestPtr);
}

Type *
NamedType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (auto rt = resolvesTo()) {
		Type *ret = rt->meetWith(other, ch, bHighestPtr);
		if (ret == rt)
			return this;  // Retain the named type, much better than some compound type
		return ret;  // Otherwise, whatever the result is
	}
	if (other->resolvesToVoid())
		return this;
	if (*this == *other)
		return this;
	return createUnion(other, ch, bHighestPtr);
}

Type *
CompoundType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (!other->resolvesToCompound()) {
		if (elems[0].type->isCompatibleWith(other))
			// struct meet first element = struct
			return this;
		return createUnion(other, ch, bHighestPtr);
	}
	auto otherCmp = other->asCompound();
	if (otherCmp->isSuperStructOf(*this)) {
		// The other structure has a superset of my struct's offsets. Preserve the names etc of the bigger struct.
		ch = true;
		return other;
	}
	if (isSubStructOf(*otherCmp)) {
		// This is a superstruct of other
		ch = true;
		return this;
	}
	if (*this == *other)
		return this;
	// Not compatible structs. Create a union of both complete structs.
	// NOTE: may be possible to take advantage of some overlaps of the two structures some day.
	return createUnion(other, ch, bHighestPtr);
}

#define PRINT_UNION 0  // Set to 1 to debug unions to stderr
#ifdef PRINT_UNION
unsigned unionCount = 0;
#endif

Type *
UnionType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToUnion()) {
		if (this == other)  // Note: pointer comparison
			return this;  // Avoid infinite recursion
		ch = true;
		auto otherUnion = other->asUnion();
		// Always return this, never other, (even if other is larger than this) because otherwise iterators can become
		// invalid below
		for (const auto &elem : otherUnion->elems) {
			meetWith(elem.type, ch, bHighestPtr);
			return this;
		}
	}

	// Other is a non union type
	if (other->resolvesToPointer() && other->asPointer()->getPointsTo() == this) {
		LOG << "WARNING! attempt to union " << getCtype() << " with pointer to self!\n";
		return this;
	}
	for (auto &elem : elems) {
		Type *curr = elem.type->clone();
		if (curr->isCompatibleWith(other)) {
			elem.type = curr->meetWith(other, ch, bHighestPtr);
			return this;
		}
	}

	// Other is not compatible with any of my component types. Add a new type
	char name[20];
#if PRINT_UNION  // Set above
	if (unionCount == 999)  // Adjust the count to catch the one you want
		std::cerr << "createUnion breakpoint\n";  // Note: you need two breakpoints (also in Type::createUnion)
	std::cerr << "  " << ++unionCount << " Created union from " << getCtype() << " and " << other->getCtype();
#endif
	sprintf(name, "x%d", ++nextUnionNumber);
	addType(other->clone(), name);
#if PRINT_UNION
	std::cerr << ", result is " << getCtype() << "\n";
#endif
	ch = true;
	return this;
}

Type *
SizeType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToSize()) {
		unsigned otherSize = other->getSize();
		if (otherSize != size) {
			LOG << "size " << size << " meet with size " << otherSize << "!\n";
			if (size < otherSize) {
				size = otherSize;
				ch = true;
			}
		}
		return this;
	}
	ch = true;
	if (other->resolvesToInteger()
	 || other->resolvesToFloat()
	 || other->resolvesToPointer()) {
		unsigned otherSize = other->getSize();
		if (otherSize == 0) {
			other->setSize(size);  // FIXME:  Won't work if other->isNamed().  Stuff below probably makes similar assumptions.
			return other->clone();
		}
		if (otherSize == size)
			return other->clone();
		LOG << "WARNING: size " << size << " meet with " << other->getCtype() << "; allowing temporarily\n";
		return other->clone();
	}
	return createUnion(other, ch, bHighestPtr);
}

Type *
UpperType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToUpper()) {
		auto otherUpp = other->asUpper();
		Type *newBase = base_type->clone()->meetWith(otherUpp->base_type, ch, bHighestPtr);
		if (*newBase != *base_type) {
			ch = true;
			base_type = newBase;
		}
		return this;
	}
	// Needs work?
	return createUnion(other, ch, bHighestPtr);
}

Type *
LowerType::meetWith(Type *other, bool &ch, bool bHighestPtr)
{
	if (other->resolvesToVoid())
		return this;
	if (other->resolvesToUpper()) {
		auto otherLow = other->asLower();
		Type *newBase = base_type->clone()->meetWith(otherLow->base_type, ch, bHighestPtr);
		if (*newBase != *base_type) {
			ch = true;
			base_type = newBase;
		}
		return this;
	}
	// Needs work?
	return createUnion(other, ch, bHighestPtr);
}

Type *
Statement::meetWithFor(Type *ty, Exp *e, bool &ch)
{
	bool thisCh = false;
	Type *newType = getTypeFor(e)->meetWith(ty, thisCh);
	if (thisCh) {
		ch = true;
		setTypeFor(e, newType->clone());
	}
	return newType;
}

Type *
Type::createUnion(Type *other, bool &ch, bool bHighestPtr /* = false */)
{

	assert(!resolvesToUnion());  // `this' should not be a UnionType
	if (other->resolvesToUnion())
		return other->meetWith(this, ch, bHighestPtr)->clone();  // Put all the hard union logic in one place
	// Check for anytype meet compound with anytype as first element
	if (other->resolvesToCompound()) {
		auto otherComp = other->asCompound();
		Type *firstType = otherComp->getType((unsigned)0);
		if (firstType->isCompatibleWith(this))
			// struct meet first element = struct
			return other->clone();
	}
	// Check for anytype meet array of anytype
	if (other->resolvesToArray()) {
		auto otherArr = other->asArray();
		Type *elemTy = otherArr->getBaseType();
		if (elemTy->isCompatibleWith(this))
			// array meet element = array
			return other->clone();
	}

	char name[20];
#if PRINT_UNION
	if (unionCount == 999)  // Adjust the count to catch the one you want
		std::cerr << "createUnion breakpoint\n";  // Note: you need two breakpoints (also in UnionType::meetWith)
#endif
	sprintf(name, "x%d", ++nextUnionNumber);
	auto u = new UnionType;
	u->addType(this->clone(), name);
	sprintf(name, "x%d", ++nextUnionNumber);
	u->addType(other->clone(), name);
	ch = true;
#if PRINT_UNION
	std::cerr << "  " << ++unionCount << " Created union from "
	          << getCtype() << " and " << other->getCtype()
	          << ", result is " << u->getCtype() << "\n";
#endif
	return u;
}

void
CallStatement::dfaTypeAnalysis(bool &ch)
{
	// Iterate through the arguments
	int n = 0;
	for (auto aa = arguments.begin(); aa != arguments.end(); ++aa, ++n) {
		if (procDest
		 && procDest->getSignature()->getParamBoundMax(n)
		 && ((Assign *)*aa)->getRight()->isIntConst()) {
			Assign *a = (Assign *)*aa;
			std::string boundmax = procDest->getSignature()->getParamBoundMax(n);
			assert(a->getType()->resolvesToInteger());
			int nt = 0;
			for (auto aat = arguments.begin(); aat != arguments.end(); ++aat, ++nt) {
				if (boundmax == procDest->getSignature()->getParamName(nt)) {
					Type *tyt = ((Assign *)*aat)->getType();
					if (tyt->resolvesToPointer()
					 && tyt->asPointer()->getPointsTo()->resolvesToArray()
					 && tyt->asPointer()->getPointsTo()->asArray()->isUnbounded())
						tyt->asPointer()->getPointsTo()->asArray()->setLength(((Const *)a->getRight())->getInt());
					break;
				}
			}
		}
		// The below will ascend type, meet type with that of arg, and descend type. Note that the type of the assign
		// will already be that of the signature, if this is a library call, from updateArguments()
		((Assign *)*aa)->dfaTypeAnalysis(ch);
	}
	// The destination is a pointer to a function with this function's signature (if any)
	if (pDest) {
		if (signature)
			pDest->descendType(new FuncType(signature), ch, this);
		else if (procDest)
			pDest->descendType(new FuncType(procDest->getSignature()), ch, this);
	}
}

void
ReturnStatement::dfaTypeAnalysis(bool &ch)
{
	for (const auto &mod : modifieds) {
		((Assign *)mod)->dfaTypeAnalysis(ch);
	}
	for (const auto &ret : returns) {
		((Assign *)ret)->dfaTypeAnalysis(ch);
	}
}

// For x0 := phi(x1, x2, ...) want
// Tx0 := Tx0 meet (Tx1 meet Tx2 meet ...)
// Tx1 := Tx1 meet Tx0
// Tx2 := Tx2 meet Tx0
// ...
void
PhiAssign::dfaTypeAnalysis(bool &ch)
{
	auto it = defVec.begin();
	while (!it->e && it != defVec.end())
		++it;
	assert(it != defVec.end());
	Type *meetOfArgs = it->def->getTypeFor(lhs);
	for (++it; it != defVec.end(); ++it) {
		if (!it->e) continue;
		assert(it->def);
		Type *typeOfDef = it->def->getTypeFor(it->e);
		meetOfArgs = meetOfArgs->meetWith(typeOfDef, ch);
	}
	type = type->meetWith(meetOfArgs, ch);
	for (const auto &def : defVec) {
		if (!def.e) continue;
		def.def->meetWithFor(type, def.e, ch);
	}
	Assignment::dfaTypeAnalysis(ch);  // Handle the LHS
}

void
Assign::dfaTypeAnalysis(bool &ch)
{
	Type *tr = rhs->ascendType();
	type = type->meetWith(tr, ch, true);    // Note: bHighestPtr is set true, since the lhs could have a greater type
	                                        // (more possibilities) than the rhs. Example: pEmployee = pManager.
	rhs->descendType(type, ch, this);       // This will effect rhs = rhs MEET lhs
	Assignment::dfaTypeAnalysis(ch);        // Handle the LHS wrt m[] operands
}

void
Assignment::dfaTypeAnalysis(bool &ch)
{
	Signature *sig = proc->getSignature();
	// Don't do this for the common case of an ordinary local, since it generates hundreds of implicit references,
	// without any new type information
	if (lhs->isMemOf() && !sig->isStackLocal(proc->getProg(), lhs)) {
		Exp *addr = ((Unary *)lhs)->getSubExp1();
		// Meet the assignment type with *(type of the address)
		Type *addrType = addr->ascendType();
		Type *memofType;
		if (addrType->resolvesToPointer())
			memofType = addrType->asPointer()->getPointsTo();
		else
			memofType = new VoidType;
		type = type->meetWith(memofType, ch);
		// Push down the fact that the memof operand is a pointer to the assignment type
		addrType = new PointerType(type);
		addr->descendType(addrType, ch, this);
	}
}

void
BranchStatement::dfaTypeAnalysis(bool &ch)
{
	if (pCond)
		pCond->descendType(new BooleanType(), ch, this);
	// Not fully implemented yet?
}

void
ImplicitAssign::dfaTypeAnalysis(bool &ch)
{
	Assignment::dfaTypeAnalysis(ch);
}

void
BoolAssign::dfaTypeAnalysis(bool &ch)
{
	// Not properly implemented yet
	Assignment::dfaTypeAnalysis(ch);
}

// Special operators for handling addition and subtraction in a data flow based type analysis
//                  ta=
//  tb=     alpha*  int     pi
// beta*    bottom  void*   void*
// int      void*   int     pi
// pi       void*   pi      pi
Type *
sigmaSum(Type *ta, Type *tb)
{
	bool ch;
	if (ta->resolvesToPointer()) {
		if (tb->resolvesToPointer())
			return ta->createUnion(tb, ch);
		return new PointerType(new VoidType);
	}
	if (ta->resolvesToInteger()) {
		if (tb->resolvesToPointer())
			return new PointerType(new VoidType);
		return tb->clone();
	}
	if (tb->resolvesToPointer())
		return new PointerType(new VoidType);
	return ta->clone();
}


//                  tc=
//  to=     beta*   int     pi
// alpha*   int     bottom  int
// int      void*   int     pi
// pi       pi      pi      pi
Type *
sigmaAddend(Type *tc, Type *to)
{
	bool ch;
	if (tc->resolvesToPointer()) {
		if (to->resolvesToPointer())
			return new IntegerType;
		if (to->resolvesToInteger())
			return new PointerType(new VoidType);
		return to->clone();
	}
	if (tc->resolvesToInteger()) {
		if (to->resolvesToPointer())
			return tc->createUnion(to, ch);
		return to->clone();
	}
	if (to->resolvesToPointer())
		return new IntegerType;
	return tc->clone();
}

//                  tc=
//  tb=     beta*   int     pi
// alpha*   bottom  void*   void*
// int      void*   int     pi
// pi       void*   int     pi
Type *
deltaMinuend(Type *tc, Type *tb)
{
	bool ch;
	if (tc->resolvesToPointer()) {
		if (tb->resolvesToPointer())
			return tc->createUnion(tb, ch);
		return new PointerType(new VoidType);
	}
	if (tc->resolvesToInteger()) {
		if (tb->resolvesToPointer())
			return new PointerType(new VoidType);
		return tc->clone();
	}
	if (tb->resolvesToPointer())
		return new PointerType(new VoidType);
	return tc->clone();
}

//                  tc=
//  ta=     beta*   int     pi
// alpha*   int     void*   pi
// int      bottom  int     int
// pi       int     pi      pi
Type *
deltaSubtrahend(Type *tc, Type *ta)
{
	bool ch;
	if (tc->resolvesToPointer()) {
		if (ta->resolvesToPointer())
			return new IntegerType;
		if (ta->resolvesToInteger())
			return tc->createUnion(ta, ch);
		return new IntegerType;
	}
	if (tc->resolvesToInteger())
		if (ta->resolvesToPointer())
			return new PointerType(new VoidType);
	return ta->clone();
	if (ta->resolvesToPointer())
		return tc->clone();
	return ta->clone();
}

//                  ta=
//  tb=     alpha*  int     pi
// beta*    int     bottom  int
// int      void*   int     pi
// pi       pi      int     pi
Type *
deltaDifference(Type *ta, Type *tb)
{
	bool ch;
	if (ta->resolvesToPointer()) {
		if (tb->resolvesToPointer())
			return new IntegerType;
		if (tb->resolvesToInteger())
			return new PointerType(new VoidType);
		return tb->clone();
	}
	if (ta->resolvesToInteger()) {
		if (tb->resolvesToPointer())
			return ta->createUnion(tb, ch);
		return new IntegerType;
	}
	if (tb->resolvesToPointer())
		return new IntegerType;
	return ta->clone();
}

//  //  //  //  //  //  //  //  //  //  //
//                                      //
//  ascendType: draw type information   //
//      up the expression tree          //
//                                      //
//  //  //  //  //  //  //  //  //  //  //

Type *
Binary::ascendType()
{
	if (op == opFlagCall) return new VoidType;
	Type *ta = subExp1->ascendType();
	Type *tb = subExp2->ascendType();
	switch (op) {
	case opPlus:
		return sigmaSum(ta, tb);
	// Do I need to check here for Array* promotion? I think checking in descendType is enough
	case opMinus:
		return deltaDifference(ta, tb);
	case opMult:
	case opDiv:
		return new IntegerType(ta->getSize(), -1);
	case opMults:
	case opDivs:
	case opShiftRA:
		return new IntegerType(ta->getSize(), +1);
	case opBitAnd:
	case opBitOr:
	case opBitXor:
	case opShiftR:
	case opShiftL:
		return new IntegerType(ta->getSize(), 0);
	case opLess:
	case opGtr:
	case opLessEq:
	case opGtrEq:
	case opLessUns:
	case opGtrUns:
	case opLessEqUns:
	case opGtrEqUns:
		return new BooleanType();
	default:
		// Many more cases to implement
		return new VoidType;
	}
}

// Constants and subscripted locations are at the leaves of the expression tree. Just return their stored types.
Type *
RefExp::ascendType()
{
	if (!def) {
		std::cerr << "Warning! Null reference in " << *this << "\n";
		return new VoidType;
	}
	return def->getTypeFor(subExp1);
}

Type *
Const::ascendType()
{
	if (type->resolvesToVoid()) {
		switch (op) {
		case opIntConst:
// could be anything, Boolean, Character, we could be bit fiddling pointers for all we know - trentw
#if 0
			if (u.i != 0 && (u.i < 0x1000 && u.i > -0x100))
				// Assume that small nonzero integer constants are of integer type (can't be pointers)
				// But note that you can't say anything about sign; these are bit patterns, not HLL constants
				// (e.g. all ones could be signed -1 or unsigned 0xFFFFFFFF)
				type = new IntegerType(STD_SIZE, 0);
#endif
			break;
		case opLongConst:
			type = new IntegerType(STD_SIZE * 2, 0);
			break;
		case opFltConst:
			type = new FloatType(64);
			break;
		case opStrConst:
			type = new PointerType(new CharType);
			break;
		case opFuncConst:
			type = new FuncType;  // More needed here?
			break;
		default:
			assert(0);  // Bad Const
		}
	}
	return type;
}

// Can also find various terminals at the leaves of an expression tree
Type *
Terminal::ascendType()
{
	switch (op) {
	case opPC:
		return new IntegerType(STD_SIZE, -1);
	case opCF:
	case opZF:
		return new BooleanType;
	case opDefineAll:
		return new VoidType;
	case opFlags:
		return new IntegerType(STD_SIZE, -1);
	default:
		std::cerr << "ascendType() for terminal " << *this << " not implemented!\n";
		return new VoidType;
	}
}

Type *
Unary::ascendType()
{
	Type *ta = subExp1->ascendType();
	switch (op) {
	case opMemOf:
		if (ta->resolvesToPointer())
			return ta->asPointer()->getPointsTo();
		else
			return new VoidType();  // NOT SURE! Really should be bottom
	case opAddrOf:
		return new PointerType(ta);
	}
	return new VoidType;
}

Type *
Ternary::ascendType()
{
	switch (op) {
	case opFsize:
		return new FloatType(((Const *)subExp2)->getInt());
	case opZfill:
	case opSgnEx:
		{
			int toSize = ((Const *)subExp2)->getInt();
			return Type::newIntegerLikeType(toSize, op == opZfill ? -1 : 1);
		}
	}
	return new VoidType;
}

Type *
TypedExp::ascendType()
{
	return type;
}


//  //  //  //  //  //  //  //  //  //  //
//                                      //
//  descendType: push type information  //
//      down the expression tree        //
//                                      //
//  //  //  //  //  //  //  //  //  //  //

void
Binary::descendType(Type *parentType, bool &ch, Statement *s)
{
	if (op == opFlagCall) return;
	Type *ta = subExp1->ascendType();
	Type *tb = subExp2->ascendType();
	Type *nt;  // "New" type for certain operators
	// The following is an idea of Mike's that is not yet implemented well. It is designed to handle the situation
	// where the only reference to a local is where its address is taken. In the current implementation, it incorrectly
	// triggers with every ordinary local reference, causing esp to appear used in the final program
#if 0
	Signature *sig = s->getProc()->getSignature();
	Prog *prog = s->getProc()->getProg();
	if (parentType->resolvesToPointer()
	 && !parentType->asPointer()->getPointsTo()->resolvesToVoid()
	 && sig->isAddrOfStackLocal(prog, this)) {
		// This is the address of some local. What I used to do is to make an implicit assignment for the local, and
		// try to meet with the real assignment later. But this had some problems. Now, make an implicit *reference*
		// to the specified address; this should eventually meet with the main assignment(s).
		s->getProc()->setImplicitRef(s, this, parentType);
	}
#endif
	switch (op) {
	case opPlus:
		ta = ta->meetWith(sigmaAddend(parentType, tb), ch);
		subExp1->descendType(ta, ch, s);
		tb = tb->meetWith(sigmaAddend(parentType, ta), ch);
		subExp2->descendType(tb, ch, s);
		break;
	case opMinus:
		ta = ta->meetWith(deltaMinuend(parentType, tb), ch);
		subExp1->descendType(ta, ch, s);
		tb = tb->meetWith(deltaSubtrahend(parentType, ta), ch);
		subExp2->descendType(tb, ch, s);
		break;
	case opGtrUns:
	case opLessUns:
	case opGtrEqUns:
	case opLessEqUns:
		{
			nt = new IntegerType(ta->getSize(), -1);  // Used as unsigned
			ta = ta->meetWith(nt, ch);
			tb = tb->meetWith(nt, ch);
			subExp1->descendType(ta, ch, s);
			subExp2->descendType(tb, ch, s);
		}
		break;
	case opGtr:
	case opLess:
	case opGtrEq:
	case opLessEq:
		{
			nt = new IntegerType(ta->getSize(), +1);  // Used as signed
			ta = ta->meetWith(nt, ch);
			tb = tb->meetWith(nt, ch);
			subExp1->descendType(ta, ch, s);
			subExp2->descendType(tb, ch, s);
		}
		break;
	case opBitAnd:
	case opBitOr:
	case opBitXor:
	case opShiftR:
	case opShiftL:
	case opMults:
	case opDivs:
	case opShiftRA:
	case opMult:
	case opDiv:
		{
			int signedness;
			switch (op) {
			case opBitAnd:
			case opBitOr:
			case opBitXor:
			case opShiftR:
			case opShiftL:
				signedness = 0;
				break;
			case opMults:
			case opDivs:
			case opShiftRA:
				signedness = 1;
				break;
			case opMult:
			case opDiv:
				signedness = -1;
				break;
			default:
				signedness = 0;
				break;  // Unknown signedness
			}

			int parentSize = parentType->getSize();
			ta = ta->meetWith(new IntegerType(parentSize, signedness), ch);
			subExp1->descendType(ta, ch, s);
			if (op == opShiftL || op == opShiftR || op == opShiftRA)
				// These operators are not symmetric; doesn't force a signedness on the second operand
				// FIXME: should there be a gentle bias twowards unsigned? Generally, you can't shift by negative
				// amounts.
				signedness = 0;
			tb = tb->meetWith(new IntegerType(parentSize, signedness), ch);
			subExp2->descendType(tb, ch, s);
		}
		break;
	default:
		// Many more cases to implement
		break;
	}
}

void
RefExp::descendType(Type *parentType, bool &ch, Statement *s)
{
	Type *newType = def->meetWithFor(parentType, subExp1, ch);
	// In case subExp1 is a m[...]
	subExp1->descendType(newType, ch, s);
}

void
Const::descendType(Type *parentType, bool &ch, Statement *s)
{
	bool thisCh = false;
	type = type->meetWith(parentType, thisCh);
	ch |= thisCh;
	if (thisCh) {
		// May need to change the representation
		if (type->resolvesToFloat()) {
			if (op == opIntConst) {
				op = opFltConst;
				type = new FloatType(64);
				float f = *(float *)&u.i;
				u.d = (double)f;
			} else if (op == opLongConst) {
				op = opFltConst;
				type = new FloatType(64);
				double d = *(double *)&u.ll;
				u.d = d;
			}
		}
		// May be other cases
	}
}

void
Unary::descendType(Type *parentType, bool &ch, Statement *s)
{
	switch (op) {
	case opMemOf:
		// Check for m[x*K1 + K2]: array with base K2 and stride K1
		if (subExp1->getOper() == opPlus
		 && ((Binary *)subExp1)->getSubExp1()->getOper() == opMult
		 && ((Binary *)subExp1)->getSubExp2()->isIntConst()
		 && ((Binary *)((Binary *)subExp1)->getSubExp1())->getSubExp2()->isIntConst()) {
			Exp *leftOfPlus = ((Binary *)subExp1)->getSubExp1();
			// We would expect the stride to be the same size as the base type
			unsigned stride = ((Const *)((Binary *)leftOfPlus)->getSubExp2())->getInt();
			if (DEBUG_TA && stride * 8 != parentType->getSize())
				LOG << "type WARNING: apparent array reference at " << *this
				    << " has stride " << stride * 8 << " bits, but parent type " << parentType->getCtype()
				    << " has size " << parentType->getSize() << "\n";
			// The index is integer type
			Exp *x = ((Binary *)leftOfPlus)->getSubExp1();
			x->descendType(new IntegerType(parentType->getSize(), 0), ch, s);
			// K2 is of type <array of parentType>
			Const *constK2 = (Const *)((Binary *)subExp1)->getSubExp2();
			ADDRESS intK2 = (ADDRESS)constK2->getInt();
			Prog *prog = s->getProc()->getProg();
			constK2->descendType(prog->makeArrayType(intK2, parentType), ch, s);
		} else if (subExp1->getOper() == opPlus
		      && ((Binary *)subExp1)->getSubExp1()->isSubscript()
		      && ((RefExp *)((Binary *)subExp1)->getSubExp1())->isLocation()
		      && ((Binary *)subExp1)->getSubExp2()->isIntConst()) {
			// m[l1 + K]
			Location *l1 = (Location *)((RefExp *)((Binary *)subExp1)->getSubExp1());  // FIXME: This casting looks suspicious
			Type *l1Type = l1->ascendType();
			int K = ((Const *)((Binary *)subExp1)->getSubExp2())->getInt();
			if (l1Type->resolvesToPointer()) {
				// This is a struct reference m[ptr + K]; ptr points to the struct and K is an offset into it
				// First find out if we already have struct information
				if (l1Type->asPointer()->resolvesToCompound()) {
					CompoundType *ct = l1Type->asPointer()->asCompound();
					if (ct->isGeneric())
						ct->updateGenericMember(K, parentType, ch);
					else
						// would like to force a simplify here; I guess it will happen soon enough
						;
				} else {
					// Need to create a generic stuct with a least one member at offset K
					auto ct = new CompoundType(true);
					ct->updateGenericMember(K, parentType, ch);
				}
			} else {
				// K must be the pointer, so this is a global array
				// FIXME: finish this case
			}
			// FIXME: many other cases
		} else
			subExp1->descendType(new PointerType(parentType), ch, s);
		break;
	case opAddrOf:
		if (parentType->resolvesToPointer())
			subExp1->descendType(parentType->asPointer()->getPointsTo(), ch, s);
		break;
	case opGlobal:
		{
			Prog *prog = s->getProc()->getProg();
			const char *name = ((Const *)subExp1)->getStr();
			Type *ty = prog->getGlobalType(name);
			ty = ty->meetWith(parentType, ch);
			prog->setGlobalType(name, ty);
		}
		break;
	}
}

void
Ternary::descendType(Type *parentType, bool &ch, Statement *s)
{
	switch (op) {
	case opFsize:
		subExp3->descendType(new FloatType(((Const *)subExp1)->getInt()), ch, s);
		break;
	case opZfill:
	case opSgnEx:
		{
			int fromSize = ((Const *)subExp1)->getInt();
			Type *fromType;
			fromType = Type::newIntegerLikeType(fromSize, op == opZfill ? -1 : 1);
			subExp3->descendType(fromType, ch, s);
		}
		break;
	}
}

void
TypedExp::descendType(Type *parentType, bool &ch, Statement *s)
{
}

void
Terminal::descendType(Type *parentType, bool &ch, Statement *s)
{
}

bool
Signature::dfaTypeAnalysis(Cfg *cfg)
{
	bool ch = false;
	for (const auto &param : params) {
		// Parameters should be defined in an implicit assignment
		if (auto def = cfg->findImplicitParamAssign(param)) {  // But sometimes they are not used, and hence have no implicit definition
			bool thisCh = false;
			def->meetWithFor(param->getType(), param->getExp(), thisCh);
			if (thisCh) {
				ch = true;
				if (DEBUG_TA)
					LOG << "  sig caused change: " << param->getType()->getCtype() << " " << param->getName() << "\n";
			}
		}
	}
	return ch;
}


// Note: to prevent infinite recursion, CompoundType, ArrayType, and UnionType implement this function as a delegation
// to isCompatible()
bool
Type::isCompatibleWith(Type *other, bool all /* = false */)
{
	if (other->resolvesToCompound()
	 || other->resolvesToArray()
	 || other->resolvesToUnion())
		return other->isCompatible(this, all);
	return isCompatible(other, all);
}

bool
VoidType::isCompatible(Type *other, bool all)
{
	return true;  // Void is compatible with any type
}

bool
SizeType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToFunc()) return false;
	// FIXME: why is there a test for size 0 here?
	unsigned otherSize = other->getSize();
	if (otherSize == size || otherSize == 0) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (other->resolvesToArray()) return isCompatibleWith(other->asArray()->getBaseType());
	//return false;
	// For now, size32 and double will be considered compatible (helps test/pentium/global2)
	return true;
}

bool
IntegerType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToInteger()) return true;
	if (other->resolvesToChar()) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (other->resolvesToSize() && other->getSize() == getSize()) return true;
	return false;
}

bool
FloatType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToFloat()) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (other->resolvesToArray()) return isCompatibleWith(other->asArray()->getBaseType());
	if (other->resolvesToSize() && other->getSize() == getSize()) return true;
	return false;
}

bool
CharType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToChar()) return true;
	if (other->resolvesToInteger()) return true;
	if (other->resolvesToSize() && other->getSize() == getSize()) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (other->resolvesToArray()) return isCompatibleWith(other->asArray()->getBaseType());
	return false;
}

bool
BooleanType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToBoolean()) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (other->resolvesToSize() && other->getSize() == getSize()) return true;
	return false;
}

bool
FuncType::isCompatible(Type *other, bool all)
{
	assert(signature);
	if (other->resolvesToVoid()) return true;
	if (*this == *other) return true;  // MVE: should not compare names!
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (other->resolvesToSize() && other->getSize() == STD_SIZE) return true;  // FIXME:  FuncType::getSize() != STD_SIZE, is this right?
	if (other->resolvesToFunc()) {
		assert(other->asFunc()->signature);
		if (*other->asFunc()->signature == *signature) return true;
	}
	return false;
}

bool
PointerType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (other->resolvesToSize() && other->getSize() == getSize()) return true;
	if (!other->resolvesToPointer()) return false;
	return points_to->isCompatibleWith(other->asPointer()->points_to);
}

bool
NamedType::isCompatible(Type *other, bool all)
{
	if (other->isNamed() && name == ((NamedType *)other)->getName())
		return true;
	if (auto rt = resolvesTo())
		return rt->isCompatibleWith(other);
	if (other->resolvesToVoid()) return true;
	return (*this == *other);
}

bool
ArrayType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToArray() && base_type->isCompatibleWith(other->asArray()->base_type)) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (!all && base_type->isCompatibleWith(other)) return true;  // An array of x is compatible with x
	return false;
}

bool
UnionType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToUnion()) {
		if (this == other)  // Note: pointer comparison
			return true;  // Avoid infinite recursion
		auto o = other->asUnion();
		// Unions are compatible if one is a subset of the other
		if (elems.size() < o->elems.size()) {
			for (const auto &elem : elems)
				if (!o->isCompatible(elem.type, all))
					return false;
		} else {
			for (const auto &elem : o->elems)
				if (!isCompatible(elem.type, all))
					return false;
		}
		return true;
	}
	// Other is not a UnionType
	for (const auto &elem : elems)
		if (other->isCompatibleWith(elem.type), all)
			return true;
	return false;
}

bool
CompoundType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	if (!other->resolvesToCompound())
		// Used to always return false here. But in fact, a struct is compatible with its first member (if all is false)
		return !all && elems[0].type->isCompatibleWith(other);
	auto o = other->asCompound();
	if (elems.size() != o->elems.size()) return false;  // Is a subcompound compatible with a supercompound?
	for (auto it1 = elems.cbegin(), it2 = o->elems.cbegin(); it1 != elems.cend(); ++it1, ++it2)
		if (!it1->type->isCompatibleWith(it2->type))
			return false;
	return true;
}

bool
UpperType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToUpper() && base_type->isCompatibleWith(other->asUpper()->base_type)) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	return false;
}

bool
LowerType::isCompatible(Type *other, bool all)
{
	if (other->resolvesToVoid()) return true;
	if (other->resolvesToLower() && base_type->isCompatibleWith(other->asLower()->base_type)) return true;
	if (other->resolvesToUnion()) return other->isCompatibleWith(this);
	return false;
}

bool
Type::isSubTypeOrEqual(Type *other)
{
	if (resolvesToVoid()) return true;
	if (*this == *other) return true;
	if (this->resolvesToCompound() && other->resolvesToCompound())
		return this->asCompound()->isSubStructOf(*other);
	// Not really sure here
	return false;
}

Type *
Type::dereference()
{
	if (resolvesToPointer())
		return asPointer()->getPointsTo();
	if (resolvesToUnion())
		return asUnion()->dereferenceUnion();
	return new VoidType();  // Can't dereference this type. Note: should probably be bottom
}

// Dereference this union. If it is a union of pointers, return a union of the dereferenced items. Else return VoidType
// (note: should probably be bottom)
Type *
UnionType::dereferenceUnion() const
{
	auto ret = new UnionType;
	char name[20];
	for (const auto &elem : elems) {
		Type *ty = elem.type->dereference();
		if (ty->resolvesToVoid())
			return ty;  // Return void for the whole thing
		sprintf(name, "x%d", ++nextUnionNumber);
		ret->addType(ty->clone(), name);
	}
	return ret;
}
