/**
 * \file
 * \brief Command line test of the Rtl class.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "RtlTest.h"

#include <cppunit/ui/text/TestRunner.h>

#include <cstdlib>

int main(int argc, char *argv[])
{
	CppUnit::TextUi::TestRunner runner;

	runner.addTest(RtlTest::suite());

	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
