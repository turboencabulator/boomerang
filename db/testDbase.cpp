/**
 * \file
 * \ingroup UnitTest
 * \brief Command line test of the Exp and related classes.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exp.h"

#include "ExpTest.h"
#include "ProgTest.h"
#include "ProcTest.h"
#include "RtlTest.h"
#include "ParserTest.h"
#include "TypeTest.h"

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

	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
