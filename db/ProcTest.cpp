/**
 * \file
 * \brief Provides the implementation for the ProcTest class, which tests the
 *        Proc class.
 */

#define HELLO_PENTIUM       "test/pentium/hello"

#include "ProcTest.h"
#include "BinaryFile.h"
#include "BinaryFileStub.h"
#include "pentiumfrontend.h"

#include <sstream>
#include <map>

/*==============================================================================
 * FUNCTION:        ProcTest::tearDown
 * OVERVIEW:        Delete expressions created in setUp
 * NOTE:            Called after all tests
 * PARAMETERS:      <none>
 * RETURNS:         <nothing>
 *============================================================================*/
void ProcTest::tearDown()
{
	delete m_proc;
}

/*==============================================================================
 * FUNCTION:        ProcTest::testName
 * OVERVIEW:        Test setting and reading name, constructor, native address
 *============================================================================*/
void ProcTest::testName()
{
	Prog *prog = new Prog;
	std::string nm("default name");
	BinaryFile *pBF = BinaryFile::open(HELLO_PENTIUM);
	CPPUNIT_ASSERT(pBF != 0);
	FrontEnd *pFE = new PentiumFrontEnd(pBF, prog);
	CPPUNIT_ASSERT(pFE != 0);
	prog->setFrontEnd(pFE);
	CPPUNIT_ASSERT(prog);
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
