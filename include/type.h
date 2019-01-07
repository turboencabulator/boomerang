/**
 * \file
 * \brief Definition of the Type class:  Low-level type information.
 *
 * Note that we may have a completely different system for recording
 * high-level types.
 *
 * \authors
 * Copyright (C) 2000-2001, The University of Queensland
 * \authors
 * Copyright (C) 2002-2006, Trent Waddington and Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TYPE_H
#define TYPE_H

#include "types.h"          // For STD_SIZE

#include <ostream>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <cassert>

class VoidType;
class FuncType;
class BooleanType;
class CharType;
class IntegerType;
class FloatType;
class NamedType;
class PointerType;
class ArrayType;
class CompoundType;
class UnionType;
class SizeType;
class UpperType;
class LowerType;

class DataIntervalMap;
class Exp;
class Signature;
class UserProc;

enum eType {
	eVoid,
	eFunc,
	eBoolean,
	eChar,
	eInteger,
	eFloat,
	ePointer,
	eArray,
	eNamed,
	eCompound,
	eUnion,
	eSize,
	eUpper,
	eLower
};

// The following two are for Type::compForAddress()
struct ComplexTypeComp {
	bool isArray;
	union {
		char *memberName;           // Member name if offset
		unsigned index;             // Constant index if array
	} u;
};
typedef std::list<ComplexTypeComp> ComplexTypeCompList;

class Type {
	friend class XMLProgParser;

protected:
	        eType       id;  // For operator < mostly
private:
	static  std::map<std::string, Type *> namedTypes;

public:
	// Constructors
	                    Type(eType id);
	virtual            ~Type();
	        eType       getId() const { return id; }

	static void         addNamedType(const std::string &name, Type *type);
	static Type        *getNamedType(const std::string &name);

	// Return type for given temporary variable name
	static Type        *getTempType(const std::string &name);
	static Type        *parseType(const std::string &); // parse a C type

	bool                isCString();

	// runtime type information. Deprecated for most situations; use resolvesToTYPE()
	virtual bool        isVoid()        const { return false; }
	virtual bool        isFunc()        const { return false; }
	virtual bool        isBoolean()     const { return false; }
	virtual bool        isChar()        const { return false; }
	virtual bool        isInteger()     const { return false; }
	virtual bool        isFloat()       const { return false; }
	virtual bool        isPointer()     const { return false; }
	virtual bool        isArray()       const { return false; }
	virtual bool        isNamed()       const { return false; }
	virtual bool        isCompound()    const { return false; }
	virtual bool        isUnion()       const { return false; }
	virtual bool        isSize()        const { return false; }
	virtual bool        isUpper()       const { return false; }
	virtual bool        isLower()       const { return false; }

	// Return false if some info is missing, e.g. unknown sign, size or basic type
	virtual bool        isComplete() const { return true; }

	// These replace type casts
	        VoidType     *asVoid();
	        FuncType     *asFunc();
	        BooleanType  *asBoolean();
	        CharType     *asChar();
	        IntegerType  *asInteger();
	        FloatType    *asFloat();
	        PointerType  *asPointer();
	        ArrayType    *asArray();
	        NamedType    *asNamed();
	        CompoundType *asCompound();
	        UnionType    *asUnion();
	        SizeType     *asSize();
	        UpperType    *asUpper();
	        LowerType    *asLower();

	// These replace calls to isNamed() and resolvesTo()
	        bool        resolvesToVoid() const;
	        bool        resolvesToFunc() const;
	        bool        resolvesToBoolean() const;
	        bool        resolvesToChar() const;
	        bool        resolvesToInteger() const;
	        bool        resolvesToFloat() const;
	        bool        resolvesToPointer() const;
	        bool        resolvesToArray() const;
	        bool        resolvesToCompound() const;
	        bool        resolvesToUnion() const;
	        bool        resolvesToSize() const;
	        bool        resolvesToUpper() const;
	        bool        resolvesToLower() const;

	// cloning
	virtual Type       *clone() const = 0;

	// Comparisons
	virtual bool        operator ==(const Type &other) const = 0;    // Considers sign
	virtual bool        operator !=(const Type &other) const;        // Considers sign
	//virtual bool        operator -=(const Type &other) const = 0;    // Ignores sign
	virtual bool        operator < (const Type &other) const = 0;    // Considers sign
	        bool        operator *=(const Type &other) const {       // Consider only broad type
		                    return id == other.id;
	                    }
	virtual Exp        *match(Type *pattern);
	// Constraint-based TA: merge one type with another, e.g. size16 with integer-of-size-0 -> int16
	virtual Type       *mergeWith(Type *other) { assert(0); return nullptr; }

	// Acccess functions
	virtual unsigned    getSize() const = 0;
	        unsigned    getBytes() const { return (getSize() + 7) / 8; }
	virtual void        setSize(int sz) { assert(0); }

	// Print and format functions
	// Get the C type, e.g. "unsigned int". If not final, include comment for lack of sign information.
	// When final, choose a signedness etc
	virtual std::string getCtype(bool final = false) const = 0;
	// Print in *i32* format
	        void        starPrint(std::ostream &os) const;
	virtual void        print(std::ostream &) const = 0;
	        std::string prints() const;     // For debugging

	virtual std::string getTempName() const; // Get a temporary name for the type

	// Clear the named type map. This is necessary when testing; the
	// type for the first parameter to 'main' is different for sparc and pentium
	static  void        clearNamedTypes() { namedTypes.clear(); }

	        bool        isPointerToAlpha();

	// For data-flow-based type analysis only: implement the meet operator. Set ch true if any change
	// If bHighestPtr is true, then if this and other are non void* pointers, set the result to the
	// *highest* possible type compatible with both (i.e. this JOIN other)
	virtual Type       *meetWith(Type *other, bool &ch, bool bHighestPtr = false) = 0;
	// When all=false (default), return true if can use this and other interchangeably; in particular,
	// if at most one of the types is compound and the first element is compatible with the other, then
	// the types are considered compatible. With all set to true, if one or both types is compound, all
	// corresponding elements must be compatible
	virtual bool        isCompatibleWith(Type *other, bool all = false);
	// isCompatible does most of the work; isCompatibleWith looks for complex types in other, and if so
	// reverses the parameters (this and other) to prevent many tedious repetitions
	virtual bool        isCompatible(Type *other, bool all) = 0;
	// Return true if this is a subset or equal to other
	        bool        isSubTypeOrEqual(Type *other);
	// Create a union of this Type and other. Set ch true if any change
	        Type       *createUnion(Type *other, bool &ch, bool bHighestPtr = false);
	static  Type       *newIntegerLikeType(int size, int signedness);   // Return a new Bool/Char/Int
	// From a complex type like an array of structs with a float, return a list of components so you
	// can construct e.g. myarray1[8].mystruct2.myfloat7
	        ComplexTypeCompList &compForAddress(ADDRESS addr, DataIntervalMap &dim);
	// Dereference this type. For most cases, return null unless you are a pointer type. But for a
	// union of pointers, return a new union with the dereference of all members. In dfa.cpp
	        Type       *dereference();
};

class VoidType : public Type {
	friend class XMLProgParser;

public:
	            VoidType();
	virtual    ~VoidType();
	bool        isVoid() const override { return true; }

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

class FuncType : public Type {
	friend class XMLProgParser;

	Signature  *signature;

public:
	            FuncType(Signature *sig = nullptr);
	virtual    ~FuncType();
	bool        isFunc() const override { return true; }

	Type       *clone() const override;

	Signature  *getSignature() const { return signature; }
	void        setSignature(Signature *sig) { signature = sig; }

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	// Split the C type into return and parameter parts
	std::string getReturn(bool final = false) const;
	std::string getParam(bool final = false) const;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

class IntegerType : public Type {
	friend class XMLProgParser;

	unsigned    size;           // Size in bits, e.g. 16
	int         signedness;     // pos=signed, neg=unsigned, 0=unknown or evenly matched

public:
	            IntegerType(int sz = STD_SIZE, int sign = 0);
	virtual    ~IntegerType();
	bool        isInteger() const override { return true; }
	bool        isComplete() const override { return signedness != 0 && size != 0; }

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;
	Type       *mergeWith(Type *other) override;

	unsigned    getSize() const override;            // Get size in bits
	void        setSize(int sz) override { size = sz; }
	// Is it signed? 0=unknown, pos=yes, neg = no
	bool        isSigned() const   { return signedness >= 0; }  // True if not unsigned
	bool        isUnsigned() const { return signedness <= 0; }  // True if not definitely signed
	// A hint for signedness
	void        bumpSigned(int sg) { signedness += sg; }
	// Set absolute signedness
	void        setSigned(int sg)  { signedness = sg; }
	// Get the signedness
	int         getSignedness() const { return signedness; }

	// Get the C type as a string. If full, output comments re the lack of sign information (in IntegerTypes).
	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	std::string getTempName() const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

class FloatType : public Type {
	friend class XMLProgParser;

	unsigned    size;               // Size in bits, e.g. 64

public:
	            FloatType(int sz = 64);
	virtual    ~FloatType();
	bool        isFloat() const override { return true; }

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;
	void        setSize(int sz) override { size = sz; }

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	std::string getTempName() const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

class BooleanType : public Type {
	friend class XMLProgParser;

public:
	            BooleanType();
	virtual    ~BooleanType();
	bool        isBoolean() const override { return true; }

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

class CharType : public Type {
	friend class XMLProgParser;

public:
	            CharType();
	virtual    ~CharType();
	bool        isChar() const override { return true; }

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

class PointerType : public Type {
	friend class XMLProgParser;

	Type        *points_to;

public:
	            PointerType(Type *p);
	virtual    ~PointerType();
	bool        isPointer() const override { return true; }
	void        setPointsTo(Type *p);
	Type       *getPointsTo() const { return points_to; }
	static PointerType *newPtrAlpha();
	bool        pointsToAlpha();
	int         pointerDepth();     // Return 2 for **x
	Type       *getFinalPointsTo(); // Return x for **x

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;
	Exp        *match(Type *pattern) override;

	unsigned    getSize() const override;
	void        setSize(int sz) override { assert(sz == STD_SIZE); }

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

class ArrayType : public Type {
	friend class XMLProgParser;

	Type       *base_type = nullptr;
	unsigned    length = 0;

protected:
	            ArrayType() : Type(eArray) { }
public:
	            ArrayType(Type *p, unsigned length);
	            ArrayType(Type *p);
	virtual    ~ArrayType();
	bool        isArray() const override { return true; }
	Type       *getBaseType() const { return base_type; }
	void        setBaseType(Type *b);
	void        fixBaseType(Type *b);
	unsigned    getLength() const { return length; }
	void        setLength(unsigned n) { length = n; }
	bool        isUnbounded() const;

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;
	Exp        *match(Type *pattern) override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatibleWith(Type *other, bool all = false) override { return isCompatible(other, all); }
	bool        isCompatible(Type *other, bool all) override;
};

class NamedType : public Type {
	friend class XMLProgParser;

	std::string name;
	static int  nextAlpha;

public:
	            NamedType(const std::string &name);
	virtual    ~NamedType();
	bool        isNamed() const override { return true; }
	const std::string &getName() const { return name; }
	Type       *resolvesTo() const;
	// Get a new type variable, e.g. alpha0, alpha55
	static NamedType *getAlpha();

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

struct CompoundElement {
	Type       *type;
	std::string name;
};

/**
 * The compound type represents structures, not unions.
 */
class CompoundType : public Type {
	friend class XMLProgParser;

	std::vector<CompoundElement> elems;
	int         nextGenericMemberNum = 1;
	bool        generic;

public:
	            CompoundType(bool generic = false);
	virtual    ~CompoundType();
	bool        isCompound() const override { return true; }

	void        addType(Type *, const std::string &);
	unsigned    getNumTypes() const { return elems.size(); }
	Type       *getType(unsigned n) const { assert(n < elems.size()); return elems[n].type; }
	Type       *getType(const std::string &) const;
	const char *getName(unsigned n) const { assert(n < elems.size()); return elems[n].name.c_str(); }
	void        setTypeAtOffset(unsigned n, Type *ty);
	Type       *getTypeAtOffset(unsigned n) const;
	void        setNameAtOffset(unsigned n, const std::string &);
	const char *getNameAtOffset(unsigned n) const;
	bool        isGeneric() const { return generic; }
	void        updateGenericMember(int off, Type *ty, bool &ch);   // Add a new generic member if necessary
	unsigned    getOffsetTo(unsigned n) const;
	unsigned    getOffsetTo(const std::string &) const;
	unsigned    getOffsetRemainder(unsigned n) const;

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	bool        isSuperStructOf(const Type &other) const;  // True if this is a superstructure of other
	bool        isSubStructOf(const Type &other) const;    // True if this is a substructure of other

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatibleWith(Type *other, bool all = false) override { return isCompatible(other, all); }
	bool        isCompatible(Type *other, bool all) override;
};

struct UnionElement {
	Type       *type;
	std::string name;
};

/**
 * The union type represents the union of any number of any other types.
 */
class UnionType : public Type {
	friend class XMLProgParser;

	// Note: list, not vector, as it is occasionally desirable to insert elements without affecting iterators
	// (e.g. meetWith(another Union))
	std::list<UnionElement> elems;

public:
	            UnionType();
	virtual    ~UnionType();
	bool        isUnion() const override { return true; }

	void        addType(Type *, const std::string &);
	unsigned    getNumTypes() const { return elems.size(); }
	bool        findType(Type *ty) const;  // Return true if ty is already in the union
	//Type       *getType(unsigned n) const { assert(n < elems.size()); return elems[n].type; }
	//Type       *getType(const std::string &) const;
	//const char *getName(unsigned n) const { assert(n < elems.size()); return elems[n].name.c_str(); }

	Type       *clone() const override;

	bool        operator ==(const Type &other) const override;
	//bool        operator -=(const Type &other) const override;
	bool        operator < (const Type &other) const override;

	unsigned    getSize() const override;

	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;

	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatibleWith(Type *other, bool all) override { return isCompatible(other, all); }
	bool        isCompatible(Type *other, bool all) override;
	// if this is a union of pointer types, get the union of things they point to. In dfa.cpp
	Type       *dereferenceUnion() const;
};

/**
 * This class is for before type analysis.  Typically, you have no info at all,
 * or only know the size (e.g. width of a register or memory transfer).
 */
class SizeType : public Type {
	friend class XMLProgParser;

	unsigned    size;               // Size in bits, e.g. 16

public:
	            SizeType() : Type(eSize) { }
	            SizeType(unsigned sz) : Type(eSize), size(sz) { }
	virtual    ~SizeType() { }
	Type       *clone() const override;
	bool        operator ==(const Type &other) const override;
	bool        operator < (const Type &other) const override;
	Type       *mergeWith(Type *other) override;

	unsigned    getSize() const override;
	void        setSize(unsigned sz) /* override */ { size = sz; }  // FIXME: different parameter type
	bool        isSize() const override { return true; }
	bool        isComplete() const override { return false; }    // Basic type is unknown
	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;
	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

/**
 * This class represents the upper half of its base type.
 * Mainly needed to represent the upper and lower half for type double.
 */
class UpperType : public Type {
	Type       *base_type;

public:
	            UpperType(Type *base) : Type(eUpper), base_type(base) { }
	virtual    ~UpperType() { }
	Type       *clone() const override;
	bool        operator ==(const Type &other) const override;
	bool        operator < (const Type &other) const override;
	Type       *mergeWith(Type *other) override;
	Type       *getBaseType() const { return base_type; }
	void        setBaseType(Type *b) { base_type = b; }

	unsigned    getSize() const override { return base_type->getSize() / 2; }
	void        setSize(int sz) override;        // Does this make sense?
	bool        isUpper() const override { return true; }
	bool        isComplete() const override { return base_type->isComplete(); }
	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;
	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};

/**
 * As above, but stores the lower half.
 */
class LowerType : public Type {
	Type       *base_type;

public:
	            LowerType(Type *base) : Type(eLower), base_type(base) { }
	virtual    ~LowerType() { }
	Type       *clone() const override;
	bool        operator ==(const Type &other) const override;
	bool        operator < (const Type &other) const override;
	Type       *mergeWith(Type *other) override;
	Type       *getBaseType() const { return base_type; }
	void        setBaseType(Type *b) { base_type = b; }

	unsigned    getSize() const override { return base_type->getSize() / 2; }
	void        setSize(int sz) override;        // Does this make sense?
	bool        isLower() const override { return true; }
	bool        isComplete() const override { return base_type->isComplete(); }
	std::string getCtype(bool final = false) const override;
	void        print(std::ostream &) const override;
	Type       *meetWith(Type *other, bool &ch, bool bHighestPtr) override;
	bool        isCompatible(Type *other, bool all) override;
};


/**
 * This class is used to represent local variables in procedures, and the
 * global variables for the program.  The concept is that the data space (the
 * current procedure's stack or the global data space) has to be partitioned
 * into separate variables of various sizes and types.  If a new variable is
 * inserted that would cause an overlap, the types have to be reconciled such
 * that they no longer conflict (generally, the smaller type becomes a member
 * of the larger type, which has to be a structure or an array).
 *
 * Each procedure and the Prog object have a map from ADDRESS (stack offset
 * from sp{0} for locals, or native address for globals), to an object of this
 * class.  A multimap is not needed, as the type of the entry specifies the
 * overlapping.
 */
struct DataInterval {
	unsigned    size;               // The size of this type in bytes
	std::string name;               // The name of the variable
	Type       *type;               // The type of the variable
};

typedef std::pair<const ADDRESS, DataInterval> DataIntervalEntry;       // For result of find() below

class DataIntervalMap {
	std::map<ADDRESS, DataInterval> dimap;
	UserProc   *proc;                             // If used for locals, has ptr to UserProc, else nullptr
public:
	            DataIntervalMap() { }
	typedef std::map<ADDRESS, DataInterval>::iterator iterator;
	void        setProc(UserProc *p) { proc = p; }// Initialise the proc pointer
	DataIntervalEntry *find(ADDRESS addr);        // Find the DataInterval at address addr, or nullptr if none
	iterator    find_it(ADDRESS addr);            // Return an iterator to the entry for it, or end() if none
	bool        isClear(ADDRESS addr, unsigned size);       // True if from addr for size bytes is clear
	// Add a new data item
	void        addItem(ADDRESS addr, const char *name, Type *ty, bool forced = false);
	void        deleteItem(ADDRESS addr);       // Mainly for testing?
	void        expandItem(ADDRESS addr, unsigned size);
	std::string prints() const;                 // For test and debug

private:
	void        enterComponent(DataIntervalEntry *pdie, ADDRESS addr, const char *name, Type *ty, bool forced);
	void        replaceComponents(ADDRESS addr, const char *name, Type *ty, bool forced);
	void        checkMatching(DataIntervalEntry *pdie, ADDRESS addr, const char *name, Type *ty, bool forced);
};

// Not part of the Type class, but logically belongs with it:
std::ostream &operator <<(std::ostream &, const Type *);
std::ostream &operator <<(std::ostream &, const Type &);

#endif
