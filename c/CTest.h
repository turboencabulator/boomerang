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
 * \brief Tests the C parser.
 */
class CTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(CTest);
	CPPUNIT_TEST(testSignature);
	CPPUNIT_TEST_SUITE_END();

public:
	CTest() { }

	void testSignature();
};
