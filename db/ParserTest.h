/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "prog.h"

#include <cppunit/extensions/HelperMacros.h>

class ParserTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(ParserTest);
	CPPUNIT_TEST(testRead);
	CPPUNIT_TEST(testExp);
	CPPUNIT_TEST_SUITE_END();

public:
	ParserTest() { }

	void testRead();
	void testExp();
};
