/**
 * \file
 * \brief Command line test of the Exp class.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ExpTest.h"

#include <cppunit/ui/text/TestRunner.h>

#include <cstdlib>

int main(int argc, char *argv[])
{
	CppUnit::TextUi::TestRunner runner;

	runner.addTest(ExpTest::suite());

	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
