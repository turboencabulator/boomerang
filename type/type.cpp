/**
 * \file
 * \brief Implementation of the Type class:  Low-level type information.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
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

#include "type.h"

#include "boomerang.h"
#include "cfg.h"
#include "exp.h"
#include "proc.h"
#include "signature.h"
#include "types.h"

#include <sstream>

#include <cstring>
#include <cassert>

FuncType::FuncType(Signature *sig) : signature(sig) { }

IntegerType::IntegerType(unsigned sz, int sign) : size(sz), signedness(sign) { }

FloatType::FloatType(unsigned sz) : size(sz) { }

PointerType::PointerType(Type *p)
{
	setPointsTo(p);
}

ArrayType::ArrayType(Type *p, unsigned length) : base_type(p), length(length) { }
ArrayType::ArrayType(Type *p) : ArrayType(p, NO_BOUND) { }

NamedType::NamedType(const std::string &name) : name(name) { }

CompoundType::CompoundType(bool generic) : generic(generic) { }

SizeType::SizeType(unsigned sz) : size(sz) { }

#if 0 // Cruft?
UpperType::UpperType(Type *base) : base_type(base) { }
LowerType::LowerType(Type *base) : base_type(base) { }
#endif


PointerType::~PointerType()
{
	// delete points_to;  // Easier for test code (which doesn't use garbage collection)
}
ArrayType::~ArrayType()
{
	// delete base_type;
}

/**
 * \fn Type *Type::clone()
 * \brief Make a deep copy of this Type.
 *
 * \returns  Copy of the Type.
 */
Type *
IntegerType::clone() const
{
	return new IntegerType(size, signedness);
}
Type *
FloatType::clone() const
{
	return new FloatType(size);
}
Type *
BooleanType::clone() const
{
	return new BooleanType();
}
Type *
CharType::clone() const
{
	return new CharType();
}
Type *
VoidType::clone() const
{
	return new VoidType();
}
Type *
FuncType::clone() const
{
	return new FuncType(signature);
}
Type *
PointerType::clone() const
{
	return new PointerType(points_to->clone());
}
Type *
ArrayType::clone() const
{
	return new ArrayType(base_type->clone(), length);
}
Type *
NamedType::clone() const
{
	return new NamedType(name);
}
Type *
CompoundType::clone() const
{
	auto t = new CompoundType();
	for (const auto &elem : elems)
		t->addType(elem.type->clone(), elem.name);
	return t;
}
Type *
UnionType::clone() const
{
	auto u = new UnionType();
	for (const auto &elem : elems)
		u->addType(elem.type, elem.name);
	return u;
}
Type *
SizeType::clone() const
{
	return new SizeType(size);
}
#if 0 // Cruft?
Type *
UpperType::clone() const
{
	return new UpperType(base_type->clone());
}
Type *
LowerType::clone() const
{
	return new LowerType(base_type->clone());
}
#endif

/**
 * \returns false if some info is missing, e.g. unknown sign, size, or basic
 * type.
 */
bool
Type::isComplete() const
{
	return true;
}
bool
IntegerType::isComplete() const
{
	return signedness != 0 && size != 0;
}
bool
SizeType::isComplete() const
{
	return false;  // Basic type is unknown
}
#if 0 // Cruft?
bool
UpperType::isComplete() const
{
	return base_type->isComplete();
}
bool
LowerType::isComplete() const
{
	return base_type->isComplete();
}
#endif

/**
 * \brief Get the size of this Type.
 *
 * \returns  Size of the Type (in bytes).
 */
unsigned
Type::getBytes() const
{
	return (getSize() + 7) / 8;
}

/**
 * \fn unsigned Type::getSize() const
 * \brief Get the size of this Type.
 *
 * \returns  Size of the Type (in bits).
 */
unsigned
IntegerType::getSize() const
{
	return size;
}
unsigned
FloatType::getSize() const
{
	return size;
}
unsigned
BooleanType::getSize() const
{
	return 1;
}
unsigned
CharType::getSize() const
{
	return 8;
}
unsigned
VoidType::getSize() const
{
	return 0;
}
unsigned
FuncType::getSize() const
{
	return 0; /* always nagged me */
}
unsigned
PointerType::getSize() const
{
	//points_to->getSize(); // yes, it was a good idea at the time
	return STD_SIZE;
}
unsigned
ArrayType::getSize() const
{
	return base_type->getSize() * length;
}
unsigned
NamedType::getSize() const
{
	if (auto ty = resolvesTo())
		return ty->getSize();
	if (VERBOSE)
		LOG << "WARNING: Unknown size for named type " << name << "\n";
	return 0; // don't know
}
unsigned
CompoundType::getSize() const
{
	unsigned sz = 0;
	for (const auto &elem : elems)
		// NOTE: this assumes no padding... perhaps explicit padding will be needed
		sz += elem.type->getSize();
	return sz;
}
unsigned
UnionType::getSize() const
{
	unsigned max = 0;
	for (const auto &elem : elems) {
		unsigned sz = elem.type->getSize();
		if (sz > max) max = sz;
	}
	return max;
}
unsigned
SizeType::getSize() const
{
	return size;
}
#if 0 // Cruft?
unsigned
UpperType::getSize() const
{
	return base_type->getSize() / 2;
}
unsigned
LowerType::getSize() const
{
	return base_type->getSize() / 2;
}
#endif

/**
 * \brief Set the size of this Type.
 *
 * \param[in] sz  Size of the Type (in bits).
 */
void
Type::setSize(unsigned sz)
{
	assert(0);
}
void
IntegerType::setSize(unsigned sz)
{
	size = sz;
}
void
FloatType::setSize(unsigned sz)
{
	size = sz;
}
void
PointerType::setSize(unsigned sz)
{
	assert(sz == STD_SIZE);
}
void
SizeType::setSize(unsigned sz)
{
	size = sz;
}

/**
 * \brief True if not unsigned.
 */
bool
IntegerType::isSigned() const
{
	return signedness >= 0;
}
/**
 * \brief True if not definitely signed.
 */
bool
IntegerType::isUnsigned() const
{
	return signedness <= 0;
}
/**
 * \brief A hint for signedness.
 */
void
IntegerType::bumpSigned(int sg)
{
	signedness += sg;
}
/**
 * \brief Set absolute signedness.
 */
void
IntegerType::setSigned(int sg)
{
	signedness = sg;
}
/**
 * \brief Get the signedness.
 */
int
IntegerType::getSignedness() const
{
	return signedness;
}

Signature *
FuncType::getSignature() const
{
	return signature;
}

void
FuncType::setSignature(Signature *sig)
{
	signature = sig;
}

void
PointerType::setPointsTo(Type *p)
{
	if (p == this) {  // Note: comparing pointers
		points_to = new VoidType();  // Can't point to self; impossible to compare, print, etc
		if (VERBOSE)
			LOG << "Warning: attempted to create pointer to self: " << (void *)this << "\n";
	} else {
		points_to = p;
	}
}

Type *
PointerType::getPointsTo() const
{
	return points_to;
}

/**
 * \brief Return x for \*\*x
 */
Type *
PointerType::getFinalPointsTo() const
{
	Type *ty = points_to;
	if (auto pt = dynamic_cast<PointerType *>(ty))
		return pt->getFinalPointsTo();
	return ty;
}

/**
 * \brief Return 2 for \*\*x
 */
int
PointerType::pointerDepth() const
{
	int d = 1;
	auto pt = dynamic_cast<PointerType *>(points_to);
	while (pt) {
		pt = dynamic_cast<PointerType *>(pt->points_to);
		++d;
	}
	return d;
}

Type *
ArrayType::getBaseType() const
{
	return base_type;
}

void
ArrayType::setBaseType(Type *b)
{
	// MVE: not sure if this is always the right thing to do
	if (!isUnbounded()) {
		unsigned baseSize = base_type->getBytes(); // Old base size (one element) in bytes
		if (baseSize == 0) baseSize = 1;  // Count void as size 1
		baseSize *= length;  // Old base size (length elements) in bytes
		unsigned newSize = b->getBytes();
		if (newSize == 0) newSize = 1;
		length = baseSize / newSize;  // Preserve same byte size for array
	}
	base_type = b;
}

void
ArrayType::fixBaseType(Type *b)
{
	if (!base_type) {
		base_type = b;
	} else {
		auto bt = dynamic_cast<ArrayType *>(base_type);
		assert(bt);
		bt->fixBaseType(b);
	}
}

unsigned
ArrayType::getLength() const
{
	return length;
}

void
ArrayType::setLength(unsigned n)
{
	length = n;
}

/**
 * We actually want unbounded arrays to still work correctly when computing
 * aliases.  As such, we give them a very large bound and hope that no-one
 * tries to alias beyond them.
 */
const unsigned ArrayType::NO_BOUND = 9999999;

bool
ArrayType::isUnbounded() const
{
	return length == NO_BOUND;
}

const std::string &
NamedType::getName() const
{
	return name;
}

CompoundType::const_iterator
CompoundType::cbegin() const
{
	return elems.cbegin();
}

CompoundType::const_iterator
CompoundType::cend() const
{
	return elems.cend();
}

Type *
CompoundType::getType(const_iterator it) const
{
	assert(it != cend());
	return it->type;
}

Type *
CompoundType::getType(const std::string &nam) const
{
	for (const auto &elem : elems)
		if (elem.name == nam)
			return elem.type;
	return nullptr;
}

const char *
CompoundType::getName(const_iterator it) const
{
	assert(it != cend());
	return it->name.c_str();
}

unsigned
CompoundType::getOffsetTo(const_iterator it) const
{
	unsigned offset = 0;
	for (auto ii = elems.cbegin(); ii != it; ++ii) {
		offset += ii->type->getSize();
	}
	return offset;
}

unsigned
CompoundType::getOffsetTo(const std::string &member) const
{
	unsigned offset = 0;
	for (const auto &elem : elems) {
		if (elem.name == member)
			return offset;
		offset += elem.type->getSize();
	}
	return (unsigned)-1;
}

unsigned
CompoundType::getOffsetRemainder(unsigned off) const
{
	unsigned r = off;
	unsigned offset = 0;
	for (const auto &elem : elems) {
		unsigned sz = elem.type->getSize();
		offset += sz;
		if (offset > off)
			break;
		r -= sz;
	}
	return r;
}

/**
 * \note off is a BIT offset.
 */
Type *
CompoundType::getTypeAtOffset(unsigned off) const
{
	unsigned offset = 0;
	for (const auto &elem : elems) {
		unsigned sz = elem.type->getSize();
		if (offset <= off && off < offset + sz)
			return elem.type;
		offset += sz;
	}
	return nullptr;
}

/**
 * \note off is a BIT offset.
 */
void
CompoundType::setTypeAtOffset(unsigned off, Type *ty)
{
	unsigned offset = 0;
	for (auto it = elems.begin(); it != elems.end(); ++it) {
		unsigned sz = it->type->getSize();
		if (offset <= off && off < offset + sz) {
			it->type = ty;
			unsigned newsz = ty->getSize();
			if (newsz < sz) {
				Element e;
				e.type = new SizeType(sz - newsz);
				e.name = "pad";
				it = elems.insert(++it, e);
			}
			return;
		}
		offset += sz;
	}
}

const char *
CompoundType::getNameAtOffset(unsigned off) const
{
	unsigned offset = 0;
	for (const auto &elem : elems) {
		unsigned sz = elem.type->getSize();
		if (offset <= off && off < offset + sz)
			return elem.name.c_str();
		offset += sz;
	}
	return nullptr;
}

void
CompoundType::setNameAtOffset(unsigned off, const std::string &nam)
{
	unsigned offset = 0;
	for (auto &elem : elems) {
		unsigned sz = elem.type->getSize();
		if (offset <= off && off < offset + sz) {
			elem.name = nam;
			return;
		}
		offset += sz;
	}
}

#if 0 // Cruft?
Type *
UpperType::getBaseType() const
{
	return base_type;
}

void
UpperType::setBaseType(Type *b)
{
	base_type = b;
}

Type *
LowerType::getBaseType() const
{
	return base_type;
}

void
LowerType::setBaseType(Type *b)
{
	base_type = b;
}
#endif

/**
 * \fn bool Type::operator ==(const Type &other) const
 * \brief Equality comparsion.  Considers sign.
 *
 * \param[in] other  Type being compared to.
 * \returns          this == other
 */
bool
IntegerType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const IntegerType *>(&other);
	if (!o) return false;
	// Note: zero size matches any other size (wild, or unknown, size)
	// Note: actual value of signedness is disregarded, just whether less than, equal to, or greater than 0
	return (size == 0 || o->size == 0 || size == o->size)
	    && ((signedness <  0 && o->signedness <  0)
	     || (signedness == 0 && o->signedness == 0)
	     || (signedness >  0 && o->signedness >  0));
}
bool
FloatType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const FloatType *>(&other);
	if (!o) return false;
	return (size == 0 || o->size == 0 || size == o->size);
}
bool
BooleanType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const BooleanType *>(&other);
	return !!o;
}
bool
CharType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const CharType *>(&other);
	return !!o;
}
bool
VoidType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const VoidType *>(&other);
	return !!o;
}
bool
FuncType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const FuncType *>(&other);
	if (!o) return false;
	// Note: some functions don't have a signature (e.g. indirect calls that have not yet been successfully analysed)
	if (!signature) return !o->signature;
	return *signature == *o->signature;
}
static int pointerCompareNest = 0;
bool
PointerType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const PointerType *>(&other);
	// return o && (*points_to == *o->points_to);
	if (!o) return false;
	if (++pointerCompareNest >= 20) {
		std::cerr << "PointerType operator == nesting depth exceeded!\n";
		return true;
	}
	bool ret = (*points_to == *o->points_to);
	--pointerCompareNest;
	return ret;
}
bool
ArrayType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const ArrayType *>(&other);
	if (!o) return false;
	return *base_type == *o->base_type
	    && length == o->length;
}
bool
NamedType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const NamedType *>(&other);
	if (!o) return false;
	return name == o->name;
}
bool
CompoundType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const CompoundType *>(&other);
	if (!o) return false;
	if (elems.size() != o->elems.size()) return false;
	for (auto it1 = elems.cbegin(), it2 = o->elems.cbegin(); it1 != elems.cend(); ++it1, ++it2)
		if (*it1->type != *it2->type)
			return false;
	return true;
}
bool
UnionType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const UnionType *>(&other);
	if (!o) return false;
	if (elems.size() != o->elems.size()) return false;
	for (auto it1 = elems.cbegin(), it2 = o->elems.cbegin(); it1 != elems.cend(); ++it1, ++it2)
		if (*it1->type != *it2->type)
			return false;
	return true;
}
bool
SizeType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const SizeType *>(&other);
	if (!o) return false;
	return size == o->size;
}
#if 0 // Cruft?
bool
UpperType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const UpperType *>(&other);
	if (!o) return false;
	return *base_type == *o->base_type;
}
bool
LowerType::operator ==(const Type &other) const
{
	auto o = dynamic_cast<const LowerType *>(&other);
	if (!o) return false;
	return *base_type == *o->base_type;
}
#endif

/**
 * \brief Inequality comparsion.  Considers sign.
 *
 * \param[in] other  Type being compared to.
 * \returns          this != other
 */
bool
Type::operator !=(const Type &other) const
{
	return !(*this == other);
}

/**
 * \brief Equality comparsion.  Considers only broad type.
 *
 * \param[in] other  Type being compared to.
 * \returns          this.id == other.id
 */
bool
Type::operator *=(const Type &other) const
{
	return getId() == other.getId();
}

#if 0
/**
 * \fn bool Type::operator -=(const Type &other) const
 * \brief Equality comparsion.  Ignores sign.
 *
 * True if equal in broad type and size, but not necessarily sign.  Considers
 * all float types > 64 bits to be the same.
 *
 * \param[in] other  Type being compared to.
 * \returns          this == other (ignoring sign)
 */
bool
IntegerType::operator -=(const Type &other) const
{
	auto o = dynamic_cast<const IntegerType *>(&other);
	if (!o) return false;
	return size == o->size;
}
bool
FloatType::operator -=(const Type &other) const
{
	auto o = dynamic_cast<const FloatType *>(&other);
	if (!o) return false;
	if (size > 64 && o->size > 64) return true;
	return size == o->size;
}
#endif

/**
 * \fn bool Type::operator <(const Type &other) const
 * \brief Ordering comparsion.  Considers sign.
 *
 * Defines an ordering between Types (and hence sets, etc. of Exp* using
 * lessExpStar).
 *
 * \param[in] other  Type being compared to.
 * \returns          this is less than other
 */
bool
IntegerType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const IntegerType *>(&other);
	if (!o) return getId() < other.getId();
	if (size != o->size) return size < o->size;
	return signedness < o->signedness;
}
bool
FloatType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const FloatType *>(&other);
	if (!o) return getId() < other.getId();
	return size < o->size;
}
bool
BooleanType::operator <(const Type &other) const
{
	return getId() < other.getId();
}
bool
CharType::operator <(const Type &other) const
{
	return getId() < other.getId();
}
bool
VoidType::operator <(const Type &other) const
{
	return getId() < other.getId();
}
bool
FuncType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const FuncType *>(&other);
	if (!o) return getId() < other.getId();
	// FIXME: Need to compare signatures
	return true;
}
bool
PointerType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const PointerType *>(&other);
	if (!o) return getId() < other.getId();
	return *points_to < *o->points_to;
}
bool
ArrayType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const ArrayType *>(&other);
	if (!o) return getId() < other.getId();
	return *base_type < *o->base_type;
}
bool
NamedType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const NamedType *>(&other);
	if (!o) return getId() < other.getId();
	return name < o->name;
}
bool
CompoundType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const CompoundType *>(&other);
	if (!o) return getId() < other.getId();
	return getSize() < o->getSize();  // This won't separate structs of the same size!! MVE
}
bool
UnionType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const UnionType *>(&other);
	if (!o) return getId() < other.getId();
	return elems.size() < o->elems.size();
}
bool
SizeType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const SizeType *>(&other);
	if (!o) return getId() < other.getId();
	return size < o->size;
}
#if 0 // Cruft?
bool
UpperType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const UpperType *>(&other);
	if (!o) return getId() < other.getId();
	return *base_type < *o->base_type;
}
bool
LowerType::operator <(const Type &other) const
{
	auto o = dynamic_cast<const LowerType *>(&other);
	if (!o) return getId() < other.getId();
	return *base_type < *o->base_type;
}
#endif

/**
 * \brief Match operation.
 *
 * \param pattern  Type to match.
 * \returns        Exp list of bindings if match, or null.
 */
Exp *
Type::match(Type *pattern)
{
	if (auto p = dynamic_cast<NamedType *>(pattern)) {
		LOG << "type match: " << this->getCtype() << " to " << p->getCtype() << "\n";
		return new Binary(opList,
		                  new Binary(opEqual,
		                             new Unary(opVar,
		                                       new Const(p->getName())),
		                             new TypeVal(this->clone())),
		                  new Terminal(opNil));
	}
	return nullptr;
}
Exp *
PointerType::match(Type *pattern)
{
	if (auto p = dynamic_cast<PointerType *>(pattern)) {
		LOG << "got pointer match: " << this->getCtype() << " to " << p->getCtype() << "\n";
		return points_to->match(p->getPointsTo());
	}
	return Type::match(pattern);
}
Exp *
ArrayType::match(Type *pattern)
{
	if (auto p = dynamic_cast<ArrayType *>(pattern))
		return base_type->match(p);
	return Type::match(pattern);
}

/**
 * \fn std::string Type::getCtype(bool final) const
 * \brief Return a string representing this Type.
 *
 * Get the C type, e.g. "unsigned int".
 *
 * \param[in] final  If false, include comment for lack of sign information
 *                   (in IntegerTypes).  Otherwise, this is final output,
 *                   choose a signedness.
 */
std::string
VoidType::getCtype(bool final) const
{
	return "void";
}
std::string
FuncType::getCtype(bool final) const
{
	return getReturn(final) + " " + getParam(final);
}
std::string
IntegerType::getCtype(bool final) const
{
	if (signedness >= 0) {
		std::string s;
		if (!final && signedness == 0)
			s = "/*signed?*/";
		switch (size) {
		case 32: s += "int";       break;
		case 16: s += "short";     break;
		case  8: s += "char";      break;
		case  1: s += "bool";      break;
		case 64: s += "long long"; break;
		default:
			if (!final) s += "?";  // To indicate invalid/unknown size
			s += "int";
		}
		return s;
	} else {
		switch (size) {
		case 32: return "unsigned int";
		case 16: return "unsigned short";
		case  8: return "unsigned char";
		case  1: return "bool";
		case 64: return "unsigned long long";
		default:
			if (final) return "unsigned int";
			else return "?unsigned int";
		}
	}
}
std::string
FloatType::getCtype(bool final) const
{
	switch (size) {
	case 32: return "float";
	case 64: return "double";
	default: return "double";
	}
}
std::string
BooleanType::getCtype(bool final) const
{
	return "bool";
}
std::string
CharType::getCtype(bool final) const
{
	return "char";
}
std::string
PointerType::getCtype(bool final) const
{
	std::string s = points_to->getCtype(final);
	if (dynamic_cast<PointerType *>(points_to))
		s += "*";
	else
		s += " *";
	return s;
}
std::string
ArrayType::getCtype(bool final) const
{
	std::ostringstream ost;
	ost << base_type->getCtype(final) << "[";
	if (!isUnbounded())
		ost << length;
	ost << "]";
	return ost.str();
}
std::string
NamedType::getCtype(bool final) const
{
	return name;
}
std::string
CompoundType::getCtype(bool final) const
{
	std::string tmp = "struct { ";
	for (const auto &elem : elems) {
		tmp += elem.type->getCtype(final);
		if (!elem.name.empty()) {
			tmp += " ";
			tmp += elem.name;
		}
		tmp += "; ";
	}
	tmp += "}";
	return tmp;
}
std::string
UnionType::getCtype(bool final) const
{
	std::string tmp = "union { ";
	for (const auto &elem : elems) {
		tmp += elem.type->getCtype(final);
		if (!elem.name.empty()) {
			tmp += " ";
			tmp += elem.name;
		}
		tmp += "; ";
	}
	tmp += "}";
	return tmp;
}
std::string
SizeType::getCtype(bool final) const
{
	// Emit a comment and the size
	std::ostringstream ost;
	ost << "__size" << size;
	return ost.str();
}
#if 0 // Cruft?
std::string
UpperType::getCtype(bool final) const
{
	std::ostringstream ost;
	ost << "/*upper*/(" << base_type << ")";
	return ost.str();
}
std::string
LowerType::getCtype(bool final) const
{
	std::ostringstream ost;
	ost << "/*lower*/(" << base_type << ")";
	return ost.str();
}
#endif

std::string
FuncType::getReturn(bool final) const
{
	if (!signature
	 || signature->getNumReturns() == 0)
		return "void";
	return signature->getReturnType(0)->getCtype(final);
}

std::string
FuncType::getParam(bool final) const
{
	if (!signature)
		return "(void)";
	std::string s = "(";
	for (unsigned i = 0; i < signature->getNumParams(); ++i) {
		if (i != 0) s += ", ";
		s += signature->getParamType(i)->getCtype(final);
	}
	s += ")";
	return s;
}

/**
 * \brief For debugging.
 */
std::string
Type::prints() const
{
	return getCtype(false);
}

bool
Type::isCString()
{
	if (!resolvesToPointer())
		return false;
	Type *p = asPointer()->getPointsTo();
	if (p->resolvesToChar())
		return true;
	if (!p->resolvesToArray())
		return false;
	p = p->asArray()->getBaseType();
	return p->resolvesToChar();
}

/**
 * Dictionary of user-defined types (typedefs).
 */
std::map<std::string, Type *> Type::namedTypes;

/**
 * Adds a named type to the dictionary.  Takes ownership of the Type object.
 */
void
Type::addNamedType(const std::string &name, Type *type)
{
	auto prev = getNamedType(name);
	if (!prev) {
		// check if it is:
		// typedef int a;
		// typedef a b;
		// we then need to define b as int
		// we create clones to keep the GC happy
		auto it = namedTypes.find(type->getCtype());
		if (it != namedTypes.end()) {
			delete type;
			type = it->second->clone();
		}
		namedTypes[name] = type;
	} else {
		// Already exists.  Complain if different, then discard.
		if (*type != *prev) {
			//LOG << "addNamedType: name " << name
			//    << " type " << type->getCtype()
			//    << " != " << prev->getCtype() << "\n";
			std::cerr << "Warning: Type::addNamedType: Attempted redefinition of type " << name << "\n"
			          << " type     = " << type->prints() << "\n"
			          << " previous = " << prev->prints() << "\n";
		}
		delete type;
	}
}

Type *
Type::getNamedType(const std::string &name)
{
	auto it = namedTypes.find(name);
	if (it != namedTypes.end())
		return it->second;
	return nullptr;
}

/**
 * Clear the named type map.  This is necessary when testing; the type for the
 * first parameter to 'main' is different for sparc and pentium.
 */
void
Type::clearNamedTypes()
{
	namedTypes.clear();
}

/**
 * \brief Given the name of a temporary variable, return its Type.
 *
 * \note            Caller must delete result.
 * \param[in] name  A string (e.g. "tmp", "tmpd").
 * \returns         Ptr to a new Type object.
 */
Type *
Type::getTempType(const std::string &name)
{
	char ctype = ' ';
	if (name.size() > 3) ctype = name[3];
	switch (ctype) {
	// They are all int32, except for a few specials
	case 'f': return new FloatType(32);
	case 'd': return new FloatType(64);
	case 'F': return new FloatType(80);
	case 'D': return new FloatType(128);
	case 'l': return new IntegerType(64);
	case 'h': return new IntegerType(16);
	case 'b': return new IntegerType(8);
	default:  return new IntegerType(32);
	}
}

/**
 * \brief Get a temporary name for the Type.
 *
 * It'd be even nicer to return a unique name, but we don't know scope at this
 * point, and even so we could still clash with a user-defined name later on
 * :(
 *
 * \returns  A minimal temporary name for this Type.
 */
std::string
Type::getTempName() const
{
	return std::string("tmp"); // what else can we do? (besides panic)
}
std::string
IntegerType::getTempName() const
{
	switch (size) {
	case 1:  /* Treat as a tmpb */
	case 8:  return std::string("tmpb");
	case 16: return std::string("tmph");
	case 32: return std::string("tmpi");
	case 64: return std::string("tmpl");
	default: return std::string("tmp");
	}
}
std::string
FloatType::getTempName() const
{
	switch (size) {
	case 32:  return std::string("tmpf");
	case 64:  return std::string("tmpd");
	case 80:  return std::string("tmpF");
	case 128: return std::string("tmpD");
	default:  return std::string("tmp");
	}
}

int NamedType::nextAlpha = 0;
/**
 * \brief Get a new type variable, e.g. alpha0, alpha55.
 * \note alpha is therefore a "reserved name" for types.
 */
NamedType *
NamedType::getAlpha()
{
	std::ostringstream ost;
	ost << "alpha" << nextAlpha++;
	return new NamedType(ost.str());
}

PointerType *
PointerType::newPtrAlpha()
{
	return new PointerType(NamedType::getAlpha());
}

bool
PointerType::pointsToAlpha() const
{
	// void* counts as alpha* (and may replace it soon)
	if (dynamic_cast<VoidType *>(points_to)) return true;
	if (auto pt = dynamic_cast<NamedType *>(points_to))
		return pt->getName().compare(0, 5, "alpha") == 0;
	return false;
}

bool
Type::isPointerToAlpha()
{
	auto pt = dynamic_cast<PointerType *>(this);
	return pt && pt->pointsToAlpha();
}

Type *
NamedType::resolvesTo() const
{
	Type *ty = getNamedType(name);
	if (auto nt = dynamic_cast<NamedType *>(ty))
		return nt->resolvesTo();
	return ty;
}

Type *
Type::resolvesTo()
{
	if (auto nt = dynamic_cast<const NamedType *>(this))
		return nt->resolvesTo();
	return this;
}

const Type *
Type::resolvesTo() const
{
	if (auto nt = dynamic_cast<const NamedType *>(this))
		return nt->resolvesTo();
	return this;
}

#define AS_TYPE(x) \
x##Type * \
Type::as##x() \
{ \
	auto res = dynamic_cast<x##Type *>(resolvesTo()); \
	assert(res); \
	return res; \
}

AS_TYPE(Void)
AS_TYPE(Func)
AS_TYPE(Boolean)
AS_TYPE(Char)
AS_TYPE(Integer)
AS_TYPE(Float)
AS_TYPE(Pointer)
AS_TYPE(Array)
AS_TYPE(Compound)
AS_TYPE(Size)
AS_TYPE(Union)
//AS_TYPE(Upper)
//AS_TYPE(Lower)

/**
 * \note Don't want to call this->resolvesTo() for this case, since then we
 * (probably) won't have a NamedType and the assert will fail.
 */
NamedType *
Type::asNamed()
{
	auto res = dynamic_cast<NamedType *>(this);
	assert(res);
	return res;
}

#define RESOLVES_TO_TYPE(x) \
bool \
Type::resolvesTo##x() const \
{ \
	return !!dynamic_cast<const x##Type *>(resolvesTo()); \
}

RESOLVES_TO_TYPE(Void)
RESOLVES_TO_TYPE(Func)
RESOLVES_TO_TYPE(Boolean)
RESOLVES_TO_TYPE(Char)
RESOLVES_TO_TYPE(Integer)
RESOLVES_TO_TYPE(Float)
RESOLVES_TO_TYPE(Pointer)
RESOLVES_TO_TYPE(Array)
RESOLVES_TO_TYPE(Compound)
RESOLVES_TO_TYPE(Union)
RESOLVES_TO_TYPE(Size)
//RESOLVES_TO_TYPE(Upper)
//RESOLVES_TO_TYPE(Lower)

/**
 * \brief Print in \*i32\* format.
 */
void
Type::starPrint(std::ostream &os) const
{
	os << "*" << *this << "*";
}

/**
 * \fn void Type::print(std::ostream &os) const
 * \brief Print a crude shortcut representation of a Type.
 */
void
VoidType::print(std::ostream &os) const
{
	os << 'v';
}
void
FuncType::print(std::ostream &os) const
{
	os << "func";
}
void
BooleanType::print(std::ostream &os) const
{
	os << 'b';
}
void
CharType::print(std::ostream &os) const
{
	os << 'c';
}
void
IntegerType::print(std::ostream &os) const
{
	int sg = getSignedness();
	// 'j' for either i or u, don't know which
	os << (sg == 0 ? 'j' : sg > 0 ? 'i' : 'u') << getSize();
}
void
FloatType::print(std::ostream &os) const
{
	os << 'f' << getSize();
}
void
PointerType::print(std::ostream &os) const
{
	os << getPointsTo() << '*';
}
void
ArrayType::print(std::ostream &os) const
{
	os << '[' << getBaseType();
	if (!isUnbounded())
		os << ", " << getLength();
	os << ']';
}
void
NamedType::print(std::ostream &os) const
{
	os << getName();
}
void
CompoundType::print(std::ostream &os) const
{
	os << "struct";
}
void
UnionType::print(std::ostream &os) const
{
	os << "union";
	//os << getCtype();
}
void
SizeType::print(std::ostream &os) const
{
	os << getSize();
}
#if 0 // Cruft?
void
UpperType::print(std::ostream &os) const
{
	os << "U(" << getBaseType() << ')';
}
void
LowerType::print(std::ostream &os) const
{
	os << "L(" << getBaseType() << ')';
}
#endif

std::ostream &
operator <<(std::ostream &os, const Type *t)
{
	if (!t) return os << '0';
	return os << *t;
}
std::ostream &
operator <<(std::ostream &os, const Type &t)
{
	t.print(os);
	return os;
}

/**
 * Constraint-based TA:  Merge this Type with another,
 * e.g. size16 with integer-of-size-0 -> int16.
 *
 * FIXME:  Aren't mergeWith() and meetWith() really the same thing?
 */
Type *
Type::mergeWith(Type *other)
{
	assert(0);
	return nullptr;
}
Type *
IntegerType::mergeWith(Type *other)
{
	if (*this == *other) return this;
	auto o = dynamic_cast<IntegerType *>(other);
	if (!o) return nullptr;  // Can you merge with a pointer?
	auto ret = (IntegerType *)this->clone();
	if (ret->size == 0) ret->size = o->size;
	if (ret->signedness == 0) ret->signedness = o->signedness;
	return ret;
}
Type *
SizeType::mergeWith(Type *other)
{
	Type *ret = other->clone();
	ret->setSize(size);
	return ret;
}
#if 0 // Cruft?
Type *
UpperType::mergeWith(Type *other)
{
	// FIXME: TBC
	return this;
}
Type *
LowerType::mergeWith(Type *other)
{
	// FIXME: TBC
	return this;
}
#endif

/**
 * \returns true if this is a substructure of other, i.e. other has the same
 * types at the same offsets as this.
 */
bool
CompoundType::isSubStructOf(const Type &other) const
{
	auto o = dynamic_cast<const CompoundType *>(&other);
	if (!o) return false;
	if (elems.size() > o->elems.size()) return false;
	for (auto it1 = elems.cbegin(), it2 = o->elems.cbegin(); it1 != elems.cend(); ++it1, ++it2)
		if (*it1->type != *it2->type)
			return false;
	return true;
}

/**
 * \returns true if ty is already in the union.
 * \note Linear search, but number of types is usually small.
 */
bool
UnionType::findType(Type *ty) const
{
	for (const auto &elem : elems) {
		if (*elem.type == *ty)
			return true;
	}
	return false;
}

/**
 * \brief Return a new Bool/Char/Int.
 */
Type *
Type::newIntegerLikeType(unsigned size, int signedness)
{
	if (size == 1)
		return new BooleanType();
	if (size == 8 && signedness >= 0)
		return new CharType();
	return new IntegerType(size, signedness);
}

/**
 * \brief Return an iterator to the entry for it, or end() if none.
 *
 * Find the entry that overlaps with addr.  If none, return end().  We have to
 * use upper_bound and decrement the iterator, because we might want an entry
 * that starts earlier than addr yet still overlaps it.
 */
DataIntervalMap::iterator
DataIntervalMap::find_it(ADDRESS addr)
{
	auto it = dimap.upper_bound(addr);  // Find the first item strictly greater than addr
	if (it == dimap.begin())
		return dimap.end();  // None <= this address, so no overlap possible
	--it;  // If any item overlaps, it is this one
	if (it->first <= addr && it->first + it->second.size > addr)
		// This is the one that overlaps with addr
		return it;
	return dimap.end();
}

/**
 * \brief Find the DataInterval at address addr, or null if none.
 */
DataIntervalMap::value_type *
DataIntervalMap::find(ADDRESS addr)
{
	auto it = find_it(addr);
	if (it == dimap.end())
		return nullptr;
	return &*it;
}

/**
 * \brief True if from addr for size bytes is clear.
 */
bool
DataIntervalMap::isClear(ADDRESS addr, unsigned size)
{
	auto it = dimap.upper_bound(addr + size - 1); // Find the first item strictly greater than address of last byte
	if (it == dimap.begin())
		return true;  // None <= this address, so no overlap possible
	--it;  // If any item overlaps, it is this one
	// Make sure the previous item ends before this one will start
	ADDRESS end;
	if (it->first + it->second.size < it->first)
		// overflow
		end = 0xFFFFFFFF;  // Overflow
	else
		end = it->first + it->second.size;
	if (end <= addr)
		return true;
	if (auto at = dynamic_cast<ArrayType *>(it->second.type)) {
		if (at->isUnbounded()) {
			it->second.size = addr - it->first;
			LOG << "shrinking size of unbound array to " << it->second.size << " bytes\n";
			return true;
		}
	}
	return false;
}

/**
 * \brief Add a new data item.
 *
 * With the forced parameter:  Are we forcing the name, the type, or always
 * both?
 */
void
DataIntervalMap::addItem(ADDRESS addr, const char *name, Type *ty, bool forced)
{
	if (!name)
		name = "<noname>";
	auto pdie = find(addr);
	if (!pdie) {
		// Check that this new item is compatible with any items it overlaps with, and insert it
		replaceComponents(addr, name, ty, forced);
		return;
	}
	// There are two basic cases, and an error if the two data types weave
	if (pdie->first < addr) {
		// The existing entry comes first. Make sure it ends last (possibly equal last)
		if (pdie->first + pdie->second.size < addr + ty->getSize() / 8) {
			LOG << "TYPE ERROR: attempt to insert item " << name << " at 0x" << std::hex << addr << std::dec << " of type " << ty->getCtype()
			    << " which weaves after " << pdie->second.name << " at 0x" << std::hex << pdie->first << std::dec << " of type " << pdie->second.type->getCtype() << "\n";
			return;
		}
		enterComponent(pdie, addr, name, ty, forced);
	} else if (pdie->first == addr) {
		// Could go either way, depending on where the data items end
		unsigned endOfCurrent = pdie->first + pdie->second.size;
		unsigned endOfNew = addr + ty->getSize() / 8;
		if (endOfCurrent < endOfNew)
			replaceComponents(addr, name, ty, forced);
		else if (endOfCurrent == endOfNew)
			checkMatching(pdie, addr, name, ty, forced);  // Size match; check that new type matches old
		else
			enterComponent(pdie, addr, name, ty, forced);
	} else {
		// Old starts after new; check it also ends first
		if (pdie->first + pdie->second.size > addr + ty->getSize() / 8) {
			LOG << "TYPE ERROR: attempt to insert item " << name << " at 0x" << std::hex << addr << std::dec << " of type " << ty->getCtype()
			    << " which weaves before " << pdie->second.name << " at 0x" << std::hex << pdie->first << std::dec << " of type " << pdie->second.type->getCtype() << "\n";
			return;
		}
		replaceComponents(addr, name, ty, forced);
	}
}

/**
 * We are entering an item that already exists in a larger type.  Check for
 * compatibility, meet if necessary.
 */
void
DataIntervalMap::enterComponent(value_type *pdie, ADDRESS addr, const char *name, Type *ty, bool forced)
{
	if (pdie->second.type->resolvesToCompound()) {
		unsigned bitOffset = (addr - pdie->first) * 8;
		Type *memberType = pdie->second.type->asCompound()->getTypeAtOffset(bitOffset);
		if (memberType->isCompatibleWith(*ty)) {
			bool ch;
			memberType = memberType->meetWith(ty, ch);
			pdie->second.type->asCompound()->setTypeAtOffset(bitOffset, memberType);
		} else
			LOG << "TYPE ERROR: At address 0x" << std::hex << addr << std::dec << " type " << ty->getCtype()
			    << " is not compatible with existing structure member type " << memberType->getCtype() << "\n";
	} else if (pdie->second.type->resolvesToArray()) {
		Type *memberType = pdie->second.type->asArray()->getBaseType();
		if (memberType->isCompatibleWith(*ty)) {
			bool ch;
			memberType = memberType->meetWith(ty, ch);
			pdie->second.type->asArray()->setBaseType(memberType);
		} else
			LOG << "TYPE ERROR: At address 0x" << std::hex << addr << std::dec << " type " << ty->getCtype()
			    << " is not compatible with existing array member type " << memberType->getCtype() << "\n";
	} else
		LOG << "TYPE ERROR: Existing type at address 0x" << std::hex << pdie->first << std::dec << " is not structure or array type\n";
}

/**
 * We are entering a struct or array that overlaps existing components.  Check
 * for compatibility, and move the components out of the way, meeting if
 * necessary.
 */
void
DataIntervalMap::replaceComponents(ADDRESS addr, const char *name, Type *ty, bool forced)
{
	unsigned pastLast = addr + ty->getSize() / 8; // This is the byte address just past the type to be inserted
	// First check that the new entry will be compatible with everything it will overlap
	if (ty->resolvesToCompound()) {
		auto it1 = dimap.lower_bound(addr);  // Iterator to the first overlapping item (could be end(), but
		                                     // if so, it2 will also be end())
		auto it2 = dimap.upper_bound(pastLast - 1); // Iterator to the first item that starts too late
		for (auto it = it1; it != it2; ++it) {
			unsigned bitOffset = (it->first - addr) * 8;
			Type *memberType = ty->asCompound()->getTypeAtOffset(bitOffset);
			if (memberType->isCompatibleWith(*it->second.type, true)) {
				bool ch;
				memberType = it->second.type->meetWith(memberType, ch);
				ty->asCompound()->setTypeAtOffset(bitOffset, memberType);
			} else {
				LOG << "TYPE ERROR: At address 0x" << std::hex << addr << std::dec << " struct type " << ty->getCtype()
				    << " is not compatible with existing type " << it->second.type->getCtype() << "\n";
				return;
			}
		}
	} else if (ty->resolvesToArray()) {
		Type *memberType = ty->asArray()->getBaseType();
		auto it1 = dimap.lower_bound(addr);
		auto it2 = dimap.upper_bound(pastLast - 1);
		for (auto it = it1; it != it2; ++it) {
			if (memberType->isCompatibleWith(*it->second.type, true)) {
				bool ch;
				memberType = memberType->meetWith(it->second.type, ch);
				ty->asArray()->setBaseType(memberType);
			} else {
				LOG << "TYPE ERROR: At address 0x" << std::hex << addr << std::dec << " array type " << ty->getCtype()
				    << " is not compatible with existing type " << it->second.type->getCtype() << "\n";
				return;
			}
		}
	} else {
		// Just make sure it doesn't overlap anything
		if (!isClear(addr, ty->getBytes())) {
			LOG << "TYPE ERROR: at address 0x" << std::hex << addr << std::dec << ", overlapping type " << ty->getCtype()
			    << " does not resolve to compound or array\n";
			return;
		}
	}

	// The compound or array type is compatible. Remove the items that it will overlap with
	auto it1 = dimap.lower_bound(addr);
	auto it2 = dimap.upper_bound(pastLast - 1);

	// Check for existing locals that need to be updated
	if (ty->resolvesToCompound() || ty->resolvesToArray()) {
		Exp *rsp = Location::regOf(proc->getSignature()->getStackRegister());
		auto rsp0 = new RefExp(rsp, proc->getCFG()->findTheImplicitAssign(rsp));  // sp{0}
		for (auto it = it1; it != it2; ++it) {
			// Check if there is an existing local here
			Exp *locl = Location::memOf(new Binary(opPlus,
			                                       rsp0->clone(),
			                                       new Const(it->first)));
			locl->simplifyArith();  // Convert m[sp{0} + -4] to m[sp{0} - 4]
			Type *elemTy;
			int bitOffset = (it->first - addr) / 8;
			if (ty->resolvesToCompound())
				elemTy = ty->asCompound()->getTypeAtOffset(bitOffset);
			else
				elemTy = ty->asArray()->getBaseType();
			const char *locName = proc->findLocal(locl, elemTy);
			if (locName && ty->resolvesToCompound()) {
				CompoundType *c = ty->asCompound();
				// want s.m where s is the new compound object and m is the member at offset bitOffset
				const char *memName = c->getNameAtOffset(bitOffset);
				Exp *s = Location::memOf(new Binary(opPlus,
				                                    rsp0->clone(),
				                                    new Const(addr)));
				s->simplifyArith();
				Exp *memberExp = new Binary(opMemberAccess,
				                            s,
				                            new Const(memName));
				proc->mapSymbolTo(locl, memberExp);
			} else {
				// FIXME: to be completed
			}
		}
	}

	for (auto it = it1; it != it2 && it != dimap.end(); )
		it = dimap.erase(it);

	auto &di = dimap[addr];  // Finally add the new entry
	di.size = ty->getBytes();
	di.name = name;
	di.type = ty;
}

void
DataIntervalMap::checkMatching(value_type *pdie, ADDRESS addr, const char *name, Type *ty, bool forced)
{
	if (pdie->second.type->isCompatibleWith(*ty)) {
		// Just merge the types and exit
		bool ch;
		pdie->second.type = pdie->second.type->meetWith(ty, ch);
		return;
	}
	LOG << "TYPE DIFFERENCE (could be OK): At address 0x" << std::hex << addr << std::dec
	    << " existing type " << pdie->second.type->getCtype()
	    << " but added type " << ty->getCtype() << "\n";
}

/**
 * \brief Mainly for testing?
 */
void
DataIntervalMap::deleteItem(ADDRESS addr)
{
	auto it = dimap.find(addr);
	if (it != dimap.end())
		dimap.erase(it);
}

/**
 * \brief For test and debug.
 */
std::string
DataIntervalMap::prints() const
{
	std::ostringstream ost;
	for (const auto &di : dimap)
		ost << std::hex << "0x" << di.first << std::dec << " " << di.second.name << " " << di.second.type->getCtype() << "\n";
	return ost.str();
}

/**
 * From a complex type like an array of structs with a float, return a list of
 * components so you can construct e.g. myarray1[8].mystruct2.myfloat7
 */
Type::ComplexTypeCompList &
Type::compForAddress(ADDRESS addr, DataIntervalMap &dim)
{
	auto pdie = dim.find(addr);
	auto res = new ComplexTypeCompList;
	if (!pdie) return *res;
	ADDRESS startCurrent = pdie->first;
	Type *curType = pdie->second.type;
	while (startCurrent < addr) {
		unsigned bitOffset = (addr - startCurrent) * 8;
		if (auto compCurType = dynamic_cast<CompoundType *>(curType)) {
			unsigned rem = compCurType->getOffsetRemainder(bitOffset);
			startCurrent = addr - (rem / 8);
			ComplexTypeComp ctc;
			ctc.isArray = false;
			ctc.u.memberName = strdup(compCurType->getNameAtOffset(bitOffset));
			res->push_back(ctc);
			curType = compCurType->getTypeAtOffset(bitOffset);
		} else if (auto arrCurType = dynamic_cast<ArrayType *>(curType)) {
			curType = arrCurType->getBaseType();
			unsigned baseSize = curType->getSize();
			unsigned index = bitOffset / baseSize;
			startCurrent += index * baseSize / 8;
			ComplexTypeComp ctc;
			ctc.isArray = true;
			ctc.u.index = index;
			res->push_back(ctc);
		} else {
			LOG << "TYPE ERROR: no struct or array at byte address 0x" << std::hex << addr << std::dec << "\n";
			return *res;
		}
	}
	return *res;
}

void
CompoundType::addType(Type *n, const std::string &str)
{
	// check if it is a user defined type (typedef)
	Type *t = getNamedType(n->getCtype());
	if (t) n = t;
	Element e;
	e.type = n;
	e.name = str;
	elems.push_back(e);
}

void
UnionType::addType(Type *n, const std::string &str)
{
	if (auto ut = dynamic_cast<UnionType *>(n)) {
		// Note: need to check for name clashes eventually
		elems.insert(elems.end(), ut->elems.begin(), ut->elems.end());
	} else {
		if (auto pt = dynamic_cast<PointerType *>(n)) {
			if (pt->getPointsTo() == this) {  // Note: pointer comparison
				n = new PointerType(new VoidType);
				if (VERBOSE)
					LOG << "Warning: attempt to union with pointer to self!\n";
			}
		}
		Element e;
		e.type = n;
		e.name = str;
		elems.push_back(e);
	}
}

bool
CompoundType::isGeneric() const
{
	return generic;
}

/**
 * \brief Add a new generic member if necessary.
 *
 * Update this compound to use the fact that offset off has type ty.
 */
void
CompoundType::updateGenericMember(unsigned off, Type *ty, bool &ch)
{
	assert(generic);
	if (auto existingType = getTypeAtOffset(off)) {
		existingType = existingType->meetWith(ty, ch);
	} else {
		std::ostringstream ost;
		ost << "member" << nextGenericMemberNum++;
		setTypeAtOffset(off * 8, ty);
		setNameAtOffset(off * 8, ost.str());
	}
}


#ifdef USING_MEMO
class FuncTypeMemo : public Memo {
public:
	FuncTypeMemo(int m) : Memo(m) { }
	Signature *signature;
};

Memo *
FuncType::makeMemo(int mId)
{
	auto m = new FuncTypeMemo(mId);
	m->signature = signature;

	signature->takeMemo(mId);
	return m;
}

void
FuncType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<FuncTypeMemo *>(mm);
	signature = m->signature;

	//signature->restoreMemo(m->mId, dec);
}

class IntegerTypeMemo : public Memo {
public:
	IntegerTypeMemo(int m) : Memo(m) { }
	unsigned size;
	int signedness;
};

Memo *
IntegerType::makeMemo(int mId)
{
	auto m = new IntegerTypeMemo(mId);
	m->size = size;
	m->signedness = signedness;
	return m;
}

void
IntegerType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<IntegerTypeMemo *>(mm);
	size = m->size;
	signedness = m->signedness;
}

class FloatTypeMemo : public Memo {
public:
	FloatTypeMemo(int m) : Memo(m) { }
	unsigned size;
};

Memo *
FloatType::makeMemo(int mId)
{
	auto m = new FloatTypeMemo(mId);
	m->size = size;
	return m;
}

void
FloatType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<FloatTypeMemo *>(mm);
	size = m->size;
}

class PointerTypeMemo : public Memo {
public:
	PointerTypeMemo(int m) : Memo(m) { }
	Type *points_to;
};

Memo *
PointerType::makeMemo(int mId)
{
	auto m = new PointerTypeMemo(mId);
	m->points_to = points_to;

	points_to->takeMemo(mId);

	return m;
}

void
PointerType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<PointerTypeMemo *>(mm);
	points_to = m->points_to;

	points_to->restoreMemo(m->mId, dec);
}

class ArrayTypeMemo : public Memo {
public:
	ArrayTypeMemo(int m) : Memo(m) { }
	Type *base_type;
	unsigned length;
};

Memo *
ArrayType::makeMemo(int mId)
{
	auto m = new ArrayTypeMemo(mId);
	m->base_type = base_type;
	m->length = length;

	base_type->takeMemo(mId);

	return m;
}

void
ArrayType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<ArrayTypeMemo *>(mm);
	length = m->length;
	base_type = m->base_type;

	base_type->restoreMemo(m->mId, dec);
}

class NamedTypeMemo : public Memo {
public:
	NamedTypeMemo(int m) : Memo(m) { }
	std::string name;
	int nextAlpha;
};

Memo *
NamedType::makeMemo(int mId)
{
	auto m = new NamedTypeMemo(mId);
	m->name = name;
	m->nextAlpha = nextAlpha;
	return m;
}

void
NamedType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<NamedTypeMemo *>(mm);
	name = m->name;
	nextAlpha = m->nextAlpha;
}

class CompoundTypeMemo : public Memo {
public:
	CompoundTypeMemo(int m) : Memo(m) { }
	std::vector<CompoundType::Element> elems;
};

Memo *
CompoundType::makeMemo(int mId)
{
	auto m = new CompoundTypeMemo(mId);
	m->elems = elems;

	for (const auto &elem : elems)
		elem.type->takeMemo(mId);
	return m;
}

void
CompoundType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<CompoundTypeMemo *>(mm);
	elems = m->elems;

	for (const auto &elem : elems)
		elem.type->restoreMemo(m->mId, dec);
}

class UnionTypeMemo : public Memo {
public:
	UnionTypeMemo(int m) : Memo(m) { }
	std::list<UnionType::Element> li;
};

Memo *
UnionType::makeMemo(int mId)
{
	auto m = new UnionTypeMemo(mId);
	m->elems = elems;

	for (const auto &elem : elems)
		elem.type->takeMemo(mId);  // Is this right? What about the names? MVE
	return m;
}

void
UnionType::readMemo(Memo *mm, bool dec)
{
	auto m = dynamic_cast<UnionTypeMemo *>(mm);
	elems = m->elems;

	for (const auto &elem : elems)
		elem.type->restoreMemo(m->mId, dec);
}

// Don't insert new functions here! (Unles memo related.) Inside #ifdef USING_MEMO!

#endif
