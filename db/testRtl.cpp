/**
 * \file
 * \ingroup UnitTest
 * \brief Command line test of the RTL class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "RtlTest.h"

#include <cppunit/ui/text/TestRunner.h>

#include <cstdlib>

int
main(int argc, char *argv[])
{
	CppUnit::TextUi::TestRunner runner;

	runner.addTest(RtlTest::suite());

	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
