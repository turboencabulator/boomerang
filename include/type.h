/**
 * \file
 * \brief Definition of the Type class.
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
#include <vector>

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
//class UpperType;
//class LowerType;

class DataIntervalMap;
class Exp;
class Signature;
class UserProc;

/**
 * Low-level type information.
 *
 * Note that we may have a completely different system for recording
 * high-level types.
 */
class Type {
	friend class XMLProgParser;

protected:
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
		//eUpper,
		//eLower,
	} id;  ///< For operator < mostly

private:
	static  std::map<std::string, Type *> namedTypes;

public:
	                    Type(eType);
	virtual            ~Type();

	/**
	 * \name Runtime type information
	 * Deprecated for most situations; use resolvesToTYPE().
	 * \{
	 */
	        eType       getId()         const { return id; }
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
	//virtual bool        isUpper()       const { return false; }
	//virtual bool        isLower()       const { return false; }
	/** \} */

	virtual bool        isComplete() const;
	        bool        isCString();
	        bool        isPointerToAlpha();

	/**
	 * \name These replace type casts
	 * \{
	 */
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
	//        UpperType    *asUpper();
	//        LowerType    *asLower();
	/** \} */

	/**
	 * \name These replace calls to isNamed() and NamedType::resolvesTo()
	 * \{
	 */
	        Type *      resolvesTo();
	        const Type *resolvesTo() const;
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
	//        bool        resolvesToUpper() const;
	//        bool        resolvesToLower() const;
	/** \} */

	/**
	 * \name Comparisons
	 * \{
	 */
	virtual bool        operator ==(const Type &) const = 0;
	virtual bool        operator !=(const Type &) const;
	//virtual bool        operator -=(const Type &) const = 0;
	virtual bool        operator < (const Type &) const = 0;
	        bool        operator *=(const Type &) const;
	        bool        isSubTypeOrEqual(const Type &);
	virtual bool        isCompatibleWith(const Type &, bool = false) const;
protected:
	virtual bool        isCompatible(const Type &, bool) const = 0;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	        void        starPrint(std::ostream &) const;
	virtual void        print(std::ostream &) const = 0;
	        std::string prints() const;
	virtual std::string getCtype(bool = false) const = 0;

	virtual std::string getTempName() const;
	static  Type       *getTempType(const std::string &);
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	        unsigned    getBytes() const;
	virtual unsigned    getSize() const = 0;
	virtual void        setSize(unsigned);
	/** \} */

	/**
	 * \name Named type accessors
	 * \{
	 */
	static  void        addNamedType(const std::string &, Type *);
	static  Type       *getNamedType(const std::string &);
	static  void        clearNamedTypes();
	/** \} */

	virtual Exp        *match(Type *);
	virtual Type       *clone() const = 0;
	virtual Type       *mergeWith(Type *);
	virtual Type       *meetWith(Type *, bool &, bool = false) = 0;
	        Type       *createUnion(Type *, bool &, bool = false);
	static  Type       *newIntegerLikeType(unsigned, int);
	        Type       *dereference();

	struct ComplexTypeComp {
		bool isArray;
		union {
			const char *memberName;  ///< Member name if offset
			unsigned index;          ///< Constant index if array
		} u;
	};
	typedef std::list<ComplexTypeComp> ComplexTypeCompList;
	        ComplexTypeCompList &compForAddress(ADDRESS, DataIntervalMap &);
};

/**
 * \brief No type information?
 */
class VoidType : public Type {
	friend class XMLProgParser;

public:
	            VoidType();
	virtual    ~VoidType();
	bool        isVoid() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * \brief Represents a function.
 */
class FuncType : public Type {
	friend class XMLProgParser;

	Signature  *signature;

public:
	            FuncType(Signature * = nullptr);
	virtual    ~FuncType();
	bool        isFunc() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	// Split the C type into return and parameter parts
	std::string getReturn(bool = false) const;
	std::string getParam(bool = false) const;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;

	Signature  *getSignature() const;
	void        setSignature(Signature *);
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * \brief Represents an integer.
 */
class IntegerType : public Type {
	friend class XMLProgParser;

	unsigned    size;        ///< Size in bits, e.g. 16
	int         signedness;  ///< pos=signed, neg=unsigned, 0=unknown or evenly matched

public:
	            IntegerType(unsigned = STD_SIZE, int = 0);
	virtual    ~IntegerType();
	bool        isInteger() const override { return true; }
	bool        isComplete() const override;

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	std::string getTempName() const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	void        setSize(unsigned) override;

	bool        isSigned() const;
	bool        isUnsigned() const;
	void        bumpSigned(int);
	void        setSigned(int);
	int         getSignedness() const;
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
	Type       *mergeWith(Type *) override;
};

/**
 * \brief Represents a float.
 */
class FloatType : public Type {
	friend class XMLProgParser;

	unsigned    size;  ///< Size in bits, e.g. 64

public:
	            FloatType(unsigned = 64);
	virtual    ~FloatType();
	bool        isFloat() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	std::string getTempName() const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	void        setSize(unsigned) override;
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * \brief Represents a bool.
 */
class BooleanType : public Type {
	friend class XMLProgParser;

public:
	            BooleanType();
	virtual    ~BooleanType();
	bool        isBoolean() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * \brief Represents a string character.
 */
class CharType : public Type {
	friend class XMLProgParser;

public:
	            CharType();
	virtual    ~CharType();
	bool        isChar() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * \brief Represents a pointer to another Type.
 */
class PointerType : public Type {
	friend class XMLProgParser;

	Type        *points_to;

public:
	            PointerType(Type *);
	virtual    ~PointerType();
	bool        isPointer() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	void        setSize(unsigned) override;

	void        setPointsTo(Type *);
	Type       *getPointsTo() const;
	Type       *getFinalPointsTo() const;
	int         pointerDepth() const;
	/** \} */

	static PointerType *newPtrAlpha();
	bool        pointsToAlpha() const;

	Exp        *match(Type *) override;
	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * \brief Represents an array of another Type.
 */
class ArrayType : public Type {
	friend class XMLProgParser;

	Type       *base_type = nullptr;
	unsigned    length = 0;
	static const unsigned NO_BOUND;

protected:
	            ArrayType();
public:
	            ArrayType(Type *, unsigned);
	            ArrayType(Type *);
	virtual    ~ArrayType();
	bool        isArray() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
	bool        isCompatibleWith(const Type &, bool = false) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;

	Type       *getBaseType() const;
	void        setBaseType(Type *);
	void        fixBaseType(Type *);

	unsigned    getLength() const;
	void        setLength(unsigned);
	bool        isUnbounded() const;
	/** \} */

	Exp        *match(Type *) override;
	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * \brief Represents a Type in the namedTypes map.
 */
class NamedType : public Type {
	friend class XMLProgParser;

	std::string name;
	static int  nextAlpha;

public:
	            NamedType(const std::string &name);
	virtual    ~NamedType();
	bool        isNamed() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;

	const std::string &getName() const;
	Type       *resolvesTo() const;
	/** \} */

	static NamedType *getAlpha();

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * The compound type represents structures, not unions.
 */
class CompoundType : public Type {
	friend class XMLProgParser;

public:
	struct Element {
		Type       *type;
		std::string name;
	};
	typedef std::vector<Element>::const_iterator const_iterator;

private:
	std::vector<Element> elems;
	int         nextGenericMemberNum = 1;
	bool        generic;

public:
	            CompoundType(bool = false);
	virtual    ~CompoundType();
	bool        isCompound() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
	bool        isCompatibleWith(const Type &, bool = false) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	bool        isSubStructOf(const Type &) const;
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;

	const_iterator cbegin() const;
	const_iterator cend() const;

	void        addType(Type *, const std::string &);
	Type       *getType(const_iterator it) const;
	Type       *getType(const std::string &) const;
	const char *getName(const_iterator it) const;

	unsigned    getOffsetTo(const_iterator) const;
	unsigned    getOffsetTo(const std::string &) const;
	unsigned    getOffsetRemainder(unsigned) const;

	void        setTypeAtOffset(unsigned, Type *);
	Type       *getTypeAtOffset(unsigned) const;
	void        setNameAtOffset(unsigned, const std::string &);
	const char *getNameAtOffset(unsigned) const;

	bool        isGeneric() const;
	void        updateGenericMember(unsigned, Type *, bool &);
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * The union type represents the union of any number of any other types.
 */
class UnionType : public Type {
	friend class XMLProgParser;

public:
	struct Element {
		Type       *type;
		std::string name;
	};

private:
	/**
	 * \note list, not vector, as it is occasionally desirable to insert
	 * elements without affecting iterators (e.g. meetWith(another Union))
	 */
	std::list<Element> elems;

public:
	            UnionType();
	virtual    ~UnionType();
	bool        isUnion() const override { return true; }

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	//bool        operator -=(const Type &) const override;
	bool        operator < (const Type &) const override;
	bool        isCompatibleWith(const Type &, bool = false) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;

	void        addType(Type *, const std::string &);
	bool        findType(Type *) const;
	/** \} */

	Type       *clone() const override;
	Type       *meetWith(Type *, bool &, bool) override;
	Type       *dereferenceUnion() const;
};

/**
 * This class is for before type analysis.  Typically, you have no info at
 * all, or only know the size (e.g. width of a register or memory transfer).
 */
class SizeType : public Type {
	friend class XMLProgParser;

	unsigned    size;  ///< Size in bits, e.g. 16

public:
	            SizeType();
	            SizeType(unsigned);
	virtual    ~SizeType();
	bool        isSize() const override { return true; }
	bool        isComplete() const override;

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	void        setSize(unsigned) override;
	/** \} */

	Type       *clone() const override;
	Type       *mergeWith(Type *) override;
	Type       *meetWith(Type *, bool &, bool) override;

};

#if 0 // Cruft?
/**
 * This class represents the upper half of its base type.
 * Mainly needed to represent the upper and lower half for type double.
 */
class UpperType : public Type {
	Type       *base_type;

public:
	            UpperType(Type *);
	virtual    ~UpperType();
	bool        isUpper() const override { return true; }
	bool        isComplete() const override;

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	//void        setSize(unsigned sz) override;  // Does this make sense?

	Type       *getBaseType() const;
	void        setBaseType(Type *);
	/** \} */

	Type       *clone() const override;
	Type       *mergeWith(Type *) override;
	Type       *meetWith(Type *, bool &, bool) override;
};

/**
 * As above, but stores the lower half.
 */
class LowerType : public Type {
	Type       *base_type;

public:
	            LowerType(Type *);
	virtual    ~LowerType();
	bool        isLower() const override { return true; }
	bool        isComplete() const override;

	/**
	 * \name Comparisons
	 * \{
	 */
	bool        operator ==(const Type &) const override;
	bool        operator < (const Type &) const override;
protected:
	bool        isCompatible(const Type &, bool) const override;
public:
	/** \} */

	/**
	 * \name Print and format methods
	 * \{
	 */
	void        print(std::ostream &) const override;
	std::string getCtype(bool = false) const override;
	/** \} */

	/**
	 * \name Accessors
	 * \{
	 */
	unsigned    getSize() const override;
	//void        setSize(unsigned sz) override;  // Does this make sense?

	Type       *getBaseType() const;
	void        setBaseType(Type *);
	/** \} */

	Type       *clone() const override;
	Type       *mergeWith(Type *) override;
	Type       *meetWith(Type *, bool &, bool) override;
};
#endif


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
class DataIntervalMap {
public:
	struct DataInterval {
		unsigned    size;  ///< The size of this type in bytes.
		std::string name;  ///< The name of the variable.
		Type       *type;  ///< The type of the variable.
	};
	typedef std::map<ADDRESS, DataInterval>::iterator iterator;
	typedef std::map<ADDRESS, DataInterval>::value_type value_type;

private:
	std::map<ADDRESS, DataInterval> dimap;
	UserProc   *proc;  ///< If used for locals, has ptr to UserProc, else null.

public:
	            DataIntervalMap() { }
	void        setProc(UserProc *p) { proc = p; }  ///< Initialise the proc pointer.
	value_type *find(ADDRESS);
	iterator    find_it(ADDRESS);
	bool        isClear(ADDRESS, unsigned);
	void        addItem(ADDRESS, const char *, Type *, bool = false);
	void        deleteItem(ADDRESS);
	//void        expandItem(ADDRESS addr, unsigned size);
	std::string prints() const;

private:
	void        enterComponent(value_type *, ADDRESS, const char *, Type *, bool);
	void        replaceComponents(ADDRESS, const char *, Type *, bool);
	void        checkMatching(value_type *, ADDRESS, const char *, Type *, bool);
};

// Not part of the Type class, but logically belongs with it:
std::ostream &operator <<(std::ostream &, const Type *);
std::ostream &operator <<(std::ostream &, const Type &);

#endif
