/**
 * \file
 * \ingroup UnitTest
 * \brief Command line test of the BinaryFile and related classes.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
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
