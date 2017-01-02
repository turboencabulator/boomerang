/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the CTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "CTest.h"

#include "ansi-c-parser.h"
#include "sigenum.h"
#include "type.h"

#include <sstream>

void CTest::testSignature()
{
	std::istringstream os("int printf(char *fmt, ...);");
	AnsiCParser p(os, false);
	p.yyparse(PLAT_PENTIUM, CONV_C);
	CPPUNIT_ASSERT_EQUAL(1, (int)p.signatures.size());
	Signature *sig = p.signatures.front();
	CPPUNIT_ASSERT_EQUAL(std::string("printf"), std::string(sig->getName()));
	CPPUNIT_ASSERT(sig->getReturnType(0)->resolvesToInteger());
	Type *t = new PointerType(new CharType());
	// Pentium signatures used to have esp prepended to the list of parameters; no more?
	int num = sig->getNumParams();
	CPPUNIT_ASSERT_EQUAL(1, num);
	CPPUNIT_ASSERT(*sig->getParamType(0) == *t);
	CPPUNIT_ASSERT_EQUAL(std::string("fmt"), std::string(sig->getParamName(0)));
	CPPUNIT_ASSERT(sig->hasEllipsis());
	delete t;
}
