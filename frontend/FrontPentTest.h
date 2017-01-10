/**
 * \file
 * \ingroup UnitTest
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <cppunit/extensions/HelperMacros.h>

/**
 * \ingroup UnitTest
 * \brief Tests the Pentium front end.
 */
class FrontPentTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(FrontPentTest);
	CPPUNIT_TEST(test1);
	CPPUNIT_TEST(test2);
	CPPUNIT_TEST(test3);
	CPPUNIT_TEST(testBranch);
	//CPPUNIT_TEST(testFindMain);
	CPPUNIT_TEST_SUITE_END();

public:
	FrontPentTest() { }

	void test1();
	void test2();
	void test3();
	void testBranch();
	void testFindMain();
};
