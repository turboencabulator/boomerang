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
 * \brief Tests the RTL class.
 */
class RtlTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(RtlTest);
	CPPUNIT_TEST(testAppend);
	CPPUNIT_TEST(testClone);
	CPPUNIT_TEST(testVisitor);
	CPPUNIT_TEST(testSetConscripts);
	CPPUNIT_TEST_SUITE_END();

public:
	void testAppend();
	void testClone();
	void testVisitor();
	void testIsCompare();
	void testSetConscripts();
};
