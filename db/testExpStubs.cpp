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
#include "types.h"
#include "cfg.h"
#include "proc.h"
#include "prog.h"
#include "signature.h"
#include "boomerang.h"

#include <iostream>
#include <string>

class Prog;

// util
#include "utilStubs.cpp"

// basicblock
void BasicBlock::getReachInAt(Statement *stmt, StatementSet &reachin, int phase) { }
void BasicBlock::getAvailInAt(Statement *stmt, StatementSet &reachin, int phase) { }

// type
#include "typeStubs.cpp"

// Proc
Signature *Proc::getSignature() { return nullptr; }
Cfg *UserProc::getCFG() { return nullptr; }
const char *Proc::getName() { return ""; }
Prog *Proc::getProg() { return nullptr; }
void UserProc::getReturnSet(LocationSet &ret) { }

// Prog
const char *Prog::getStringConstant(ADDRESS uaddr) { return nullptr; }
Proc *Prog::findProc(ADDRESS uAddr) const { return nullptr; }
void Prog::analyse() { }

// signature
std::list<Exp *> *Signature::getCallerSave(Prog *prog) { return nullptr; }

// frontend
void FrontEnd::decode(Prog *prog, ADDRESS a) { }

//Misc
Boomerang::Boomerang() { }
Boomerang *Boomerang::boomerang = nullptr;
