/**
 * \file
 * \brief Provides the implementation for the ProgTest class, which tests the
 *        Exp and derived classes.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define HELLO_PENTIUM       "test/pentium/hello"

#include "ProgTest.h"
#include "frontend.h"

#include <map>
#include <sstream>

/*==============================================================================
 * FUNCTION:        ProgTest::setUp
 * OVERVIEW:        Set up some expressions for use with all the tests
 * NOTE:            Called before any tests
 * PARAMETERS:      <none>
 * RETURNS:         <nothing>
 *============================================================================*/
void ProgTest::setUp()
{
	//prog.setName("default name");
}

/*==============================================================================
 * FUNCTION:        ProgTest::testName
 * OVERVIEW:        Test setting and reading name
 *============================================================================*/
void ProgTest::testName()
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
