/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the ParserTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define SPARC_SSL       "frontend/machine/sparc/sparc.ssl"

#include "ParserTest.h"
#include "sslparser.h"

#include <sstream>

/**
 * Test reading the SSL file.
 */
void ParserTest::testRead()
{
	RTLInstDict d;
	CPPUNIT_ASSERT(d.readSSLFile(SPARC_SSL));
}

/**
 * Test parsing an expression.
 */
void ParserTest::testExp()
{
	std::string s("*i32* r0 := 5 + 6");
	Statement *a = SSLParser::parseExp(s.c_str());
	CPPUNIT_ASSERT(a);
	std::ostringstream ost;
	a->print(ost);
	CPPUNIT_ASSERT_EQUAL("   0 " + s, std::string(ost.str()));
	std::string s2 = "*i32* r[0] := 5 + 6";
	a = SSLParser::parseExp(s2.c_str());
	CPPUNIT_ASSERT(a);
	std::ostringstream ost2;
	a->print(ost2);
	// Still should print to string s, not s2
	CPPUNIT_ASSERT_EQUAL("   0 " + s, std::string(ost2.str()));
}
