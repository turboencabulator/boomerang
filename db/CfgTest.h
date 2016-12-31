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
 * \brief Tests the Cfg class.
 */
class CfgTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(CfgTest);
	// Oops - they were all for dataflow. Need some real Cfg tests!
	CPPUNIT_TEST(testDominators);
	CPPUNIT_TEST(testSemiDominators);
	//CPPUNIT_TEST(testPlacePhi);
	//CPPUNIT_TEST(testPlacePhi2);
	CPPUNIT_TEST(testRenameVars);
	CPPUNIT_TEST_SUITE_END();

public:
	CfgTest() { }

	void testDominators();
	void testSemiDominators();
	void testPlacePhi();
	void testPlacePhi2();
	void testRenameVars();
};
