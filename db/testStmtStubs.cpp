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
#include "pentiumfrontend.h"

#include <iostream>
#include <string>

class Prog;

// util
#include "utilStubs.cpp"

// basicblock
void BasicBlock::setOutEdge(int i, BasicBlock *pNewOutEdge) { }
void BasicBlock::addInEdge(BasicBlock *pNewInEdge) { }

// type
#include "typeStubs.cpp"

// Prog
Prog::Prog() { }
Prog::~Prog() { }
Prog::Prog(BinaryFile *pBF, FrontEnd *pFE) { }
const char *Prog::getStringConstant(ADDRESS uaddr) { return nullptr; }
Proc *Prog::findProc(ADDRESS uAddr) const { return nullptr; }
void Prog::analyse() { }
void Prog::decompile() { }
void Prog::toSSAform() { }
void Prog::initStatements() { }
UserProc *Prog::getFirstUserProc(std::list<Proc *>::iterator &it) { return nullptr; }
UserProc *Prog::getNextUserProc(std::list<Proc *>::iterator &it) { return nullptr; }

// frontend
void FrontEnd::decode(Prog *prog, ADDRESS a) { }
FrontEnd::FrontEnd(BinaryFile *pBF) { }
PentiumFrontEnd::PentiumFrontEnd(BinaryFile *pBF) : FrontEnd(pBF) { }
PentiumFrontEnd::~PentiumFrontEnd() { }
FrontEnd::~FrontEnd() { }
bool PentiumFrontEnd::processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool spec /* = false */, PHELPER helperFunc /* = nullptr */) { return false; }
ADDRESS PentiumFrontEnd::getMainEntryPoint(bool &gotMain) { return 0; }
Prog *FrontEnd::decode() { return nullptr; }
bool FrontEnd::processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool spec /* = false */, PHELPER helperFunc) { return false; }

// cfg
BasicBlock *Cfg::newBB(std::list<RTL *> *pRtls, BBTYPE bbType, int iNumOutEdges) { return nullptr; }
void Cfg::print(std::ostream &out, bool withDF) { }
void Cfg::setEntryBB(BasicBlock *bb) { }

//Misc
Boomerang::Boomerang() { }
Boomerang *Boomerang::boomerang = nullptr;
