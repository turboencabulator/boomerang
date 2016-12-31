/**
 * \file
 * \ingroup UnitTest
 * \brief Command line test of all of Boomerang.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/**
 * \defgroup UnitTest Unit Testing
 */

/**
 * \defgroup UnitTestStub Unit Testing Stubs
 * \ingroup UnitTest
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ExpTest.h"
#include "ProgTest.h"
#include "ProcTest.h"
#include "RtlTest.h"
#include "ParserTest.h"
#include "TypeTest.h"
#include "FrontSparcTest.h"
#include "FrontPentTest.h"
#include "CTest.h"
#include "StatementTest.h"
#include "CfgTest.h"
#include "DfaTest.h"

#include <cppunit/ui/text/TestRunner.h>

#include <cstdlib>

int main(int argc, char *argv[])
{
	CppUnit::TextUi::TestRunner runner;

	runner.addTest(ExpTest::suite());
	runner.addTest(ProgTest::suite());
	runner.addTest(ProcTest::suite());
	runner.addTest(RtlTest::suite());
	runner.addTest(ParserTest::suite());
	runner.addTest(TypeTest::suite());
	runner.addTest(FrontSparcTest::suite());
	runner.addTest(FrontPentTest::suite());
	runner.addTest(CTest::suite());
	runner.addTest(StatementTest::suite());
	runner.addTest(CfgTest::suite());
	runner.addTest(DfaTest::suite());

	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
