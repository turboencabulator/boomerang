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
 * \brief Tests the Type class and some utility functions.
 */
class TypeTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(TypeTest);
	CPPUNIT_TEST(testTypeLong);
	CPPUNIT_TEST(testNotEqual);
	//CPPUNIT_TEST(testCompound);
	CPPUNIT_TEST(testDataInterval);
	//CPPUNIT_TEST(testDataIntervalOverlaps);
	CPPUNIT_TEST_SUITE_END();

public:
	TypeTest() { }

	void testTypeLong();
	void testNotEqual();
	void testCompound();

	void testDataInterval();
	void testDataIntervalOverlaps();
};
