/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the FrontPentTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "FrontPentTest.h"

#include "decoder.h"
#include "frontend.h"
#include "prog.h"
#include "rtl.h"
#include "types.h"

#include <sstream>
#include <string>

#define HELLO_PENT      "test/pentium/hello"
#define BRANCH_PENT     "test/pentium/branch"
#define FEDORA2_TRUE    "test/pentium/fedora2_true"
#define FEDORA3_TRUE    "test/pentium/fedora3_true"
#define SUSE_TRUE       "test/pentium/suse_true"

/**
 * \name Test decoding some Pentium instructions.
 * \{
 */
void
FrontPentTest::test1()
{
	auto prog = Prog::open(HELLO_PENT);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	CPPUNIT_ASSERT(fe->getFrontEndId() == PLAT_PENTIUM);

	bool gotMain;
	ADDRESS addr = fe->getMainEntryPoint(gotMain);
	CPPUNIT_ASSERT(addr != NO_ADDRESS);

	// Decode first instruction
	DecodeResult inst;
	fe->decodeInstruction(inst, addr);

	std::string expected("08048328    0 *32* m[r28 - 4] := r29\n"
	                     "            0 *32* r28 := r28 - 4\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	addr += inst.numBytes;
	fe->decodeInstruction(inst, addr);
	expected = std::string("08048329    0 *32* r29 := r28\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	addr = 0x804833b;
	fe->decodeInstruction(inst, addr);
	expected = std::string("0804833b    0 *32* m[r28 - 4] := 0x80483fc\n"
	                       "            0 *32* r28 := r28 - 4\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	delete prog;
}

void
FrontPentTest::test2()
{
	DecodeResult inst;
	std::string expected;

	auto prog = Prog::open(HELLO_PENT);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	CPPUNIT_ASSERT(fe->getFrontEndId() == PLAT_PENTIUM);

	fe->decodeInstruction(inst, 0x8048345);
	expected = std::string("08048345    0 *32* tmpi := r28\n"
	                       "            0 *32* r28 := r28 + 16\n"
	                       "            0 *v* %flags := ADDFLAGS32(tmpi, 16, r28)\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	fe->decodeInstruction(inst, 0x8048348);
	expected = std::string("08048348    0 *32* r24 := 0\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	fe->decodeInstruction(inst, 0x8048329);
	expected = std::string("08048329    0 *32* r29 := r28\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	delete prog;
}

void
FrontPentTest::test3()
{
	DecodeResult inst;
	std::string expected;

	auto prog = Prog::open(HELLO_PENT);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	CPPUNIT_ASSERT(fe->getFrontEndId() == PLAT_PENTIUM);

	fe->decodeInstruction(inst, 0x804834d);
	expected = std::string("0804834d    0 *32* r28 := r29\n"
	                       "            0 *32* r29 := m[r28]\n"
	                       "            0 *32* r28 := r28 + 4\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	fe->decodeInstruction(inst, 0x804834e);
	expected = std::string("0804834e    0 *32* %pc := m[r28]\n"
	                       "            0 *32* r28 := r28 + 4\n"
	                       "            0 RET\n"
	                       "              Modifieds: \n"
	                       "              Reaching definitions: \n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	delete prog;
}
/** \} */

void
FrontPentTest::testBranch()
{
	DecodeResult inst;
	std::string expected;

	auto prog = Prog::open(BRANCH_PENT);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	CPPUNIT_ASSERT(fe->getFrontEndId() == PLAT_PENTIUM);

	// jne
	fe->decodeInstruction(inst, 0x8048979);
	expected = std::string("08048979    0 BRANCH 0x8048988, condition not equal\n"
	                       "              High level: %flags\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	// jg
	fe->decodeInstruction(inst, 0x80489c1);
	expected = std::string("080489c1    0 BRANCH 0x80489d5, condition signed greater\n"
	                       "              High level: %flags\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	// jbe
	fe->decodeInstruction(inst, 0x8048a1b);
	expected = std::string("08048a1b    0 BRANCH 0x8048a2a, condition unsigned less or equal\n"
	                       "              High level: %flags\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	delete prog;
}

/**
 * Test the algorithm for finding main, when there is a call to
 * __libc_start_main.  Also tests the loader hack.
 */
void
FrontPentTest::testFindMain()
{
	auto prog = Prog::open(FEDORA2_TRUE);
	CPPUNIT_ASSERT(prog);
	auto fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	bool found;
	ADDRESS addr = fe->getMainEntryPoint(found);
	ADDRESS expected = 0x8048b10;
	CPPUNIT_ASSERT_EQUAL(expected, addr);
	delete prog;

	prog = Prog::open(FEDORA3_TRUE);
	CPPUNIT_ASSERT(prog);
	fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	addr = fe->getMainEntryPoint(found);
	expected = 0x8048c4a;
	CPPUNIT_ASSERT_EQUAL(expected, addr);
	delete prog;

	prog = Prog::open(SUSE_TRUE);
	CPPUNIT_ASSERT(prog);
	fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	addr = fe->getMainEntryPoint(found);
	expected = 0x8048b60;
	CPPUNIT_ASSERT_EQUAL(expected, addr);
	delete prog;
}
