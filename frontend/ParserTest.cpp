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

#include "ParserTest.h"

#include "sslinst.h"
#include "sslparser.h"

#include <sstream>
#include <string>

#define SPARC_SSL       "frontend/machine/sparc/sparc.ssl"

/**
 * Test reading the SSL file.
 */
void
ParserTest::testRead()
{
	RTLInstDict d;
	CPPUNIT_ASSERT(d.readSSLFile(SPARC_SSL));
}

/**
 * Parse an assignment from a string.
 */
static Statement *
parseStmt(const std::string &str)
{
	std::string s("OPCODE " + str);
	std::istringstream ss(s);
	auto p = SSLDriver();
	RTLInstDict d;
	p.parse(ss, d);
	CPPUNIT_ASSERT(d.idict.count("OPCODE"));
	TableEntry &t = d.idict["OPCODE"];
	CPPUNIT_ASSERT(!t.rtl.getList().empty());
	return t.rtl.getList().front();
}

/**
 * Test parsing an expression.
 */
void
ParserTest::testExp()
{
	std::string s("*i32* r0 := 5 + 6");
	Statement *a = parseStmt(s);
	CPPUNIT_ASSERT(a);
	CPPUNIT_ASSERT_EQUAL("   0 " + s, a->prints());
	std::string s2 = "*i32* r[0] := 5 + 6";
	a = parseStmt(s2);
	CPPUNIT_ASSERT(a);
	// Still should print to string s, not s2
	CPPUNIT_ASSERT_EQUAL("   0 " + s, a->prints());
}
