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

#include "signature.h"

std::list<Exp *> *Signature::getCallerSave(Prog *prog) { return nullptr; }
Signature::Signature(const char *nam) { }
bool Signature::operator ==(const Signature &other) const { return false; }
Signature *Signature::clone() { return nullptr; }
Exp *Signature::getReturnExp() { return nullptr; }
Type *Signature::getReturnType() { return nullptr; }
void Signature::setReturnType(Type *t) { }
const char *Signature::getName() { return nullptr; }
void Signature::setName(const char *nam) { }
void Signature::addParameter(const char *nam) { }
void Signature::addParameter(Type *type, const char *nam, Exp *e) { }
void Signature::addParameter(Exp *e) { }
void Signature::setNumParams(int n) { }
int Signature::getNumParams() { return 0; }
Exp *Signature::getParamExp(int n) { return nullptr; }
Type *Signature::getParamType(int n) { return nullptr; }
Exp *Signature::getArgumentExp(int n) { return nullptr; }
const char *Signature::getParamName(int n) { return nullptr; }
//void Signature::analyse(UserProc *p) { }
Signature *Signature::promote(UserProc *p) { return nullptr; }
void Signature::getInternalStatements(StatementList &stmts) { }
void Signature::print(std::ostream &out) { }
int Signature::getStackRegister(Prog *prog) { return 0; }
Signature *Signature::instantiate(const char *str, const char *nam) { return nullptr; }
