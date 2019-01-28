/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the interface for the ProcTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <cppunit/extensions/HelperMacros.h>

class Proc;

/**
 * \ingroup UnitTest
 * \brief Tests the Proc class.
 */
class ProcTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(ProcTest);
	CPPUNIT_TEST(testName);
	CPPUNIT_TEST_SUITE_END();

protected:
	Proc *m_proc;

public:
	void tearDown();

	void testName();
};
