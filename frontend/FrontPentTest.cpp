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

#include "BinaryFile.h"
#include "BinaryFileStub.h"
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
	auto prog = new Prog;
	auto pBF = BinaryFile::open(HELLO_PENT);
	if (!pBF)
		pBF = new BinaryFileStub();
	CPPUNIT_ASSERT(pBF);
	CPPUNIT_ASSERT(pBF->getMachine() == MACHINE_PENTIUM);
	auto pFE = FrontEnd::open(pBF, prog);

	bool gotMain;
	ADDRESS addr = pFE->getMainEntryPoint(gotMain);
	CPPUNIT_ASSERT(addr != NO_ADDRESS);

	// Decode first instruction
	DecodeResult inst = pFE->decodeInstruction(addr);

	std::string expected("08048328    0 *32* m[r28 - 4] := r29\n"
	                     "            0 *32* r28 := r28 - 4\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	addr += inst.numBytes;
	inst = pFE->decodeInstruction(addr);
	expected = std::string("08048329    0 *32* r29 := r28\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	addr = 0x804833b;
	inst = pFE->decodeInstruction(addr);
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

	auto prog = new Prog;
	auto pBF = BinaryFile::open(HELLO_PENT);
	if (!pBF)
		pBF = new BinaryFileStub();
	CPPUNIT_ASSERT(pBF);
	CPPUNIT_ASSERT(pBF->getMachine() == MACHINE_PENTIUM);
	auto pFE = FrontEnd::open(pBF, prog);

	inst = pFE->decodeInstruction(0x8048345);
	expected = std::string("08048345    0 *32* tmp1 := r28\n"
	                       "            0 *32* r28 := r28 + 16\n"
	                       "            0 *v* %flags := ADDFLAGS32(tmp1, 16, r28)\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	inst = pFE->decodeInstruction(0x8048348);
	expected = std::string("08048348    0 *32* r24 := 0\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	inst = pFE->decodeInstruction(0x8048329);
	expected = std::string("08048329    0 *32* r29 := r28\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	delete prog;
}

void
FrontPentTest::test3()
{
	DecodeResult inst;
	std::string expected;

	auto prog = new Prog;
	auto pBF = BinaryFile::open(HELLO_PENT);
	if (!pBF)
		pBF = new BinaryFileStub();
	CPPUNIT_ASSERT(pBF);
	CPPUNIT_ASSERT(pBF->getMachine() == MACHINE_PENTIUM);
	auto pFE = FrontEnd::open(pBF, prog);

	inst = pFE->decodeInstruction(0x804834d);
	expected = std::string("0804834d    0 *32* r28 := r29\n"
	                       "            0 *32* r29 := m[r28]\n"
	                       "            0 *32* r28 := r28 + 4\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	inst = pFE->decodeInstruction(0x804834e);
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

	auto prog = new Prog;
	auto pBF = BinaryFile::open(BRANCH_PENT);
	if (!pBF)
		pBF = new BinaryFileStub();
	CPPUNIT_ASSERT(pBF);
	CPPUNIT_ASSERT(pBF->getMachine() == MACHINE_PENTIUM);
	auto pFE = FrontEnd::open(pBF, prog);

	// jne
	inst = pFE->decodeInstruction(0x8048979);
	expected = std::string("08048979    0 BRANCH 0x8048988, condition not equals\n"
	                       "High level: %flags\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	// jg
	inst = pFE->decodeInstruction(0x80489c1);
	expected = std::string("080489c1    0 BRANCH 0x80489d5, condition signed greater\n"
	                       "High level: %flags\n");
	CPPUNIT_ASSERT_EQUAL(expected, inst.rtl->prints());

	// jbe
	inst = pFE->decodeInstruction(0x8048a1b);
	expected = std::string("08048a1b    0 BRANCH 0x8048a2a, condition unsigned less or equals\n"
	                       "High level: %flags\n");
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
	auto prog = new Prog;
	auto pBF = BinaryFile::open(FEDORA2_TRUE);
	CPPUNIT_ASSERT(pBF);
	auto pFE = FrontEnd::open(pBF, prog);
	CPPUNIT_ASSERT(pFE);
	bool found;
	ADDRESS addr = pFE->getMainEntryPoint(found);
	ADDRESS expected = 0x8048b10;
	CPPUNIT_ASSERT_EQUAL(expected, addr);
	delete prog;

	prog = new Prog;
	pBF = BinaryFile::open(FEDORA3_TRUE);
	CPPUNIT_ASSERT(pBF);
	pFE = FrontEnd::open(pBF, prog);
	CPPUNIT_ASSERT(pFE);
	addr = pFE->getMainEntryPoint(found);
	expected = 0x8048c4a;
	CPPUNIT_ASSERT_EQUAL(expected, addr);
	delete prog;

	prog = new Prog;
	pBF = BinaryFile::open(SUSE_TRUE);
	CPPUNIT_ASSERT(pBF);
	pFE = FrontEnd::open(pBF, prog);
	CPPUNIT_ASSERT(pFE);
	addr = pFE->getMainEntryPoint(found);
	expected = 0x8048b60;
	CPPUNIT_ASSERT_EQUAL(expected, addr);
	delete prog;
}
