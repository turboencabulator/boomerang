/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the ExpTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ExpTest.h"

#include "exp.h"
#include "statement.h"
#include "type.h"
#include "visitor.h"

//#include <fstream>
#include <sstream>
#include <list>
#include <map>
#include <string>
#include <vector>

/**
 * Set up some expressions for use with all the tests.
 *
 * \note Called before any tests.
 */
void
ExpTest::setUp()
{
	m_99 = new Const(99);
	m_rof2 = Location::regOf(2);
}

/**
 * Delete expressions created in setUp.
 *
 * \note Called after all tests.
 */
void
ExpTest::tearDown()
{
	delete m_99;
	delete m_rof2;
}

/**
 * Test integer constant.
 */
void
ExpTest::test99()
{
	CPPUNIT_ASSERT_EQUAL(std::string("99"), m_99->prints());
}

/**
 * Test float constant.
 */
void
ExpTest::testFlt()
{
	auto c = new Const(3.14);
	CPPUNIT_ASSERT_EQUAL(std::string("3.1400"), c->prints());
	delete c;
}

/**
 * Tests r[2], which is used in many tests.  Also tests opRegOf, and
 * ostream::operator &(Exp *).
 *
 * \note r[2] prints as r2, as of June 2003.
 */
void
ExpTest::testRegOf2()
{
	std::ostringstream ost;
	ost << *m_rof2;
	CPPUNIT_ASSERT_EQUAL(std::string("r2"), std::string(ost.str()));
}

/**
 * Test opPlus, opMinus, etc.
 */
void
ExpTest::testBinaries()
{
	auto b = new Binary(opPlus, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 + r2"), b->prints());
	delete b;

	b = new Binary(opMinus, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 - r2"), b->prints());
	delete b;

	b = new Binary(opMult, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 * r2"), b->prints());
	delete b;

	b = new Binary(opDiv, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 / r2"), b->prints());
	delete b;

	b = new Binary(opMults, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 *! r2"), b->prints());
	delete b;

	b = new Binary(opDivs, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 /! r2"), b->prints());
	delete b;

	b = new Binary(opMod, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 % r2"), b->prints());
	delete b;

	b = new Binary(opMods, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("99 %! r2"), b->prints());
	delete b;
}

/**
 * Test LNot, unary minus, etc.
 */
void
ExpTest::testUnaries()
{
	auto u = new Unary(opNot, new Terminal(opZF));
	CPPUNIT_ASSERT_EQUAL(std::string("~%ZF"), u->prints());
	delete u;

	u = new Unary(opLNot, new Terminal(opCF));
	CPPUNIT_ASSERT_EQUAL(std::string("L~%CF"), u->prints());
	delete u;

	u = new Unary(opNeg, m_rof2->clone());
	CPPUNIT_ASSERT_EQUAL(std::string("-r2"), u->prints());
	delete u;
}

/**
 * Test [ a[m[ ] %afp [+|- const].
 */
void
ExpTest::testIsAfpTerm()
{
	Terminal afp(opAFP);
	Binary plus(opPlus, afp.clone(), new Const(-99));
	Binary minus(opMinus, afp.clone(), m_99->clone());
	CPPUNIT_ASSERT(afp.isAfpTerm());
	CPPUNIT_ASSERT(plus.isAfpTerm());
	CPPUNIT_ASSERT(minus.isAfpTerm());
	CPPUNIT_ASSERT(!m_99->isAfpTerm());
	CPPUNIT_ASSERT(!m_rof2->isAfpTerm());
	// Now with typed expressions
	TypedExp tafp(new IntegerType(), afp.clone());
	//TypedExp tafp(afp.clone());
	TypedExp tplus(plus.clone());
	TypedExp tminus(minus.clone());
	CPPUNIT_ASSERT(tafp.isAfpTerm());
	CPPUNIT_ASSERT(tplus.isAfpTerm());
	CPPUNIT_ASSERT(tminus.isAfpTerm());
}

/**
 * \name Test the operator == function.
 * \{
 */
void
ExpTest::testCompare1()
{
	CPPUNIT_ASSERT(!(*m_99 == *m_rof2));
}
void
ExpTest::testCompare2()
{
	Const nineNine(99);
	CPPUNIT_ASSERT(*m_99 == nineNine);
}
void
ExpTest::testCompare3()
{
	Const minus(-99);
	CPPUNIT_ASSERT(!(*m_99 == minus));
}
void
ExpTest::testCompare4()
{
	Location regOf2(opRegOf, new Const(2), nullptr);
	CPPUNIT_ASSERT(regOf2 == *m_rof2);
}
void
ExpTest::testCompare5()
{
	Binary one(opMult, m_99->clone(), m_rof2->clone());
	Binary two(opMult, m_rof2->clone(), m_99->clone());
	CPPUNIT_ASSERT(!(one == two));
}
void
ExpTest::testCompare6()
{
	Binary one(opMult, m_99->clone(), m_rof2->clone());
	Binary two(opMult, m_99->clone(), m_rof2->clone());
	CPPUNIT_ASSERT(one == two);
}
/** \} */

/**
 * \name Test the searchReplace function.
 * \{
 */
void
ExpTest::testSearchReplace1()
{
	// Null test: should not replace. Also tests Ternary class
	Exp *p;
	bool change;
	p = new Ternary(opAt, m_rof2->clone(), new Const(8), new Const(15));
	p = p->searchReplace(m_99, m_rof2, change);
	std::string expected("r2@[8:15]");
	CPPUNIT_ASSERT_EQUAL(expected, p->prints());
	Ternary t2(*(Ternary *)p);
	CPPUNIT_ASSERT(*p == t2);
	p = p->searchReplaceAll(m_99, m_rof2, change);
	CPPUNIT_ASSERT(*p == t2);
	delete p;
}

void
ExpTest::testSearchReplace2()
{
	// Whole expression replacements
	bool change;
	Exp *p1 = new Const(55);
	Const p2(*(Const *)p1);
	Const c2(1234);
	p1 = p1->searchReplace(&p2, &c2, change);
	CPPUNIT_ASSERT(*p1 == c2);
	CPPUNIT_ASSERT(p1 != &c2);  // Pointers should not be same
	p1 = p1->searchReplace(&c2, m_rof2, change);
	CPPUNIT_ASSERT(*p1 == *m_rof2);
	delete p1;
}

void
ExpTest::testSearchReplace3()
{
	// Subexpression replacement
	bool change;
	Const two(2);
	Const three(3);
	Exp *p = m_rof2->clone();
	p = p->searchReplaceAll(&two, &three, change);
	std::string expected("r3");
	CPPUNIT_ASSERT_EQUAL(expected, p->prints());
	delete p;
}

void
ExpTest::testSearchReplace4()
{
	// Subexpression replacement with different subexpression form
	bool change;
	Const two(2);
	Exp *p = m_rof2->clone();
	// Note recursion. OK to use the all function, since it does the search
	// first.
	p = p->searchReplaceAll(&two, m_rof2, change);
	std::string expected("r[r2]");
	CPPUNIT_ASSERT_EQUAL(expected, p->prints());
	delete p;
}
/** \} */

/**
 * \name Test the search function, including wildcards.
 * \{
 */
void
ExpTest::testSearch1()
{
	Const two(2);
	Exp *result;
	// Search inside r2 for const 2
	CPPUNIT_ASSERT(m_rof2->search(&two, result));
	CPPUNIT_ASSERT(*result == two);

	// Test for false positives
	CPPUNIT_ASSERT(!m_99->search(&two, result));
	CPPUNIT_ASSERT(!m_rof2->search(m_99, result));

	// Note: opDiv's enum has value 3
	Binary e(opMult, m_rof2->clone(), m_99->clone());  // r2 / 99
	Const three(3);
	CPPUNIT_ASSERT(!e.search(&three, result));
}

void
ExpTest::testSearch2()
{
	// Search using wildcards
	Binary e(opDivs, m_rof2->clone(), m_99->clone());  // r2 /! 99
	Exp *result;
	Terminal search(opWildRegOf);  // r[?]
	CPPUNIT_ASSERT(e.search(&search, result));
	CPPUNIT_ASSERT(*result == *m_rof2);  // Should be r2

	Const three(3);
	CPPUNIT_ASSERT(!e.search(&three, result));
	CPPUNIT_ASSERT(e.search(m_99, result));
}

void
ExpTest::testSearch3()
{
	// A more complex expression:
	// (r2 * 99) + (m[1000] * 4)
	Exp *result;
	Binary e(opPlus,
	         new Binary(opMult,
	                    m_rof2->clone(),
	                    m_99->clone()),
	         new Binary(opMult,
	                    Location::memOf(new Const(1000)),
	                    new Const(4)));
	Const four(4);
	Location mem1000(opMemOf, new Const(1000), nullptr);
	Binary prod(opMult, m_rof2->clone(), m_99->clone());
	CPPUNIT_ASSERT(e.search(&four,    result));
	CPPUNIT_ASSERT(e.search(&mem1000, result));
	CPPUNIT_ASSERT(e.search(&prod,    result));
	CPPUNIT_ASSERT(e.search(m_99,     result));
	Const three(3);
	CPPUNIT_ASSERT(!e.search(&three,  result));
}

void
ExpTest::testSearchAll()
{
	// A more complex expression:
	// (r2 * 99) + (r8 * 4)
	Terminal search(opWildRegOf);  // r[?]
	std::list<Exp *> result;
	Binary e(opPlus,
	         new Binary(opMult,
	                    m_rof2->clone(),
	                    m_99->clone()),
	         new Binary(opMult,
	                    Location::regOf(8),
	                    new Const(4)));
	CPPUNIT_ASSERT(e.searchAll(&search, result));
	CPPUNIT_ASSERT(result.size() == 2);
	CPPUNIT_ASSERT(*result.front() == *m_rof2);
	Location rof8(opRegOf, new Const(8), nullptr);
	CPPUNIT_ASSERT(*result.back() == rof8);
}
/** \} */

/**
 * Test the Accumulate function.
 */
void
ExpTest::testAccumulate()
{
	Location rof2(opRegOf, new Const(2), nullptr);
	Const nineNine(99);
	Terminal afp(opAFP);

	// Zero terms
	std::list<Exp *> le;
	auto e = Exp::Accumulate(le);
	CPPUNIT_ASSERT(!e);
	CPPUNIT_ASSERT(le.empty());
	delete e;

	// One term
	le.push_back(rof2.clone());
	e = Exp::Accumulate(le);
	CPPUNIT_ASSERT(*e == rof2);
	CPPUNIT_ASSERT(le.empty());
	delete e;

	// Two terms
	le.push_back(rof2.clone());
	le.push_back(nineNine.clone());
	e = Exp::Accumulate(le);
	Binary expected2(opPlus, rof2.clone(), nineNine.clone());
	CPPUNIT_ASSERT(*e == expected2);
	CPPUNIT_ASSERT(le.empty());
	delete e;

	// Three terms, one repeated
	le.push_back(rof2.clone());
	le.push_back(nineNine.clone());
	le.push_back(nineNine.clone());
	e = Exp::Accumulate(le);
	Binary expected3(opPlus,
	                 rof2.clone(),
	                 new Binary(opPlus,
	                            nineNine.clone(),
	                            nineNine.clone()));
	CPPUNIT_ASSERT(*e == expected3);
	CPPUNIT_ASSERT(le.empty());
	delete e;

	// Four terms, one repeated
	le.push_back(rof2.clone());
	le.push_back(nineNine.clone());
	le.push_back(nineNine.clone());
	le.push_back(afp.clone());
	e = Exp::Accumulate(le);
	Binary expected4(opPlus,
	                 rof2.clone(),
	                 new Binary(opPlus,
	                            nineNine.clone(),
	                            new Binary(opPlus,
	                                       nineNine.clone(),
	                                       afp.clone())));
	CPPUNIT_ASSERT(*e == expected4);
	CPPUNIT_ASSERT(le.empty());
	delete e;
}

/**
 * Test the partitionTerms function.
 */
void
ExpTest::testPartitionTerms()
{
	Exp *e;
	// afp + 108 + n - (afp + 92)
	e = new Binary(opMinus,
	               new Binary(opPlus,
	                          new Binary(opPlus,
	                                     new Terminal(opAFP),
	                                     new Const(108)),
	                          new Unary(opVar, new Const("n"))),
	               new Binary(opPlus,
	                          new Terminal(opAFP),
	                          new Const(92)));
	std::list<Exp *> positives, negatives;
	std::vector<int> integers;
	Exp::partitionTerms(e, positives, negatives, integers, false);
	e = Exp::Accumulate(positives);
	Binary expected1(opPlus,
	                 new Terminal(opAFP),
	                 new Unary(opVar, new Const("n")));
	CPPUNIT_ASSERT(*e == expected1);
	delete e;

	e = Exp::Accumulate(negatives);
	Terminal expected2(opAFP);
	CPPUNIT_ASSERT(*e == expected2);
	int size = integers.size();
	CPPUNIT_ASSERT_EQUAL(2, size);
	CPPUNIT_ASSERT_EQUAL(108, integers.front());
	CPPUNIT_ASSERT_EQUAL(-92, integers.back());
	delete e;
}

/**
 * Test the simplifyArith function.
 */
void
ExpTest::testSimplifyArith()
{
	Exp *e;
	// afp + 108 + n - (afp + 92)
	e = new Binary(opMinus,
	               new Binary(opPlus,
	                          new Binary(opPlus,
	                                     new Terminal(opAFP),
	                                     new Const(108)),
	                          new Unary(opVar, new Const("n"))),
	               new Binary(opPlus,
	                          new Terminal(opAFP),
	                          new Const(92)));
	e = e->simplifyArith();
	std::string expected("v[n] + 16");
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;

	// m[(r28 + -4) + 8]
	e = Location::memOf(new Binary(opPlus,
	                               new Binary(opPlus,
	                                          Location::regOf(28),
	                                          new Const(-4)),
	                               new Const(8)));
	e = e->simplifyArith();
	expected = "m[r28 + 4]";
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;

	// r24 + m[(r28 - 4) - 4]
	e = new Binary(opPlus,
	               Location::regOf(24),
	               Location::memOf(new Binary(opMinus,
	                                          new Binary(opMinus,
	                                                     Location::regOf(28),
	                                                     new Const(4)),
	                                          new Const(4))));
	e = e->simplifyArith();
	expected = "r24 + m[r28 - 8]";
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;
}

/**
 * \name Test the simplify function.
 * \{
 */
void
ExpTest::testSimplifyUnary()
{
	// Unaries with integer constant argument
	Exp *u = new Unary(opNeg, new Const(55));
	u = u->simplify();
	Const minus55(-55);
	CPPUNIT_ASSERT(*u == minus55);
	delete u;

	u = new Unary(opNot, new Const(0x55AA));
	u = u->simplify();
	Const exp((int)0xFFFFAA55);
	CPPUNIT_ASSERT(*u == exp);
	delete u;

	u = new Unary(opLNot, new Const(55));
	u = u->simplify();
	Const zero(0);
	CPPUNIT_ASSERT(*u == zero);
	delete u;

	u = new Unary(opLNot, zero.clone());
	u = u->simplify();
	Const one(1);
	CPPUNIT_ASSERT(*u == one);
	delete u;

	// Null test
	u = new Unary(opNeg, new Unary(opVar, new Const("abc")));
	Unary abc(opNeg, new Unary(opVar, new Const("abc")));
	CPPUNIT_ASSERT(*u == abc);
	delete u;
}

void
ExpTest::testSimplifyBinary()
{
	// Add integer consts
	Exp *b = new Binary(opPlus, new Const(2), new Const(3));
	b = b->simplify();
	Const five(5);
	CPPUNIT_ASSERT(*b == five);
	delete b;

	// Multiply integer consts
	b = new Binary(opMult, new Const(2), new Const(3));
	b = b->simplify();
	Const six(6);
	CPPUNIT_ASSERT(*b == six);
	delete b;

	// Shift left two integer constants
	b = new Binary(opShiftL, new Const(2), new Const(3));
	b = b->simplify();
	Const sixteen(16);
	CPPUNIT_ASSERT(*b == sixteen);
	delete b;

	// Shift right arithmetic two integer constants
	b = new Binary(opShiftRA, new Const(-144), new Const(3));
	b = b->simplify();
	Const minus18(-18);
	CPPUNIT_ASSERT(*b == minus18);
	delete b;

	// Bitwise XOR
	b = new Binary(opBitXor, new Const(0x55), new Const(0xF));
	b = b->simplify();
	Const fiveA(0x5A);
	CPPUNIT_ASSERT(*b == fiveA);
	delete b;

	// Xor with self
	b = new Binary(opBitXor, m_rof2->clone(), m_rof2->clone());
	b = b->simplify();
	Const zero(0);
	CPPUNIT_ASSERT(*b == zero);
	delete b;

	// Test commute
	// 77 * r2
	b = new Binary(opMults, new Const(77), m_rof2->clone());
	b = b->simplify();
	// r2 * 77
	Binary exp(opMults, m_rof2->clone(), new Const(77));
	CPPUNIT_ASSERT(*b == exp);

	// x*1
	((Const *)b->getSubExp2())->setInt(1);
	b = b->simplify();
	CPPUNIT_ASSERT(*b == *m_rof2);
	delete b;

	// 0 | r2
	b = new Binary(opBitOr, new Const(0), m_rof2->clone());
	b = b->simplify();
	CPPUNIT_ASSERT(*b == *m_rof2);
	delete b;

	// Left shift by const
	b = new Binary(opShiftL, m_rof2->clone(), new Const(0));
	b = b->simplify();
	CPPUNIT_ASSERT(*b == *m_rof2);
	delete b;

	b = new Binary(opShiftL, m_rof2->clone(), new Const(2));
	b = b->simplify();
	Binary expb1(opMult, m_rof2->clone(), new Const(4));
	CPPUNIT_ASSERT(*b == expb1);
	delete b;

	// Add negative constant
	// r2 + -99
	b = new Binary(opPlus, m_rof2->clone(), new Const(-99));
	// r2 - 99
	Exp *expb2 = new Binary(opMinus, m_rof2->clone(), new Const(99));
	// As of June 2003, I've decided to go the old way. esp + -4 is just
	// too ugly, and all the code has to cope with pluses and minuses anyway,
	// just in case
#define OLD_WAY 1
#if OLD_WAY
	b = b->simplify();
#else
	expb2 = expb2->simplify();
#endif
	CPPUNIT_ASSERT(*b == *expb2);
	delete b; delete expb2;

	std::string expected("((0 + v[a]) - 0) | 0");
	Exp *e = new Binary(opBitOr,
	                    new Binary(opMinus,
	                               new Binary(opPlus,
	                                          new Const(0),
	                                          new Unary(opVar, new Const("a"))),
	                               new Const(0)),
	                    new Const(0));
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	// The above should simplify to just "v[a]"
	e = e->simplify();
	Unary a(opVar, new Const("a"));
	expected = "v[a]";
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;

	// r27 := m[r29 + -4]
	auto as = new Assign(Location::regOf(27),
	                     Location::memOf(new Binary(opPlus,
	                                                Location::regOf(29),
	                                                new Const(-4))));
	as->simplify();
	expected = "   0 *v* r27 := m[r29 - 4]";
	CPPUNIT_ASSERT_EQUAL(expected, as->prints());
	delete as;

	// (false and true) or (Tr24 = <int>)
	e = new Binary(opOr,
	               new Binary(opAnd,
	                          new Terminal(opFalse),
	                          new Terminal(opTrue)),
	               new Binary(opEqual,
	                          new Unary(opTypeOf, Location::regOf(24)),
	                          new TypeVal(new IntegerType(32, 1))));
	e = e->simplify();
	expected = "T[r24] = <int>";
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;
}
/** \} */

/**
 * Test the simplifyAddr function.
 */
void
ExpTest::testSimplifyAddr()
{
	// a[m[1000]] - a[m[r2]{64}]@[0:15]
	Exp *e = new Binary(opMinus,
	                    new Unary(opAddrOf,
	                              Location::memOf(new Const(1000))),
	                    new Ternary(opAt,
	                                new Unary(opAddrOf,
	                                          new Binary(opSize,
	                                                     new Const(64),
	                                                     Location::memOf(Location::regOf(2)))),
	                                new Const(0),
	                                new Const(15)));
	e = e->simplifyAddr();
	std::string expected("1000 - (r2@[0:15])");
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());

	// Now test at top level
	delete e;
	e = new Unary(opAddrOf, Location::memOf(new Const(1000)));
	expected = "1000";
	e = e->simplifyAddr();
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;
}

/**
 * Test the simplifyConstraint function.
 */
void
ExpTest::testSimpConstr()
{
	// After
	//   (T[local1{16}] = <int>) or (Tlocal1{16} = <alpha2*>)
	// gets substituted to
	//   (<char*> = <int>) or (<char*> = <alpha2*>)
	// it should simplify to
	//  <char*> = <alpha2*>
	Exp *e = new Binary(opOr,
	                    new Binary(opEqual,
	                               new TypeVal(new PointerType(new CharType())),
	                               new TypeVal(new IntegerType())),
	                    new Binary(opEqual,
	                               new TypeVal(new PointerType(new CharType())),
	                               new TypeVal(PointerType::newPtrAlpha())));
	e = e->simplifyConstraint();
	std::string expected("<char *> = <alpha0 *>");
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;

	// Similarly,
	//   <char*> = <alpha0*>) and (T[134517848\1\] = <alpha0*>
	// becomes after alpha substitution
	//   (<char*> = <char*>) and (T[134517848\1\] = <char*>)
	// which should simplify to
	//   T[134517848\1\] = <char*>
	e = new Binary(opAnd,
	               new Binary(opEqual,
	                          new TypeVal(new PointerType(new CharType())),
	                          new TypeVal(new PointerType(new CharType()))),
	               new Binary(opEqual,
	                          new Unary(opTypeOf, new Const(0x123456)),
	                          new TypeVal(new PointerType(new CharType()))));
	e = e->simplifyConstraint();
	expected = "T[0x123456] = <char *>";
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
}

/**
 * Various tests of the operator < function.
 */
void
ExpTest::testLess()
{
	// Simple constants
	Const two(2), three(3), mThree(-3), twoPointTwo(2.2), threePointThree(3.3);
	Const mThreePointThree(-3.3);
	CPPUNIT_ASSERT(two < three);
	CPPUNIT_ASSERT(mThree < two);
	CPPUNIT_ASSERT(twoPointTwo < threePointThree);
	CPPUNIT_ASSERT(mThreePointThree < twoPointTwo);
	// Terminal
	Terminal afp(opAFP), agp(opAGP);
	if (opAFP < opAGP)
		CPPUNIT_ASSERT(afp < agp);
	else
		CPPUNIT_ASSERT(agp < afp);
	// Unary
	Unary negTwo(opNeg, new Const(2)), negThree(opNeg, new Const(3));
	// Note that the ordering is not arithmetic!
	CPPUNIT_ASSERT(negTwo < negThree);
	// Binary
	Binary twoByThr(opMult, new Const(2), new Const(3));
	Binary twoByFou(opMult, new Const(2), new Const(4));
	Binary thrByThr(opMult, new Const(3), new Const(3));
	CPPUNIT_ASSERT(twoByThr < twoByFou);
	CPPUNIT_ASSERT(twoByThr < thrByThr);
	// Ternary
	Ternary twoAtThrToFou(opAt, new Const(2), new Const(3), new Const(4));
	Ternary twoAtThrToFiv(opAt, new Const(2), new Const(3), new Const(5));
	CPPUNIT_ASSERT(twoAtThrToFou < twoAtThrToFiv);
	// TypedExp later
}

/**
 * Test maps of Exp*s; exercises some comparison operators.
 */
void
ExpTest::testMapOfExp()
{
	std::map<Exp *, int, lessExpStar> m;
	m[m_rof2] = 200;
	m[m_99] = 99;
	Exp *e = new Binary(opPlus,
	                    new Const(0),
	                    new Binary(opMinus,
	                               new Binary(opMult, new Const(2), new Const(3)),
	                               new Binary(opMult, new Const(4), new Const(5))));
	m[e] = -100;
	Location rof2(opRegOf, new Const(2), nullptr);
	m[&rof2] = 2;  // Should overwrite

	int i = m.size();
	CPPUNIT_ASSERT_EQUAL(3, i);
	i = m[m_rof2];
	CPPUNIT_ASSERT_EQUAL(2, i);
	i = m[&rof2];
	CPPUNIT_ASSERT_EQUAL(2, i);
	i = m[m_99];
	CPPUNIT_ASSERT_EQUAL(99, i);
	i = m[e];
	CPPUNIT_ASSERT_EQUAL(-100, i);
	// When the map goes out of scope, the expressions pointed to still exist
	delete e;
}

/**
 * Test the opList creating and printing.
 */
void
ExpTest::testList()
{
	std::ostringstream o0, o1, o2, o3, o4;
	Exp *l0, *l1, *l2, *l3, *l4;
	// Empty list
	l0 = new Binary(opList, new Terminal(opNil), new Terminal(opNil));
	o0 << *l0;
	std::string expected0("");
	std::string actual0(o0.str());
	CPPUNIT_ASSERT_EQUAL(expected0, actual0);
	delete l0;

	// 1 element list
	l1 = new Binary(opList, Location::param("a"),
	                        new Terminal(opNil));
	o1 << *l1;
	std::string expected1("a");
	std::string actual1(o1.str());
	CPPUNIT_ASSERT_EQUAL(expected1, actual1);
	delete l1;

	// 2 element list
	l2 = new Binary(opList, Location::param("a"),
	     new Binary(opList, Location::param("b"),
	                        new Terminal(opNil)));
	o2 << *l2;
	std::string expected2("a, b");
	std::string actual2(o2.str());
	CPPUNIT_ASSERT_EQUAL(expected2, actual2);
	delete l2;

	// 3 element list
	l3 = new Binary(opList, Location::param("a"),
	     new Binary(opList, Location::param("b"),
	     new Binary(opList, Location::param("c"),
	                        new Terminal(opNil))));
	o3 << *l3;
	std::string expected3("a, b, c");
	std::string actual3(o3.str());
	CPPUNIT_ASSERT_EQUAL(expected3, actual3);
	delete l3;

	// 4 element list
	l4 = new Binary(opList, Location::param("a"),
	     new Binary(opList, Location::param("b"),
	     new Binary(opList, Location::param("c"),
	     new Binary(opList, Location::param("d"),
	                        new Terminal(opNil)))));
	o4 << *l4;
	std::string expected4("a, b, c, d");
	std::string actual4(o4.str());
	CPPUNIT_ASSERT_EQUAL(expected4, actual4);
	delete l4;
}

/**
 * Test the printing of parentheses in complex expressions.
 */
void
ExpTest::testParen()
{
	Assign a(Location::regOf(Location::param("rd")),
	         new Binary(opBitAnd,
	                    Location::regOf(Location::param("rs1")),
	                    new Binary(opMinus,
	                               new Binary(opMinus,
	                                          new Const(0),
	                                          Location::param("reg_or_imm")),
	                               new Const(1))));
	std::string expected("   0 *v* r[rd] := r[rs1] & ((0 - reg_or_imm) - 1)");
	//std::ofstream of("andn.dot");
	//a.getRight()->createDot(of);
	//of.close();
	CPPUNIT_ASSERT_EQUAL(expected, a.prints());
}

/**
 * Test succ(r[k]) == r[k+1].
 */
void
ExpTest::testFixSuccessor()
{
	Exp *e;
	// Trivial test (should not affect)
	e = new Binary(opMinus, m_99->clone(), m_rof2->clone());
	e = e->fixSuccessor();
	std::string expected("99 - r2");
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;

	e = new Unary(opSuccessor, Location::regOf(2));
	e = e->fixSuccessor();
	expected = "r3";
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;
}

/**
 * Test removal of zero fill, sign extend, truncates.
 */
void
ExpTest::testKillFill()
{
	Exp *e;
	// r18 + sgnex(16,32,m[r16 + 16])
	e = new Binary(opPlus,
	               Location::regOf(18),
	               new Ternary(opSgnEx,
	                           new Const(16),
	                           new Const(32),
	                           Location::memOf(new Binary(opPlus,
	                                                      Location::regOf(16),
	                                                      new Const(16)))));
	e = e->killFill();
	std::string expected("r18 + m[r16 + 16]");
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;

	// Try again but at top level
	e = new Ternary(opZfill,
	                new Const(16),
	                new Const(32),
	                Location::memOf(new Binary(opPlus,
	                                           Location::regOf(16),
	                                           new Const(16))));
	e = e->killFill();
	expected = "m[r16 + 16]";
	CPPUNIT_ASSERT_EQUAL(expected, e->prints());
	delete e;
}

/**
 * Test that a+K+b is the same as a+b+K when each is simplified.
 */
void
ExpTest::testAssociativity()
{
	Exp *e1, *e2;
	// (r8 + m[m[r8 + 12] + -12]) + 12
	e1 = new Binary (opPlus,
	                 new Binary(opPlus,
	                            Location::regOf(8),
	                            Location::memOf(new Binary(opPlus,
	                                                       Location::memOf(new Binary(opPlus,
	                                                                                  Location::regOf(8),
	                                                                                  new Const(12))),
	                                                       new Const(-12)))),
	                 new Const(12));
	// (r8 + 12) + m[m[r8 + 12] + -12]
	e2 = new Binary(opPlus,
	                new Binary(opPlus,
	                           Location::regOf(8),
	                           new Const(12)),
	                Location::memOf(new Binary(opPlus,
	                                           Location::memOf(new Binary(opPlus,
	                                                                      Location::regOf(8),
	                                                                      new Const(12))),
	                                           new Const(-12))));
	// Note: at one stage, simplifyArith was part of simplify().
	// Now call implifyArith() explicitly only where needed
	e1 = e1->simplify()->simplifyArith();
	e2 = e2->simplify()->simplifyArith();
	CPPUNIT_ASSERT_EQUAL(e1->prints(), e2->prints());
}

/**
 * Test Assign::subscriptVar and thereby Exp::expSubscriptVar.
 */
void
ExpTest::testSubscriptVar()
{
	// m[r28 - 4] := r28 + r29
	Exp *left = Location::memOf(new Binary(opMinus,
	                                       Location::regOf(28),
	                                       new Const(4)));
	Statement *s = new Assign(left->clone(),
	                          new Binary(opPlus,
	                                     Location::regOf(28),
	                                     Location::regOf(29)));

	// Subtest 1: should do nothing
	Exp *r28 = Location::regOf(28);
	Statement * def1 = new Assign(r28->clone(), r28->clone());
	def1->setNumber(12);
	def1->subscriptVar(left, def1);  // Should do nothing
	std::string expected1;
	expected1 = "   0 *v* m[r28 - 4] := r28 + r29";
	std::ostringstream actual1;
	actual1 << *s;
	CPPUNIT_ASSERT_EQUAL(expected1, actual1.str());
	// m[r28 - 4]

	// Subtest 2: Ordinary substitution, on LHS and RHS
	s->subscriptVar(r28, def1);
	std::string expected2("   0 *v* m[r28{12} - 4] := r28{12} + r29");
	std::ostringstream actual2;
	actual2 << *s;
	CPPUNIT_ASSERT_EQUAL(expected2, actual2.str());

	// Subtest 3: change to a different definition
	// 99: r28 := 0
	// Note: behaviour has changed. Now, we don't allow re-renaming, so it should stay the same
	Statement *def3 = new Assign(Location::regOf(28), new Const(0));
	def3->setNumber(99);
	s->subscriptVar(r28, def3);
	std::string expected3("   0 *v* m[r28{12} - 4] := r28{12} + r29");
	std::ostringstream actual3;
	actual3 << *s;
	CPPUNIT_ASSERT_EQUAL(expected3, actual3.str());

	delete s; delete def1; delete def3; delete r28; delete left;
}

/**
 * Test opTypeOf and TypeVal (type values).
 */
void
ExpTest::testTypeOf()
{
	// Tr24{5} = Tr25{9}
	std::string expected1("T[r24{5}] = T[r25{9}]");
	Statement *s5 = new Assign;
	Statement *s9 = new Assign;
	s5->setNumber(5);
	s9->setNumber(9);
	Exp *e = new Binary(opEqual,
	                    new Unary(opTypeOf, new RefExp(Location::regOf(24), s5)),
	                    new Unary(opTypeOf, new RefExp(Location::regOf(25), s9)));
	std::ostringstream actual1;
	actual1 << *e;
	CPPUNIT_ASSERT_EQUAL(expected1, actual1.str());

	// Tr24{5} = <float>
	std::string expected2("T[r24{5}] = <float>");
	delete e;
	Type *t = new FloatType(32);
	e = new Binary(opEqual,
	               new Unary(opTypeOf, new RefExp(Location::regOf(24), s5)),
	               new TypeVal(t));
	std::ostringstream actual2;
	actual2 << *e;
	CPPUNIT_ASSERT_EQUAL(expected2, actual2.str());
}

/**
 * Test setting and printing of constant "subscripts".
 */
void
ExpTest::testSetConscripts()
{
	// m[1000] + 1000
	Exp *e = new Binary(opPlus,
	                    Location::memOf(new Const(1000), nullptr),
	                    new Const(1000));
	e->setConscripts(0, false);
	std::string expected("m[1000\\1\\] + 1000\\2\\");
	std::ostringstream actual;
	actual << *e;
	CPPUNIT_ASSERT_EQUAL(expected, actual.str());

	// Clear them
	e->setConscripts(0, true);
	expected = "m[1000] + 1000";
	std::ostringstream actual1;
	actual1 << *e;
	CPPUNIT_ASSERT_EQUAL(expected, actual1.str());

	// m[r28 + 1000]
	e = Location::memOf(new Binary(opPlus,
	                               Location::regOf(28),
	                               new Const(1000)));
	e->setConscripts(0, false);
	expected = "m[r28 + 1000\\1\\]";
	std::ostringstream act2;
	act2 << *e;
	CPPUNIT_ASSERT_EQUAL(expected, act2.str());

	// Clear
	e->setConscripts(0, true);
	expected = "m[r28 + 1000]";
	std::ostringstream act3;
	act3 << *e;
	CPPUNIT_ASSERT_EQUAL(expected, act3.str());
}

/**
 * Test finding the locations used by an expression.
 */
void
ExpTest::testAddUsedLocs()
{
	// Null case
	Exp *e = new Terminal(opNil);
	LocationSet l;
	e->addUsedLocs(l);
	CPPUNIT_ASSERT(l.empty());

	// Const: "foo"
	e = new Const("foo");
	e->addUsedLocs(l);
	CPPUNIT_ASSERT(l.empty());

	// Simple terminal: %pc
	e = new Terminal(opPC);
	e->addUsedLocs(l);
	std::string expected = "%pc";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());

	// Simple location: r28
	l.clear();
	e = Location::regOf(28);
	e->addUsedLocs(l);
	expected = "r28";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());

	// Memory location: m[r28-4]
	l.clear();
	e = Location::memOf(new Binary(opMinus,
	                               Location::regOf(28),
	                               new Const(4)));
	e->addUsedLocs(l);
	expected = "r28,\tm[r28 - 4]";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());

	// Unary: a[m[r28-4]]
	l.clear();
	e = new Unary(opAddrOf, e);
	e->addUsedLocs(l);
	expected = "r28,\tm[r28 - 4]";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());

	// Binary: r24 + r25
	l.clear();
	e = new Binary(opPlus,
	               Location::regOf(24),
	               Location::regOf(25));
	e->addUsedLocs(l);
	expected = "r24,\tr25";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());

	// Ternary: r24@[r25:r26]
	l.clear();
	e = new Ternary(opAt,
	                Location::regOf(24),
	                Location::regOf(25),
	                Location::regOf(26));
	e->addUsedLocs(l);
	expected = "r24,\tr25,\tr26";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());

	// Simple RefExp: r28{2}
	l.clear();
	Assign a(e, e);
	a.setNumber(2);
	e = new RefExp(Location::regOf(28), &a);
	e->addUsedLocs(l);
	expected = "r28{2}";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());

	// RefExp: m[r28{2} - 4]{3}
	Assign t(e, e);
	t.setNumber(3);
	e = new RefExp(Location::memOf(new Binary(opMinus,
	                                          new RefExp(Location::regOf(28), &a),
	                                          new Const(4))),
	               &t);
	e->addUsedLocs(l);
	expected = "r28{2},\tm[r28{2} - 4]{3}";
	CPPUNIT_ASSERT_EQUAL(expected, l.prints());
}

/**
 * Test the subscripting of variables (locations).
 */
void
ExpTest::testSubscriptVars()
{
	// Null case: %pc
	Assign s9(new Terminal(opNil), new Terminal(opNil));
	s9.setNumber(9);
	Exp *search = Location::regOf(28);
	Exp *e = new Terminal(opPC);
	e = e->expSubscriptVar(search, &s9);
	std::string expected("%pc");
	std::ostringstream ost1;
	ost1 << *e;
	std::string actual = ost1.str();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Simple case: r28
	e = search->clone();
	e = e->expSubscriptVar(search, &s9);
	expected = "r28{9}";
	std::ostringstream ost2;
	ost2 << *e;
	actual = ost2.str();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// A temp
	e = Location::tempOf(new Const("tmp1"));
	e = e->expSubscriptVar(e->clone(), &s9);
	expected = "tmp1{9}";
	std::ostringstream ost3;
	ost3 << *e;
	actual = ost3.str();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// m[r28] + r28
	e = new Binary(opPlus,
	               Location::memOf(Location::regOf(28)),
	               Location::regOf(28));
	e = e->expSubscriptVar(search, &s9);
	expected = "m[r28{9}] + r28{9}";
	std::ostringstream ost4;
	ost4 << *e;
	actual = ost4.str();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// RefExp: r28{7} -> r28{9}
	// Again, changed behaviour: don't resubscript any location
	Assign s7(new Terminal(opNil), new Terminal(opNil));
	s7.setNumber(7);
	e = new RefExp(search->clone(), &s7);
	e = e->expSubscriptVar(search, &s9);
	expected = "r28{7}";
	std::ostringstream ost5;
	ost5 << *e;
	actual = ost5.str();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// m[r28{7} + 4]{8}
	Assign s8(new Terminal(opNil), new Terminal(opNil));
	s8.setNumber(8);
	e = new RefExp(Location::memOf(new Binary(opPlus,
	                                          new RefExp(Location::regOf(28), &s7),
	                                          new Const(4))),
	               &s8);
	e = e->expSubscriptVar(search, &s9);
	expected = "m[r28{7} + 4]{8}";
	std::ostringstream ost6;
	ost6 << *e;
	actual = ost6.str();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// r24{7} with r24{7} and 0: should not change: RefExps should not compare
	// at the top level, only with their base expression (here r24, not r24{7})
	e = new RefExp(Location::regOf(24), &s7);
	e = e->expSubscriptVar(e->clone(), nullptr);
	expected = "r24{7}";
	std::ostringstream ost7;
	ost7 << *e;
	actual = ost7.str();
	CPPUNIT_ASSERT_EQUAL(expected, actual);
}

/**
 * Test the FlagsFinder and BareMemofFinder visitors.
 */
void
ExpTest::testVisitors()
{
	auto s7 = Assign(new Terminal(opNil), new Terminal(opNil));
	// m[SETTFLAGS(m[1000], r8)]{7}
	s7.setNumber(7);
	auto e1 = new RefExp(Location::memOf(new Binary(opFlagCall,
	                                                new Const("SETFFLAGS"),
	                                                new Binary(opList,
	                                                           Location::memOf(new Const(0x1000)),  // A bare memof
	                                                           new Binary(opList,
	                                                                      Location::regOf(8),
	                                                                      new Terminal(opNil))))),
	                     &s7);

	// m[0x2000]
	auto e2 = Location::memOf(new Const(0x2000));

	// r1+m[1000]{7}*4
	auto e3 = new Binary(opPlus,
	                     Location::regOf(1),
	                     new Binary(opMult,
	                                new RefExp(Location::memOf(new Const(1000)), &s7),
	                                new Const(4)));

	int res = e1->containsFlags();
	CPPUNIT_ASSERT_EQUAL(1, res);
	res = e2->containsFlags();
	CPPUNIT_ASSERT_EQUAL(0, res);
	res = e3->containsFlags();
	CPPUNIT_ASSERT_EQUAL(0, res);

#if 0  // No longer used
	res = e1->containsBareMemof();
	CPPUNIT_ASSERT_EQUAL(1, res);
	res = e2->containsBareMemof();
	CPPUNIT_ASSERT_EQUAL(1, res);
	res = e3->containsBareMemof();
	CPPUNIT_ASSERT_EQUAL(0, res);
#endif

	delete e1;
	delete e2;
	delete e3;
}
