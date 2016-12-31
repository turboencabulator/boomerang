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
 * \brief Tests the BinaryFile and derived classes.
 */
class LoaderTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(LoaderTest);
	CPPUNIT_TEST(testSparcLoad);
	CPPUNIT_TEST(testPentiumLoad);
	CPPUNIT_TEST(testHppaLoad);
	CPPUNIT_TEST(testPalmLoad);
	CPPUNIT_TEST(testWinLoad);

	CPPUNIT_TEST(testMicroDis1);
	CPPUNIT_TEST(testMicroDis2);

	CPPUNIT_TEST(testElfHash);
	CPPUNIT_TEST_SUITE_END();

public:
	LoaderTest() { }

	void testSparcLoad();
	void testPentiumLoad();
	void testHppaLoad();
	void testPalmLoad();
	void testWinLoad();

	void testMicroDis1();
	void testMicroDis2();

	void testElfHash();
};
