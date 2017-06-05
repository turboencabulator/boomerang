/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the ProgTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ProgTest.h"

#include "frontend.h"
#include "prog.h"

#include <string>

#define HELLO_PENTIUM       "test/pentium/hello"

/**
 * Test setting and reading name.
 */
void
ProgTest::testName()
{
	Prog *prog = new Prog;
	FrontEnd *pFE = FrontEnd::open(HELLO_PENTIUM, prog);  // Don't actually use it
	// We need a Prog object with a pBF (for getEarlyParamExp())
	std::string actual(prog->getName());
	std::string expected(HELLO_PENTIUM);
	CPPUNIT_ASSERT_EQUAL(expected, actual);
	std::string name("Happy prog");
	prog->setName(name.c_str());
	actual = prog->getName();
	CPPUNIT_ASSERT_EQUAL(name, actual);
	delete prog;
}

// Pathetic: the second test we had (for readLibraryParams) is now obsolete;
// the front end does this now.
