/**
 * \file
 * \ingroup UnitTestStub
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "type.h"

Type::Type() { }
Type::~Type() { }
Type *Type::getNamedType(const std::string &name) { return nullptr; }
bool Type::operator !=(const Type &other) const { return false; }
std::string Type::getTempName() const { return ""; }
void Type::addNamedType(const std::string &name, Type *type) { }

BooleanType::BooleanType() { }
BooleanType::~BooleanType() { }
Type *BooleanType::clone() const { return nullptr; }
bool BooleanType::operator ==(const Type &other) const { return false; }
bool BooleanType::operator < (const Type &other) const { return false; }
int BooleanType::getSize() const { return 0; }
std::string BooleanType::getCtype() const { return ""; }

VoidType::VoidType() { }
VoidType::~VoidType() { }
Type *VoidType::clone() const { return nullptr; }
bool VoidType::operator ==(const Type &other) const { return false; }
bool VoidType::operator < (const Type &other) const { return false; }
int VoidType::getSize() const { return 0; }
std::string VoidType::getCtype() const { return ""; }

IntegerType::IntegerType(int sz, bool sign) { }
IntegerType::~IntegerType() { }
Type *IntegerType::clone() const { return nullptr; }
bool IntegerType::operator ==(const Type &other) const { return false; }
bool IntegerType::operator < (const Type &other) const { return false; }
int IntegerType::getSize() const { return 0; }
std::string IntegerType::getCtype() const { return ""; }
std::string IntegerType::getTempName() const { return ""; }

CharType::CharType() { }
CharType::~CharType() { }
Type *CharType::clone() const { return nullptr; }
bool CharType::operator ==(const Type &other) const { return false; }
bool CharType::operator < (const Type &other) const { return false; }
int CharType::getSize() const { return 0; }
std::string CharType::getCtype() const { return ""; }

FloatType::FloatType(int i) { }
FloatType::~FloatType() { }
Type *FloatType::clone() const { return nullptr; }
bool FloatType::operator ==(const Type &other) const { return false; }
bool FloatType::operator < (const Type &other) const { return false; }
int FloatType::getSize() const { return 0; }
std::string FloatType::getCtype() const { return ""; }
std::string FloatType::getTempName() const { return ""; }

PointerType::PointerType(Type *t) { }
PointerType::~PointerType() { }
Type *PointerType::clone() const { return nullptr; }
bool PointerType::operator ==(const Type &other) const { return false; }
bool PointerType::operator < (const Type &other) const { return false; }
int PointerType::getSize() const { return 0; }
std::string PointerType::getCtype() const { return ""; }

FuncType::FuncType(Signature *) { }
FuncType::~FuncType() { }
Type *FuncType::clone() const { return nullptr; }
bool FuncType::operator ==(const Type &other) const { return false; }
bool FuncType::operator < (const Type &other) const { return false; }
int FuncType::getSize() const { return 0; }
std::string FuncType::getCtype() const { return ""; }

NamedType::NamedType(const std::string &name) { }
NamedType::~NamedType() { }
Type *NamedType::clone() const { return nullptr; }
bool NamedType::operator ==(const Type &other) const { return false; }
bool NamedType::operator < (const Type &other) const { return false; }
int NamedType::getSize() const { return 0; }
std::string NamedType::getCtype() const { return ""; }
