/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the CfgTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "CfgTest.h"

#include "cfg.h"
#include "dataflow.h"
#include "frontend.h"
#include "proc.h"
#include "prog.h"

#include <sstream>
#include <string>

#define FRONTIER_PENTIUM        "test/pentium/frontier"
#define SEMI_PENTIUM            "test/pentium/semi"
#define IFTHEN_PENTIUM          "test/pentium/ifthen"

/**
 * Test the dominator frontier code.
 */
void
CfgTest::testDominators()
{
#define FRONTIER_FOUR     0x08048347
#define FRONTIER_FIVE     0x08048351
#define FRONTIER_TWELVE   0x080483b2
#define FRONTIER_THIRTEEN 0x080483b9

	auto prog = Prog::open(FRONTIER_PENTIUM);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	Type::clearNamedTypes();
	fe->decode();

	bool gotMain;
	ADDRESS addr = fe->getMainEntryPoint(gotMain);
	CPPUNIT_ASSERT(addr != NO_ADDRESS);

	std::list<Proc *>::iterator it;
	auto pProc = prog->getFirstUserProc(it);
	auto cfg = pProc->getCFG();
	DataFlow *df = pProc->getDataFlow();
	df->dominators(cfg);

	// Find BB "5" (as per Appel, Figure 19.5).
	BasicBlock *bb = nullptr;
	for (const auto &b : *cfg) {
		if (b->getLowAddr() == FRONTIER_FIVE) {
			bb = b;
			break;
		}
	}
	CPPUNIT_ASSERT(bb);

	std::ostringstream expected, actual;
#if 0
	expected << std::hex
	         << FRONTIER_FIVE << " "
	         << FRONTIER_THIRTEEN << " "
	         << FRONTIER_TWELVE << " "
	         << FRONTIER_FOUR << " "
	         << std::dec;
#endif
	expected << std::hex
	         << FRONTIER_THIRTEEN << " "
	         << FRONTIER_FOUR << " "
	         << FRONTIER_TWELVE << " "
	         << FRONTIER_FIVE << " "
	         << std::dec;
	int n5 = df->pbbToNode(bb);
	std::set<int> &DFset = df->getDF(n5);
	for (const auto &ii : DFset)
		actual << std::hex << (unsigned)df->nodeToBB(ii)->getLowAddr() << std::dec << " ";
	CPPUNIT_ASSERT_EQUAL(expected.str(), actual.str());
	delete prog;
}

/**
 * Test a case where semi dominators are different to dominators.
 */
void
CfgTest::testSemiDominators()
{
#define SEMI_L  0x80483b0
#define SEMI_M  0x80483e2
#define SEMI_B  0x8048345
#define SEMI_D  0x8048354
#define SEMI_M  0x80483e2

	auto prog = Prog::open(SEMI_PENTIUM);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	Type::clearNamedTypes();
	fe->decode();

	bool gotMain;
	ADDRESS addr = fe->getMainEntryPoint(gotMain);
	CPPUNIT_ASSERT(addr != NO_ADDRESS);

	std::list<Proc *>::iterator it;
	auto pProc = prog->getFirstUserProc(it);
	auto cfg = pProc->getCFG();

	DataFlow *df = pProc->getDataFlow();
	df->dominators(cfg);

	// Find BB "L (6)" (as per Appel, Figure 19.8).
	BasicBlock *bb = nullptr;
	for (const auto &b : *cfg) {
		if (b->getLowAddr() == SEMI_L) {
			bb = b;
			break;
		}
	}
	CPPUNIT_ASSERT(bb);
	int nL = df->pbbToNode(bb);

	// The dominator for L should be B, where the semi dominator is D
	// (book says F)
	unsigned actual_dom  = (unsigned)df->nodeToBB(df->getIdom(nL))->getLowAddr();
	unsigned actual_semi = (unsigned)df->nodeToBB(df->getSemi(nL))->getLowAddr();
	CPPUNIT_ASSERT_EQUAL((unsigned)SEMI_B, actual_dom);
	CPPUNIT_ASSERT_EQUAL((unsigned)SEMI_D, actual_semi);
	// Check the final dominator frontier as well; should be M and B
	std::ostringstream expected, actual;
#if 0
	expected << std::hex << SEMI_M << " " << SEMI_B << " " << std::dec;
#endif
	expected << std::hex << SEMI_B << " " << SEMI_M << " " << std::dec;
	std::set<int> &DFset = df->getDF(nL);
	for (const auto &ii : DFset)
		actual << std::hex << (unsigned)df->nodeToBB(ii)->getLowAddr() << std::dec << " ";
	CPPUNIT_ASSERT_EQUAL(expected.str(), actual.str());
	delete prog;
}

/**
 * Test the placing of phi functions.
 */
void
CfgTest::testPlacePhi()
{
	auto prog = Prog::open(FRONTIER_PENTIUM);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	Type::clearNamedTypes();
	fe->decode();

	std::list<Proc *>::iterator it;
	auto pProc = prog->getFirstUserProc(it);
	auto cfg = pProc->getCFG();

	// Simplify expressions (e.g. m[ebp + -8] -> m[ebp - 8]
	prog->finishDecode();

	DataFlow *df = pProc->getDataFlow();
	df->dominators(cfg);
	df->placePhiFunctions(pProc);

	// m[r29 - 8] (x for this program)
	Exp *e = Location::memOf(
	    new Binary(opMinus,
	        Location::regOf(29),
	        new Const(4)));

	// A_phi[x] should be the set {7 8 10 15 20 21} (all the join points)
	std::ostringstream ost;
	std::set<int> &A_phi = df->getA_phi(e);
	for (const auto &ii : A_phi)
		ost << ii << " ";
	std::string expected("7 8 10 15 20 21 ");
	CPPUNIT_ASSERT_EQUAL(expected, ost.str());
	delete prog;
}

/**
 * Test a case where a phi function is not needed.
 */
void
CfgTest::testPlacePhi2()
{
	auto prog = Prog::open(IFTHEN_PENTIUM);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	Type::clearNamedTypes();
	fe->decode();

	std::list<Proc *>::iterator it;
	auto pProc = prog->getFirstUserProc(it);
	auto cfg = pProc->getCFG();
	DataFlow *df = pProc->getDataFlow();

	// Simplify expressions (e.g. m[ebp + -8] -> m[ebp - 8]
	prog->finishDecode();

	df->dominators(cfg);
	df->placePhiFunctions(pProc);

	// In this program, x is allocated at [ebp-4], a at [ebp-8], and
	// b at [ebp-12]
	// We check that A_phi[ m[ebp-8] ] is 4, and that
	// A_phi A_phi[ m[ebp-8] ] is null
	// (block 4 comes out with n=4)

	std::string expected = "4 ";
	std::ostringstream actual;
	// m[r29 - 8]
	Exp *e = Location::memOf(
	    new Binary(opMinus,
	        Location::regOf(29),
	        new Const(8)));
	std::set<int> &s = df->getA_phi(e);
	for (const auto &pp : s)
		actual << pp << " ";
	CPPUNIT_ASSERT_EQUAL(expected, actual.str());
	delete e;

	expected = "";
	std::ostringstream actual2;
	// m[r29 - 12]
	e = Location::memOf(
	    new Binary(opMinus,
	        Location::regOf(29),
	        new Const(12)));

	std::set<int> &s2 = df->getA_phi(e);
	for (const auto &pp : s2)
		actual2 << pp << " ";
	CPPUNIT_ASSERT_EQUAL(expected, actual2.str());
	delete e;
	delete prog;
}

/**
 * Test the renaming of variables.
 */
void
CfgTest::testRenameVars()
{
	auto prog = Prog::open(FRONTIER_PENTIUM);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	Type::clearNamedTypes();
	fe->decode();

	std::list<Proc *>::iterator it;
	auto pProc = prog->getFirstUserProc(it);
	auto cfg = pProc->getCFG();
	DataFlow *df = pProc->getDataFlow();

	// Simplify expressions (e.g. m[ebp + -8] -> m[ebp - 8]
	prog->finishDecode();

	df->dominators(cfg);
	df->placePhiFunctions(pProc);
	pProc->numberStatements();         // After placing phi functions!
	df->renameBlockVars(pProc, 0, 1);  // Block 0, mem depth 1

	// MIKE: something missing here?

	delete prog;
}
