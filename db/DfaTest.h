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
 * \brief Tests the data flow based type analysis code.
 */
class DfaTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(DfaTest);
	CPPUNIT_TEST(testMeetInt);
	CPPUNIT_TEST(testMeetSize);
	CPPUNIT_TEST(testMeetPointer);
	CPPUNIT_TEST(testMeetUnion);
	CPPUNIT_TEST_SUITE_END();

public:
	void testMeetInt();
	void testMeetSize();
	void testMeetPointer();
	void testMeetUnion();
};
