/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the ProcTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ProcTest.h"

#include "frontend.h"
#include "proc.h"
#include "prog.h"

#include <string>

#define HELLO_PENTIUM       "test/pentium/hello"

/**
 * Delete expressions created in setUp.
 *
 * \note Called after all tests.
 */
void
ProcTest::tearDown()
{
	delete m_proc;
}

/**
 * Test setting and reading name, constructor, native address.
 */
void
ProcTest::testName()
{
	Prog *prog = new Prog;
	std::string nm("default name");
	FrontEnd *pFE = FrontEnd::open(HELLO_PENTIUM, prog);
	CPPUNIT_ASSERT(pFE != 0);
	pFE->readLibraryCatalog();              // Since we are not decoding
	m_proc = new UserProc(prog, nm, 20000); // Will print in decimal if error
	std::string actual(m_proc->getName());
	CPPUNIT_ASSERT_EQUAL(std::string("default name"), actual);

	std::string name("printf");
	LibProc lp(prog, name, 30000);
	actual = lp.getName();
	CPPUNIT_ASSERT_EQUAL(name, actual);

	ADDRESS a = lp.getNativeAddress();
	ADDRESS expected = 30000;
	CPPUNIT_ASSERT_EQUAL(expected, a);
	a = m_proc->getNativeAddress();
	expected = 20000;
	CPPUNIT_ASSERT_EQUAL(expected, a);

	delete prog;
}
