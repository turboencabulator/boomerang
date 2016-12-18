/**
 * \file
 * \brief Command line test of the BinaryFile and related classes.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LoaderTest.h"

#include <cppunit/ui/text/TestRunner.h>

#include <cstdlib>

int main(int argc, char *argv[])
{
	CppUnit::TextUi::TestRunner runner;

	runner.addTest(LoaderTest::suite());

	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
