/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the LoaderTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LoaderTest.h"

#include "BinaryFile.h"

#include <iostream>     // For std::cout
#include <sstream>
#include <string>

#ifdef DYNAMIC
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>      // dlopen, dlsym
#endif
#define ELFBINFILE      MODPREFIX "ElfBinaryFile" MODSUFFIX
#endif

#ifndef TESTDIR
#define TESTDIR         "../test/"
#endif
#define HELLO_SPARC     TESTDIR "sparc/hello"
#define HELLO_PENTIUM   TESTDIR "pentium/hello"
#define HELLO_HPPA      TESTDIR "hppa/hello"
#define STARTER_PALM    TESTDIR "mc68328/Starter.prc"
#if 0 /* FIXME: these programs are proprietary */
#define CALC_WINDOWS    TESTDIR "windows/calc.exe"
#define CALC_WINXP      TESTDIR "windows/calcXP.exe"
#define CALC_WIN2000    TESTDIR "windows/calc2000.exe"
#define LPQ_WINDOWS     TESTDIR "windows/lpq.exe"
#endif
#define SWITCH_BORLAND  TESTDIR "windows/switch_borland.exe"

/**
 * Test loading the SPARC hello world program.
 */
void
LoaderTest::testSparcLoad()
{
	// Load SPARC hello world
	BinaryFile *bf = BinaryFile::open(HELLO_SPARC);
	CPPUNIT_ASSERT(bf != NULL);

	int n = bf->getNumSections();
	CPPUNIT_ASSERT_EQUAL(29, n);

	// Just use the first (real one) and last sections
	const SectionInfo *si;
	std::ostringstream actual;
	si = bf->getSectionInfo(1);
	actual << si->pSectionName << "\n";
	si = bf->getSectionInfo(n - 1);
	actual << si->pSectionName << "\n";
	std::string expected(".interp\n.stab.indexstr\n");
	CPPUNIT_ASSERT_EQUAL(expected, actual.str());

	BinaryFile::close(bf);
}

/**
 * Test loading the Pentium (Solaris) hello world program.
 */
void
LoaderTest::testPentiumLoad()
{
	// Load Pentium hello world
	BinaryFile *bf = BinaryFile::open(HELLO_PENTIUM);
	CPPUNIT_ASSERT(bf != NULL);

	int n = bf->getNumSections();
	CPPUNIT_ASSERT_EQUAL(34, n);

	const SectionInfo *si;
	std::ostringstream actual;
	si = bf->getSectionInfo(1);
	actual << si->pSectionName << "\n";
	si = bf->getSectionInfo(n - 1);
	actual << si->pSectionName << "\n";
	// (slightly different string to the sparc test, e.g. rel vs rela)
	std::string expected(".interp\n.strtab\n");
	CPPUNIT_ASSERT_EQUAL(expected, actual.str());

	BinaryFile::close(bf);
}

/**
 * Test loading the HPPA hello world program.
 */
void
LoaderTest::testHppaLoad()
{
	// Load HPPA hello world
	BinaryFile *bf = BinaryFile::open(HELLO_HPPA);
	CPPUNIT_ASSERT(bf != NULL);

	int n = bf->getNumSections();
	CPPUNIT_ASSERT_EQUAL(4, n);

	const SectionInfo *si;
	std::ostringstream actual;
	for (int i = 0; i < n; i++) {
		si = bf->getSectionInfo(i);
		actual << si->pSectionName << "\n";
	}
	std::string expected("$HEADER$\n$TEXT$\n$DATA$\n$BSS$\n");
	CPPUNIT_ASSERT_EQUAL(expected, actual.str());

	BinaryFile::close(bf);
}

/**
 * Test loading the Palm 68328 Starter.prc program.
 */
void
LoaderTest::testPalmLoad()
{
	// Load Palm Starter.prc
	BinaryFile *bf = BinaryFile::open(STARTER_PALM);
	CPPUNIT_ASSERT(bf != NULL);

	int n = bf->getNumSections();
	CPPUNIT_ASSERT_EQUAL(8, n);

	const SectionInfo *si;
	std::ostringstream actual;
	for (int i = 0; i < n; i++) {
		si = bf->getSectionInfo(i);
		actual << si->pSectionName << "\n";
	}
	std::string expected("code1\nMBAR1000\ntFRM1000\nTalt1001\n"
	                     "data0\ncode0\ntAIN1000\ntver1000\n");
	CPPUNIT_ASSERT_EQUAL(expected, actual.str());

	BinaryFile::close(bf);
}

/**
 * Test loading Windows programs.
 */
void
LoaderTest::testWinLoad()
{
#if 0 /* FIXME: these tests should use non-proprietary programs */
	{
		// Load Windows program calc.exe
		BinaryFile *bf = BinaryFile::open(CALC_WINDOWS);
		CPPUNIT_ASSERT(bf != NULL);

		int n = bf->getNumSections();
		CPPUNIT_ASSERT_EQUAL(5, n);

		{
			const SectionInfo *si;
			std::ostringstream actual;
			for (int i = 0; i < n; i++) {
				si = bf->getSectionInfo(i);
				actual << si->pSectionName << "\n";
			}
			std::string expected(".text\n.rdata\n.data\n.rsrc\n.reloc\n");
			CPPUNIT_ASSERT_EQUAL(expected, actual.str());
		}

		{
			ADDRESS addr = bf->getMainEntryPoint();
			CPPUNIT_ASSERT(addr != NO_ADDRESS);
		}

		{
			// Test symbol table (imports)
			const char *s = bf->getSymbolByAddress(0x1292060U);
			std::string actual;
			if (s == 0)
				actual = "<not found>";
			else
				actual = std::string(s);
			std::string expected("SetEvent");
			CPPUNIT_ASSERT_EQUAL(expected, actual);
		}

		{
			ADDRESS actual = bf->getAddressByName("SetEvent");
			ADDRESS expected = 0x1292060;
			CPPUNIT_ASSERT_EQUAL(expected, actual);
		}

		BinaryFile::close(bf);
	}

	{
		// Test loading the "new style" exes, as found in winXP etc
		BinaryFile *bf = BinaryFile::open(CALC_WINXP);
		CPPUNIT_ASSERT(bf != NULL);

		ADDRESS addr = bf->getMainEntryPoint();
		std::ostringstream actual;
		actual << std::hex << addr;
		std::string expected("1001f51");
		CPPUNIT_ASSERT_EQUAL(expected, actual.str());

		BinaryFile::close(bf);
	}

	{
		// Test loading the calc.exe found in Windows 2000 (more NT based)
		BinaryFile *bf = BinaryFile::open(CALC_WIN2000);
		CPPUNIT_ASSERT(bf != NULL);

		ADDRESS addr = bf->getMainEntryPoint();
		std::ostringstream actual;
		actual << std::hex << addr;
		std::string expected("1001680");
		CPPUNIT_ASSERT_EQUAL(expected, actual.str());

		BinaryFile::close(bf);
	}

	{
		// Test loading the lpq.exe program - console mode PE file
		BinaryFile *bf = BinaryFile::open(LPQ_WINDOWS);
		CPPUNIT_ASSERT(bf != NULL);

		ADDRESS addr = bf->getMainEntryPoint();
		std::ostringstream actual;
		actual << std::hex << addr;
		std::string expected("18c1000");
		CPPUNIT_ASSERT_EQUAL(expected, actual.str());

		BinaryFile::close(bf);
	}
#endif

	{
		// Borland
		BinaryFile *bf = BinaryFile::open(SWITCH_BORLAND);
		CPPUNIT_ASSERT(bf != NULL);

		ADDRESS addr = bf->getMainEntryPoint();
		std::ostringstream actual;
		actual << std::hex << addr;
		std::string expected("401150");
		CPPUNIT_ASSERT_EQUAL(expected, actual.str());

		BinaryFile::close(bf);
	}
}

extern "C" size_t microX86Dis(const unsigned char *p);

// The below lengths were derived from a quick and dirty program (called
// quick.c) which used the output from a disassembly to find the lengths.
// Best way to test, but of course this array is very dependent on the
// exact booked in test program
static const unsigned char lengths[] = {
	2, 2, 2, 1, 5, 2, 2, 5, 5, 3, 5, 2, 2, 5, 5, 5, 3, 4, 6, 1,
	3, 1, 1, 5, 5, 5, 3, 1, 5, 2, 5, 7, 1, 1, 1, 2, 1, 5, 1, 6,
	2, 1, 1, 3, 6, 2, 2, 6, 3, 2, 6, 1, 5, 3, 1, 1, 1, 1, 1, 1,
	2, 1, 5, 1, 6, 3, 1, 1, 1, 1, 1, 1, 2, 1, 5, 1, 6, 6, 1, 6,
	1, 5, 3, 1, 1, 1, 2, 1, 5, 1, 6, 3, 1, 1, /* main */ 2, 3, 1, 5, 5, 3,
	2, 2, 1, /* label */ 1, 2, 1, 2, 1, 1, 2, 2, 1, 1, 1, 3, 3, 1, 3, 2, 5, 2,
	2, 2, 2, 2, 3, 2, 3, 2, 3, 3, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1,
	3, 7, 3, 1, 1, 1, 3, 1, 2, 5, 2, 3, 3, 2, 2, 2, 3, 2, 6, 2,
	5, 2, 3, 3, 3, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1,
	3, 3, 3, 3, 2, 2, 3, 1, 2, 3, 3, 4, 3, 3, 3, 2, 2, 2, 2, 3,
	2, 3, 3, 4, 3, 1, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 2, 3,
	2, 1, 1, 1, 4, 2, 4, 2, 1, 2, 2, 3, 4, 2, 2, 1, 1, 1, 1, 1,
	2, 3, 1, 1, 1, 5, 1, 6, 3, 3, 2, 3, 2, 3, 3, 2, 3, 3, 2, 1,
	1, 4, 2, 4, 2, 1, 1, 1, 3, 5, 3, 3, 3, 2, 3, 3, 3, 2, 3, 2,
	2, 3, 4, 2, 3, 2, 3, 3, 2, 3, 3, 2, 3, 1, 1, 1, 1, 1, 1, 1,
	1, 2, 3, 1, 1, 1, 5, 1, 6, 3, 3, 2, 2, 2, 7, 3, 2, 1, 1, 1,
	2, 5, 3, 3, 3, 3, 2, 2, 1, 3, 3, 5, 3, 3, 3, 3, 3, 3, 1, 5,
	2, 7, 2, 3, 3, 3, 3, 3, 2, 2, 2, 3, 2, 3, 3, 1, 1, 3, 3, 1,
	3, 1, 1, 2, 5, 3, 3, 3, 2, 2, 3, 1, 3, 1, 3, 1, 1, 3, 3, 5,
	3, 3, 3, 2, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1,
	1, 5, 1, 6, 6, 2, 2, 1, 3, 2, 1, 5, 3, 3, 2, 2, 3, 2, 3, 2,
	2, 2, 2, 2, 1, 3, 2, 1, 1, 1, 2, 3, 3, 2, 2, 3, 1, 3, 3, 2,
	2, 3, 3, 3, 3, 2, 3, 2, 1, 1, 1, 3, 3, 3, 2, 3, 3, 2, 2, 3,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1, 1, 5, 1, 6, 3, 3, 5,
	2, 3, 3, 3, 2, 3, 6, 3, 5, 2, 1, 2, 2, 2, 3, 6, 5, 1, 2, 2,
	2, 4, 2, 2, 5, 1, 1, 1, 3, 2, 3, 2, 2, 2, 1, 5, 2, 2, 3, 4,
	3, 2, 1, 3, 3, 6, 3, 5, 1, 2, 2, 2, 3, 3, 3, 3, 3, 2, 1, 1,
	1, 3, 7, 3, 5, 1, 1, 5, 2, 3, 3, 1, 1, 5, 2, 3, 3, 3, 1, 2,
	3, 3, 2, 3, 1, 1, 5, 2, 3, 2, 3, 1, 1, 1, 1, 1, 1, 1, 2, 3,
	1, 1, 1, 5, 1, 6, 3, 3, 3, 3, 1, 3, 2, 6, 3, 2, 5, 4, 2, 5,
	1, 1, 2, 2, 5, 3, 3, 1, 3, 5, 3, 3, 4, 3, 3, 3, 5, 3, 7, 3,
	4, 5, 1, 1, 2, 2, 5, 3, 3, 3, 4, 5, 1, 5, 6, 2, 7, 2, 1, 1,
	2, 2, 2, 2, 6, 2, 3, 2, 2, 4, 3, 2, 2, 2, 1, 2, 6, 2, 3, 2,
	2, 2, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 3, 2, 2, 6, 2, 3, 3, 5, 1, 1, 3, 3, 2, 1, 3, 5, 1,
	1, 1, 3, 3, 2, 3, 3, 5, 1, 2, 3, 2, 2, 3, 3, 5, 1, 1, 1, 1,
	3, 1, 3, 5, 3, 3, 1, 3, 5, 3, 3, 4, 3, 3, 3, 5, 3, 7, 3, 4,
	5, 1, 1, 1, 3, 1, 3, 5, 3, 3, 3, 5, 5, 1, 3, 1, 3, 5, 3, 3,
	1, 3, 5, 3, 3, 3, 5, 3, 7, 3, 4, 5, 1, 3, 1, 3, 5, 3, 3, 1,
	3, 5, 3, 3, 3, 4, 3, 3, 5, 1, 3, 1, 3, 5, 3, 3, 3, 4, 5, 1,
	1, 3, 1, 3, 5, 3, 3, 3, 3, 5, 1, 1, 1, 2, 5, 2, 2, 3, 2, 1,
	5, 2, 3, 3, 2, 3, 3, 2, 2, 2, 1, 5, 2, 1, 5, 2, 7, 1, 3, 3,
	5, 3, 7, 4, 3, 3, 2, 5, 2, 2, 1, 1, 3, 1, 3, 5, 3, 3, 3, 3,
	2, 1, 1, 5, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 2, 1, 5, 1,
	6, 3, 3, 3, 7, 6, 7, 7, 6, 3, 6, 3, 1, 1, 1, 2, 1, 5, 1, 6,
	3, 3, 3, 3, 7, 6, 7, 6, 3, 6, 3, 1, 1, 1, 2, 1, 5, 1, 6, 3,
	6, 7, 2, 1, 1, 2, 3, 2, 3, 2, 3, 2, 3, 5, 2, 1, 3, 4, 2, 5,
	1, 1, 3, 1, 1, 1, 1, 1, 1, 2, 6, 1, 1, 1, 5, 1, 6, 3, 5, 6,
	3, 2, 6, 3, 6, 1, 6, 5, 2, 3, 2, 6, 2, 2, 6, 6, 1, 5, 3, 4,
	3, 6, 3, 6, 3, 5, 2, 2, 2, 3, 2, 2, 6, 6, 6, 6, 1, 2, 6, 6,
	1, 5, 2, 3, 2, 2, 6, 3, 3, 3, 2, 6, 1, 1, 5, 2, 6, 3, 6, 2,
	3, 6, 3, 6, 2, 2, 6, 6, 3, 6, 2, 6, 3, 1, 6, 1, 1, 5, 2, 3,
	2, 2, 3, 6, 1, 5, 2, 3, 6, 1, 1, 1, 1, /* label */ 1, 2, 1, 2, 1, 1, 5, 1,
	6, 6, 3, 4, 2, 2, 2, 3, 3, 2, 3, 1, 1, 1, 1, 1, 1, 2, 1, 5,
	1, 6, 3, 1, 1
};

// text segment of hello pentium
static const unsigned char pent_hello_text[] = {
	0x6a, 0x00, 0x6a, 0x00, 0x8b, 0xec, 0x52, 0xb8, 0x80, 0x87, 0x04, 0x08, 0x85, 0xc0, 0x74, 0x0d,
	0x68, 0x80, 0x87, 0x04, 0x08, 0xe8, 0x66, 0xff, 0xff, 0xff, 0x83, 0xc4, 0x04, 0xb8, 0x44, 0xa4,
	0x04, 0x08, 0x85, 0xc0, 0x74, 0x05, 0xe8, 0x55, 0xff, 0xff, 0xff, 0x68, 0xe0, 0x93, 0x04, 0x08,
	0xe8, 0x4b, 0xff, 0xff, 0xff, 0x8b, 0x45, 0x08, 0x8d, 0x54, 0x85, 0x10, 0x89, 0x15, 0x0c, 0xaa,
	0x04, 0x08, 0x52, 0x8d, 0x55, 0x0c, 0x52, 0x50, 0xe8, 0x53, 0x0b, 0x00, 0x00, 0xe8, 0x3e, 0xff,
	0xff, 0xff, 0xe8, 0xb1, 0x00, 0x00, 0x00, 0x83, 0xc4, 0x0c, 0x50, 0xe8, 0x40, 0xff, 0xff, 0xff,
	0x6a, 0x00, 0xb8, 0x01, 0x00, 0x00, 0x00, 0x9a, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0xf4, 0xc3,
	0x55, 0x8b, 0xec, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0x8b, 0x1b, 0x00, 0x00,
	0xeb, 0x0f, 0x90, 0x90, 0x8d, 0x50, 0x04, 0x89, 0x93, 0xc8, 0x00, 0x00, 0x00, 0x8b, 0x00, 0xff,
	0xd0, 0x8b, 0x83, 0xc8, 0x00, 0x00, 0x00, 0x83, 0x38, 0x00, 0x75, 0xe8, 0x8d, 0x83, 0xdc, 0x00,
	0x00, 0x00, 0x50, 0xe8, 0xe8, 0x08, 0x00, 0x00, 0x8b, 0x5d, 0xfc, 0xc9, 0xc3, 0x90, 0x90, 0x90,
	0x55, 0x8b, 0xec, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0x4b, 0x1b, 0x00, 0x00,
	0x8b, 0x5d, 0xfc, 0xc9, 0xc3, 0x90, 0x90, 0x90, 0x55, 0x8b, 0xec, 0x53, 0xe8, 0x00, 0x00, 0x00,
	0x00, 0x5b, 0x81, 0xc3, 0x33, 0x1b, 0x00, 0x00, 0x8d, 0x83, 0xdc, 0x05, 0x00, 0x00, 0x50, 0x8d,
	0x83, 0xdc, 0x00, 0x00, 0x00, 0x50, 0xe8, 0x19, 0x08, 0x00, 0x00, 0x8b, 0x5d, 0xfc, 0xc9, 0xc3,
	0x55, 0x8b, 0xec, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0x0b, 0x1b, 0x00, 0x00,
	0x8b, 0x5d, 0xfc, 0xc9, 0xc3, 0x00, 0x00, 0x00, 0x55, 0x8b, 0xec, 0x68, 0xf8, 0x93, 0x04, 0x08,
	0xe8, 0x9b, 0xfe, 0xff, 0xff, 0x83, 0xc4, 0x04, 0x33, 0xc0, 0xeb, 0x00, 0xc9, 0xc3, 0x00, 0x00,
	0x55, 0x8b, 0xec, 0x57, 0x56, 0x33, 0xff, 0x8b, 0xf7, 0x90, 0x90, 0x90, 0x8b, 0x4d, 0x08, 0x0f,
	0xb6, 0x11, 0x41, 0x89, 0x4d, 0x08, 0x8b, 0xc2, 0x25, 0x7f, 0x00, 0x00, 0x00, 0x8b, 0xce, 0xd3,
	0xe0, 0x0b, 0xf8, 0x84, 0xd2, 0x7d, 0x05, 0x83, 0xc6, 0x07, 0xeb, 0xe0, 0x8b, 0x45, 0x0c, 0x89,
	0x38, 0x8b, 0x45, 0x08, 0x8d, 0x65, 0xf8, 0x5e, 0x5f, 0xc9, 0xc3, 0x90, 0x55, 0x8b, 0xec, 0x83,
	0xec, 0x04, 0x57, 0x56, 0x8b, 0x7d, 0x08, 0xc7, 0x45, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x4d,
	0xfc, 0x90, 0x90, 0x90, 0x0f, 0xb6, 0x37, 0x47, 0x8b, 0xc6, 0x25, 0x7f, 0x00, 0x00, 0x00, 0xd3,
	0xe0, 0x09, 0x45, 0xfc, 0x83, 0xc1, 0x07, 0x8b, 0xd6, 0x84, 0xd2, 0x7c, 0xe7, 0x83, 0xf9, 0x1f,
	0x77, 0x12, 0xf7, 0xc6, 0x40, 0x00, 0x00, 0x00, 0x74, 0x0a, 0xb8, 0xff, 0xff, 0xff, 0xff, 0xd3,
	0xe0, 0x09, 0x45, 0xfc, 0x8b, 0x45, 0x0c, 0x8b, 0x55, 0xfc, 0x89, 0x10, 0x8b, 0xc7, 0x8d, 0x65,
	0xf4, 0x5e, 0x5f, 0xc9, 0xc3, 0x90, 0x90, 0x90, 0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x57, 0x56,
	0x8b, 0x55, 0x0c, 0x8b, 0x45, 0x10, 0x8b, 0x75, 0x08, 0x89, 0x04, 0x96, 0x85, 0xd2, 0x74, 0x36,
	0x8d, 0x0c, 0x96, 0x90, 0x8b, 0x39, 0x89, 0x7d, 0xfc, 0x8b, 0x75, 0x08, 0x8b, 0x74, 0x96, 0xfc,
	0x89, 0x75, 0xf8, 0x8b, 0x46, 0x08, 0x8b, 0x77, 0x08, 0x2b, 0xf0, 0x8b, 0xc6, 0x85, 0xc0, 0x7d,
	0x15, 0x8b, 0x7d, 0xf8, 0x89, 0x39, 0x8b, 0x7d, 0xfc, 0x8b, 0x75, 0x08, 0x89, 0x7c, 0x96, 0xfc,
	0x83, 0xc1, 0xfc, 0x4a, 0x75, 0xce, 0x8d, 0x65, 0xf0, 0x5e, 0x5f, 0xc9, 0xc3, 0x90, 0x90, 0x90,
	0x55, 0x8b, 0xec, 0x8b, 0x55, 0x08, 0x33, 0xc9, 0x83, 0x3a, 0x00, 0x74, 0x1d, 0x90, 0x90, 0x90,
	0x83, 0x7a, 0x04, 0x00, 0x74, 0x07, 0x83, 0x7a, 0x08, 0x00, 0x74, 0x01, 0x41, 0x8b, 0xc2, 0x03,
	0x02, 0x8d, 0x50, 0x04, 0x83, 0x78, 0x04, 0x00, 0x75, 0xe6, 0x8b, 0xc1, 0xc9, 0xc3, 0x90, 0x90,
	0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x57, 0x56, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81,
	0xc3, 0xb6, 0x19, 0x00, 0x00, 0x8b, 0x75, 0x08, 0x8b, 0x55, 0x10, 0x8b, 0x3a, 0x8b, 0x4d, 0x14,
	0x8b, 0x09, 0x89, 0x4d, 0xfc, 0x8b, 0x55, 0x18, 0x8b, 0x12, 0x89, 0x55, 0xf8, 0x83, 0x3e, 0x00,
	0x74, 0x3f, 0x90, 0x90, 0x83, 0x7e, 0x04, 0x00, 0x74, 0x2a, 0x83, 0x7e, 0x08, 0x00, 0x74, 0x24,
	0x56, 0x57, 0x47, 0xff, 0x75, 0x0c, 0xe8, 0x2d, 0xff, 0xff, 0xff, 0x8b, 0x46, 0x08, 0x83, 0xc4,
	0x0c, 0x39, 0x45, 0xfc, 0x76, 0x03, 0x89, 0x45, 0xfc, 0x03, 0x46, 0x0c, 0x39, 0x45, 0xf8, 0x73,
	0x03, 0x89, 0x45, 0xf8, 0x8b, 0xc6, 0x03, 0x06, 0x8d, 0x70, 0x04, 0x83, 0x78, 0x04, 0x00, 0x75,
	0xc3, 0x8b, 0x4d, 0x10, 0x89, 0x39, 0x8b, 0x4d, 0xfc, 0x8b, 0x55, 0x14, 0x89, 0x0a, 0x8b, 0x4d,
	0xf8, 0x8b, 0x55, 0x18, 0x89, 0x0a, 0x8d, 0x65, 0xec, 0x5b, 0x5e, 0x5f, 0xc9, 0xc3, 0x90, 0x90,
	0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x57, 0x56, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81,
	0xc3, 0x26, 0x19, 0x00, 0x00, 0x8b, 0x55, 0x08, 0x8b, 0x42, 0x0c, 0x85, 0xc0, 0x74, 0x29, 0x8b,
	0xf0, 0xc7, 0x45, 0xf4, 0x00, 0x00, 0x00, 0x00, 0x83, 0x3e, 0x00, 0x74, 0x2c, 0x90, 0x90, 0x90,
	0xff, 0x36, 0xe8, 0x09, 0xff, 0xff, 0xff, 0x01, 0x45, 0xf4, 0x83, 0xc4, 0x04, 0x83, 0xc6, 0x04,
	0x83, 0x3e, 0x00, 0x75, 0xeb, 0xeb, 0x12, 0x90, 0x8b, 0x55, 0x08, 0xff, 0x72, 0x08, 0xe8, 0xed,
	0xfe, 0xff, 0xff, 0x89, 0x45, 0xf4, 0x83, 0xc4, 0x04, 0x8b, 0x45, 0xf4, 0x8b, 0x55, 0x08, 0x89,
	0x42, 0x10, 0xc1, 0xe0, 0x02, 0x50, 0xe8, 0x85, 0xfc, 0xff, 0xff, 0x8b, 0xf8, 0xc7, 0x45, 0xf8,
	0xff, 0xff, 0xff, 0xff, 0x33, 0xc0, 0x89, 0x45, 0xfc, 0x89, 0x45, 0xf4, 0x83, 0xc4, 0x04, 0x8b,
	0x55, 0x08, 0x8b, 0x42, 0x0c, 0x85, 0xc0, 0x74, 0x2f, 0x8b, 0xf0, 0x83, 0x3e, 0x00, 0x74, 0x40,
	0x8d, 0x55, 0xfc, 0x89, 0x55, 0xf0, 0x90, 0x90, 0xff, 0x75, 0xf0, 0x8d, 0x45, 0xf8, 0x50, 0x8d,
	0x45, 0xf4, 0x50, 0x57, 0xff, 0x36, 0xe8, 0xc5, 0xfe, 0xff, 0xff, 0x83, 0xc4, 0x14, 0x83, 0xc6,
	0x04, 0x83, 0x3e, 0x00, 0x75, 0xe2, 0xeb, 0x18, 0x8d, 0x45, 0xfc, 0x50, 0x8d, 0x45, 0xf8, 0x50,
	0x8d, 0x45, 0xf4, 0x50, 0x57, 0x8b, 0x55, 0x08, 0xff, 0x72, 0x08, 0xe8, 0xa0, 0xfe, 0xff, 0xff,
	0x8b, 0x55, 0x08, 0x89, 0x7a, 0x0c, 0x8b, 0x45, 0xf8, 0x89, 0x02, 0x8b, 0x45, 0xfc, 0x89, 0x42,
	0x04, 0x8d, 0x65, 0xe4, 0x5b, 0x5e, 0x5f, 0xc9, 0xc3, 0x90, 0x90, 0x90, 0x55, 0x8b, 0xec, 0x83,
	0xec, 0x08, 0x57, 0x56, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0x3a, 0x18, 0x00,
	0x00, 0x8b, 0xb3, 0xf4, 0x05, 0x00, 0x00, 0x85, 0xf6, 0x74, 0x74, 0x90, 0x83, 0x3e, 0x00, 0x75,
	0x09, 0x56, 0xe8, 0xe9, 0xfe, 0xff, 0xff, 0x83, 0xc4, 0x04, 0x8b, 0x4d, 0x08, 0x39, 0x0e, 0x77,
	0x05, 0x39, 0x4e, 0x04, 0x77, 0x07, 0x8b, 0x76, 0x14, 0x85, 0xf6, 0x75, 0xdf, 0x85, 0xf6, 0x75,
	0x0b, 0xeb, 0x4c, 0x90, 0x8b, 0x45, 0xf8, 0xeb, 0x48, 0x90, 0x90, 0x90, 0x33, 0xff, 0x8b, 0x4e,
	0x10, 0x89, 0x4d, 0xfc, 0x3b, 0xf9, 0x73, 0x37, 0x8b, 0x76, 0x0c, 0x90, 0x8b, 0x4d, 0xfc, 0x8d,
	0x04, 0x39, 0x8b, 0xd0, 0xd1, 0xea, 0x8b, 0x0c, 0x96, 0x89, 0x4d, 0xf8, 0x8b, 0x41, 0x08, 0x39,
	0x45, 0x08, 0x73, 0x08, 0x89, 0x55, 0xfc, 0xeb, 0x11, 0x90, 0x90, 0x90, 0x8b, 0x4d, 0xf8, 0x03,
	0x41, 0x0c, 0x39, 0x45, 0x08, 0x76, 0xbd, 0x8d, 0x7a, 0x01, 0x39, 0x7d, 0xfc, 0x77, 0xcd, 0x33,
	0xc0, 0x8d, 0x65, 0xec, 0x5b, 0x5e, 0x5f, 0xc9, 0xc3, 0x90, 0x90, 0x90, 0x55, 0x8b, 0xec, 0x83,
	0xec, 0x18, 0x57, 0x56, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0x9a, 0x17, 0x00,
	0x00, 0x8b, 0x55, 0x08, 0x8b, 0x42, 0x04, 0x05, 0xfc, 0xff, 0xff, 0xff, 0x2b, 0xd0, 0x8d, 0x72,
	0x09, 0x89, 0x75, 0xf4, 0x8b, 0x7d, 0x0c, 0x89, 0x37, 0x89, 0x75, 0xf8, 0x8d, 0x83, 0xf9, 0xef,
	0xff, 0xff, 0x89, 0x45, 0xf0, 0xb9, 0x01, 0x00, 0x00, 0x00, 0x8b, 0xf8, 0xfc, 0xa8, 0x00, 0xf3,
	0xa6, 0x74, 0x25, 0x8b, 0x75, 0xf4, 0x8d, 0xbb, 0xfa, 0xef, 0xff, 0xff, 0xb9, 0x03, 0x00, 0x00,
	0x00, 0xfc, 0xa8, 0x00, 0xf3, 0xa6, 0x74, 0x10, 0x80, 0x7a, 0x09, 0x7a, 0x74, 0x0a, 0x33, 0xc0,
	0xe9, 0xa6, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x8b, 0x45, 0x0c, 0x8b, 0x00, 0x89, 0x45, 0xf0,
	0x8b, 0xd0, 0x8b, 0xfa, 0x33, 0xc0, 0xfc, 0xb9, 0xff, 0xff, 0xff, 0xff, 0xf2, 0xae, 0xf7, 0xd1,
	0x89, 0x4d, 0xf8, 0x8d, 0x44, 0x0a, 0xff, 0x89, 0x45, 0xf4, 0x8b, 0xd0, 0x42, 0x8b, 0x75, 0xf0,
	0x89, 0x75, 0xf8, 0x8d, 0xbb, 0xfa, 0xef, 0xff, 0xff, 0x89, 0x7d, 0xf0, 0xb9, 0x03, 0x00, 0x00,
	0x00, 0xfc, 0xa8, 0x00, 0xf3, 0xa6, 0x75, 0x14, 0x8b, 0x40, 0x01, 0x8b, 0x75, 0x0c, 0x89, 0x46,
	0x04, 0x8b, 0x55, 0xf4, 0x83, 0xc2, 0x05, 0xeb, 0x0d, 0x90, 0x90, 0x90, 0x8b, 0x7d, 0x0c, 0xc7,
	0x47, 0x04, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x45, 0x0c, 0x05, 0x08, 0x00, 0x00, 0x00, 0x50, 0x52,
	0xe8, 0xeb, 0xfb, 0xff, 0xff, 0x8b, 0xd0, 0x8b, 0x75, 0x0c, 0x83, 0xc6, 0x0c, 0x56, 0x52, 0xe8,
	0x18, 0xfc, 0xff, 0xff, 0x8b, 0xd0, 0x0f, 0xb6, 0x02, 0x8b, 0x75, 0x0c, 0x89, 0x46, 0x10, 0x42,
	0x8b, 0x3e, 0x83, 0xc4, 0x10, 0x80, 0x3f, 0x7a, 0x75, 0x0f, 0x8d, 0x45, 0xfc, 0x50, 0x52, 0xe8,
	0xbc, 0xfb, 0xff, 0xff, 0x8b, 0xd0, 0x03, 0x55, 0xfc, 0x8b, 0xc2, 0x8d, 0x65, 0xdc, 0x5b, 0x5e,
	0x5f, 0xc9, 0xc3, 0x90, 0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x57, 0x56, 0x53, 0xe8, 0x00, 0x00,
	0x00, 0x00, 0x5b, 0x81, 0xc3, 0x82, 0x16, 0x00, 0x00, 0x8b, 0x7d, 0x10, 0x8b, 0x55, 0x14, 0x8b,
	0x4d, 0x08, 0x0f, 0xb6, 0x01, 0x41, 0x89, 0x4d, 0x08, 0x8b, 0xf0, 0x81, 0xe6, 0x40, 0x00, 0x00,
	0x00, 0x89, 0x75, 0xf0, 0x74, 0x12, 0x25, 0x3f, 0x00, 0x00, 0x00, 0x0f, 0xaf, 0x47, 0x08, 0x01,
	0x02, 0xe9, 0x42, 0x03, 0x00, 0x00, 0x90, 0x90, 0x84, 0xc0, 0x7d, 0x44, 0x25, 0x3f, 0x00, 0x00,
	0x00, 0x89, 0x45, 0xf8, 0x8d, 0x45, 0xfc, 0x50, 0xff, 0x75, 0x08, 0xe8, 0x50, 0xfb, 0xff, 0xff,
	0x89, 0x45, 0x08, 0x8b, 0x45, 0xfc, 0x0f, 0xaf, 0x47, 0x0c, 0x89, 0x45, 0xfc, 0x8b, 0x45, 0xf8,
	0x8b, 0x4d, 0x0c, 0xc6, 0x44, 0x08, 0x5c, 0x01, 0x8b, 0x75, 0xf8, 0x8d, 0x14, 0xb5, 0x00, 0x00,
	0x00, 0x00, 0x8b, 0x45, 0xfc, 0x89, 0x44, 0x0a, 0x10, 0xe9, 0xfa, 0x02, 0x00, 0x00, 0x90, 0x90,
	0xa8, 0xc0, 0x74, 0x18, 0x25, 0x3f, 0x00, 0x00, 0x00, 0x89, 0x45, 0xf8, 0x8a, 0x4d, 0xf0, 0x8b,
	0x75, 0x0c, 0x88, 0x4c, 0x30, 0x5c, 0xe9, 0xdd, 0x02, 0x00, 0x00, 0x90, 0x3d, 0x2e, 0x00, 0x00,
	0x00, 0x0f, 0x87, 0xc9, 0x02, 0x00, 0x00, 0x8b, 0xcb, 0x2b, 0x8c, 0x83, 0x30, 0xea, 0xff, 0xff,
	0xff, 0xe1, 0x90, 0x90, 0x0c, 0x13, 0x00, 0x00, 0x14, 0x15, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00,
	0xec, 0x14, 0x00, 0x00, 0xd8, 0x14, 0x00, 0x00, 0xc0, 0x14, 0x00, 0x00, 0x74, 0x14, 0x00, 0x00,
	0x0c, 0x13, 0x00, 0x00, 0x0c, 0x13, 0x00, 0x00, 0x54, 0x14, 0x00, 0x00, 0xa0, 0x13, 0x00, 0x00,
	0x80, 0x13, 0x00, 0x00, 0x14, 0x14, 0x00, 0x00, 0xe0, 0x13, 0x00, 0x00, 0xc0, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00,
	0x14, 0x13, 0x00, 0x00, 0x14, 0x13, 0x00, 0x00, 0x64, 0x13, 0x00, 0x00, 0x30, 0x13, 0x00, 0x00,
	0x8b, 0x75, 0x08, 0x8b, 0x06, 0x89, 0x02, 0x83, 0xc6, 0x04, 0x89, 0x75, 0x08, 0xe9, 0xf6, 0x01,
	0x00, 0x00, 0x90, 0x90, 0x8b, 0x4d, 0x08, 0x0f, 0xb6, 0x01, 0x01, 0x02, 0x41, 0x89, 0x4d, 0x08,
	0xe9, 0xe3, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90, 0x8b, 0x75, 0x08, 0x0f, 0xb7, 0x06, 0x01, 0x02,
	0x83, 0xc6, 0x02, 0x89, 0x75, 0x08, 0xe9, 0xcd, 0x01, 0x00, 0x00, 0x90, 0x8b, 0x02, 0x8b, 0x4d,
	0x08, 0x03, 0x01, 0x89, 0x02, 0x83, 0xc1, 0x04, 0x89, 0x4d, 0x08, 0xe9, 0xb8, 0x01, 0x00, 0x00,
	0x90, 0x90, 0x90, 0x90, 0x8d, 0x45, 0xf8, 0x50, 0xff, 0x75, 0x08, 0xe8, 0xd0, 0xf9, 0xff, 0xff,
	0x89, 0x45, 0x08, 0x8d, 0x45, 0xfc, 0x50, 0xff, 0x75, 0x08, 0xe8, 0xc1, 0xf9, 0xff, 0xff, 0x89,
	0x45, 0x08, 0x8b, 0x45, 0xfc, 0x0f, 0xaf, 0x47, 0x0c, 0x89, 0x45, 0xfc, 0x8b, 0x45, 0xf8, 0x8b,
	0x75, 0x0c, 0xc6, 0x44, 0x30, 0x5c, 0x01, 0x8b, 0x4d, 0xf8, 0x8d, 0x14, 0x8d, 0x00, 0x00, 0x00,
	0x00, 0x8b, 0x45, 0xfc, 0x89, 0x44, 0x32, 0x10, 0xe9, 0x6b, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90,
	0x8d, 0x45, 0xf8, 0x50, 0xff, 0x75, 0x08, 0xe8, 0x84, 0xf9, 0xff, 0xff, 0x89, 0x45, 0x08, 0x8b,
	0x45, 0xf8, 0x8b, 0x75, 0x0c, 0xc6, 0x44, 0x30, 0x5c, 0x00, 0xe9, 0x49, 0x01, 0x00, 0x00, 0x90,
	0x8d, 0x45, 0xf8, 0x50, 0xff, 0x75, 0x08, 0xe8, 0x64, 0xf9, 0xff, 0xff, 0x89, 0x45, 0x08, 0x8d,
	0x45, 0xf4, 0x50, 0xff, 0x75, 0x08, 0xe8, 0x55, 0xf9, 0xff, 0xff, 0x89, 0x45, 0x08, 0x8b, 0x45,
	0xf8, 0x8b, 0x4d, 0x0c, 0xc6, 0x44, 0x08, 0x5c, 0x02, 0x8b, 0x75, 0xf8, 0x8d, 0x14, 0xb5, 0x00,
	0x00, 0x00, 0x00, 0x8b, 0x45, 0xf4, 0x89, 0x44, 0x0a, 0x10, 0xe9, 0x09, 0x01, 0x00, 0x00, 0x90,
	0x8d, 0x45, 0xf8, 0x50, 0xff, 0x75, 0x08, 0xe8, 0x24, 0xf9, 0xff, 0xff, 0x89, 0x45, 0x08, 0x8d,
	0x45, 0xfc, 0x50, 0xff, 0x75, 0x08, 0xe8, 0x15, 0xf9, 0xff, 0xff, 0x89, 0x45, 0x08, 0x8b, 0x45,
	0xf8, 0x8b, 0x4d, 0x0c, 0x66, 0x89, 0x41, 0x58, 0x8b, 0x45, 0xfc, 0x89, 0x41, 0x08, 0xe9, 0xd5,
	0x00, 0x00, 0x00, 0x90, 0x8d, 0x45, 0xf8, 0x50, 0xff, 0x75, 0x08, 0xe8, 0xf0, 0xf8, 0xff, 0xff,
	0x89, 0x45, 0x08, 0x8b, 0x45, 0xf8, 0x8b, 0x75, 0x0c, 0x66, 0x89, 0x46, 0x58, 0xe9, 0xb6, 0x00,
	0x00, 0x00, 0x90, 0x90, 0x8d, 0x45, 0xfc, 0x50, 0xff, 0x75, 0x08, 0xe8, 0xd0, 0xf8, 0xff, 0xff,
	0x89, 0x45, 0x08, 0x8b, 0x45, 0xfc, 0x8b, 0x4d, 0x0c, 0x89, 0x41, 0x08, 0xe9, 0x97, 0x00, 0x00,
	0x00, 0x90, 0x90, 0x90, 0x6a, 0x74, 0xe8, 0x55, 0xf7, 0xff, 0xff, 0x8b, 0xd0, 0x8b, 0xfa, 0x8b,
	0x45, 0x0c, 0x8b, 0xf0, 0xfc, 0xb9, 0x1d, 0x00, 0x00, 0x00, 0xf3, 0xa5, 0x8b, 0x4d, 0x0c, 0x89,
	0x51, 0x70, 0xeb, 0x74, 0x8b, 0x75, 0x0c, 0x8b, 0x56, 0x70, 0x8b, 0xfe, 0x8b, 0xc2, 0x8b, 0xf0,
	0xfc, 0xb9, 0x1d, 0x00, 0x00, 0x00, 0xf3, 0xa5, 0x52, 0xe8, 0x32, 0xf7, 0xff, 0xff, 0xeb, 0x58,
	0xc7, 0x45, 0xf8, 0x10, 0x00, 0x00, 0x00, 0x90, 0x8b, 0x45, 0xf8, 0x8b, 0x4d, 0x0c, 0xc6, 0x44,
	0x08, 0x5c, 0x01, 0x8b, 0x45, 0xf8, 0x8d, 0x14, 0x85, 0xc0, 0xff, 0xff, 0xff, 0x89, 0x54, 0x81,
	0x10, 0x8d, 0x70, 0x01, 0x89, 0x75, 0xf8, 0x8b, 0xc6, 0x3d, 0x1f, 0x00, 0x00, 0x00, 0x76, 0xd8,
	0xeb, 0x26, 0x90, 0x90, 0x8d, 0x45, 0xfc, 0x50, 0xff, 0x75, 0x08, 0xe8, 0x40, 0xf8, 0xff, 0xff,
	0x89, 0x45, 0x08, 0x8b, 0x45, 0xfc, 0x8b, 0x4d, 0x0c, 0x89, 0x41, 0x0c, 0xeb, 0x0a, 0x90, 0x90,
	0xe8, 0xeb, 0xf6, 0xff, 0xff, 0x90, 0x90, 0x90, 0x8b, 0x45, 0x08, 0x8d, 0x65, 0xe4, 0x5b, 0x5e,
	0x5f, 0xc9, 0xc3, 0x90, 0x55, 0x8b, 0xec, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3,
	0xf7, 0x12, 0x00, 0x00, 0x8b, 0x45, 0x08, 0x8b, 0x55, 0x0c, 0x89, 0x42, 0x08, 0xc7, 0x42, 0x04,
	0x00, 0x00, 0x00, 0x00, 0xc7, 0x02, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x42, 0x0c, 0x00, 0x00, 0x00,
	0x00, 0xc7, 0x42, 0x10, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x83, 0xf4, 0x05, 0x00, 0x00, 0x89, 0x42,
	0x14, 0x89, 0x93, 0xf4, 0x05, 0x00, 0x00, 0x8b, 0x5d, 0xfc, 0xc9, 0xc3, 0x55, 0x8b, 0xec, 0x53,
	0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0xaf, 0x12, 0x00, 0x00, 0x8b, 0x45, 0x08, 0x8b,
	0x55, 0x0c, 0x89, 0x42, 0x08, 0x89, 0x42, 0x0c, 0xc7, 0x42, 0x04, 0x00, 0x00, 0x00, 0x00, 0xc7,
	0x02, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x42, 0x10, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x83, 0xf4, 0x05,
	0x00, 0x00, 0x89, 0x42, 0x14, 0x89, 0x93, 0xf4, 0x05, 0x00, 0x00, 0x8b, 0x5d, 0xfc, 0xc9, 0xc3,
	0x55, 0x8b, 0xec, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0x6b, 0x12, 0x00, 0x00,
	0x8b, 0x45, 0x08, 0x8d, 0x8b, 0xf4, 0x05, 0x00, 0x00, 0x83, 0xbb, 0xf4, 0x05, 0x00, 0x00, 0x00,
	0x74, 0x27, 0x90, 0x90, 0x8b, 0x11, 0x39, 0x42, 0x08, 0x75, 0x15, 0x8b, 0x42, 0x14, 0x89, 0x01,
	0x83, 0x3a, 0x00, 0x74, 0x1b, 0xff, 0x72, 0x0c, 0xe8, 0x03, 0xf6, 0xff, 0xff, 0xeb, 0x11, 0x90,
	0x8d, 0x4a, 0x14, 0x83, 0x7a, 0x14, 0x00, 0x75, 0xdb, 0xe8, 0x02, 0xf6, 0xff, 0xff, 0x90, 0x90,
	0x8b, 0x5d, 0xfc, 0xc9, 0xc3, 0x90, 0x90, 0x90, 0x55, 0x8b, 0xec, 0x81, 0xec, 0xa8, 0x00, 0x00,
	0x00, 0x57, 0x56, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81, 0xc3, 0x0b, 0x12, 0x00, 0x00,
	0xff, 0x75, 0x08, 0xe8, 0xb4, 0xf9, 0xff, 0xff, 0x89, 0x85, 0x6c, 0xff, 0xff, 0xff, 0x83, 0xc4,
	0x04, 0x85, 0xc0, 0x0f, 0x84, 0x2e, 0x01, 0x00, 0x00, 0x8d, 0x4d, 0xec, 0x89, 0x8d, 0x68, 0xff,
	0xff, 0xff, 0x51, 0xff, 0xb5, 0x6c, 0xff, 0xff, 0xff, 0xe8, 0x2e, 0xfa, 0xff, 0xff, 0x8b, 0xf0,
	0x83, 0xc4, 0x08, 0x85, 0xf6, 0x0f, 0x84, 0x0c, 0x01, 0x00, 0x00, 0x6a, 0x74, 0x6a, 0x00, 0x8d,
	0x8d, 0x78, 0xff, 0xff, 0xff, 0x89, 0x8d, 0x64, 0xff, 0xff, 0xff, 0x51, 0xe8, 0x9f, 0xf5, 0xff,
	0xff, 0x8b, 0x45, 0xfc, 0x66, 0x89, 0x45, 0xd2, 0x8b, 0x45, 0xf0, 0x89, 0x85, 0x7c, 0xff, 0xff,
	0xff, 0x83, 0xc4, 0x0c, 0x8b, 0x8d, 0x6c, 0xff, 0xff, 0xff, 0x8b, 0x41, 0x04, 0x05, 0xfc, 0xff,
	0xff, 0xff, 0x2b, 0xc8, 0x8b, 0xc1, 0x03, 0x00, 0x8d, 0x78, 0x04, 0x3b, 0xf7, 0x73, 0x36, 0x8b,
	0x8d, 0x68, 0xff, 0xff, 0xff, 0x89, 0x8d, 0x60, 0xff, 0xff, 0xff, 0x8b, 0x8d, 0x64, 0xff, 0xff,
	0xff, 0x89, 0x8d, 0x5c, 0xff, 0xff, 0xff, 0x90, 0x6a, 0x00, 0xff, 0xb5, 0x60, 0xff, 0xff, 0xff,
	0xff, 0xb5, 0x5c, 0xff, 0xff, 0xff, 0x56, 0xe8, 0xc8, 0xfa, 0xff, 0xff, 0x8b, 0xf0, 0x83, 0xc4,
	0x10, 0x3b, 0xf7, 0x72, 0xe3, 0x8b, 0xb5, 0x6c, 0xff, 0xff, 0xff, 0x83, 0xc6, 0x10, 0x8b, 0x45,
	0xec, 0x80, 0x38, 0x7a, 0x75, 0x18, 0x8d, 0x85, 0x74, 0xff, 0xff, 0xff, 0x50, 0x56, 0xe8, 0x4d,
	0xf6, 0xff, 0xff, 0x8b, 0xf0, 0x03, 0xb5, 0x74, 0xff, 0xff, 0xff, 0x83, 0xc4, 0x08, 0x8b, 0x85,
	0x6c, 0xff, 0xff, 0xff, 0x03, 0x00, 0x8d, 0x78, 0x04, 0x8b, 0x8d, 0x6c, 0xff, 0xff, 0xff, 0x8b,
	0x41, 0x08, 0x89, 0x85, 0x70, 0xff, 0xff, 0xff, 0x3b, 0xf7, 0x73, 0x37, 0x8d, 0x8d, 0x70, 0xff,
	0xff, 0xff, 0x89, 0x8d, 0x58, 0xff, 0xff, 0xff, 0x8b, 0x4d, 0x08, 0x39, 0x8d, 0x70, 0xff, 0xff,
	0xff, 0x77, 0x20, 0xff, 0xb5, 0x58, 0xff, 0xff, 0xff, 0x8d, 0x45, 0xec, 0x50, 0x8d, 0x85, 0x78,
	0xff, 0xff, 0xff, 0x50, 0x56, 0xe8, 0x4a, 0xfa, 0xff, 0xff, 0x8b, 0xf0, 0x83, 0xc4, 0x10, 0x3b,
	0xf7, 0x72, 0xd5, 0x8b, 0x7d, 0x0c, 0x8d, 0xb5, 0x78, 0xff, 0xff, 0xff, 0xfc, 0xb9, 0x1c, 0x00,
	0x00, 0x00, 0xf3, 0xa5, 0x8b, 0x45, 0x0c, 0x8d, 0xa5, 0x4c, 0xff, 0xff, 0xff, 0x5b, 0x5e, 0x5f,
	0xc9, 0xc3, 0x00, 0x00, 0x55, 0x8b, 0xec, 0x56, 0x53, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x81,
	0xc3, 0xa6, 0x10, 0x00, 0x00, 0x8d, 0x83, 0xd0, 0x00, 0x00, 0x00, 0x8d, 0x70, 0xfc, 0x83, 0x78,
	0xfc, 0xff, 0x74, 0x0c, 0x8b, 0x06, 0xff, 0xd0, 0x83, 0xc6, 0xfc, 0x83, 0x3e, 0xff, 0x75, 0xf4,
	0x8d, 0x65, 0xf8, 0x5b, 0x5e, 0xc9, 0xc3, 0x90, 0x55, 0x8b, 0xec, 0x53, 0xe8, 0x00, 0x00, 0x00,
	0x00, 0x5b, 0x81, 0xc3, 0x73, 0x10, 0x00, 0x00, 0x8b, 0x5d, 0xfc, 0xc9, 0xc3
};

/**
 * \name Test the micro disassembler.
 * \{
 */
void
LoaderTest::testMicroDis1()
{
	const unsigned char *p = pent_hello_text;
	size_t n = sizeof pent_hello_text;
	size_t i = 0;
	size_t totalSize = 0;
	while (totalSize < n) {
		size_t size = microX86Dis(p);
		if (size >= 0x40) {
			std::cout << "Not handled instruction at offset 0x" << std::hex
			          << (ADDRESS)p - (ADDRESS)pent_hello_text << std::endl;
			CPPUNIT_ASSERT(size != 0x40);
			return;
		}
		size_t expected = lengths[i++];
		if (expected != size) {
			std::cout << "At offset 0x" << std::hex
			          << (ADDRESS)p - (ADDRESS)pent_hello_text << " ("
			          << (int)*(p)     << " "
			          << (int)*(p + 1) << " "
			          << (int)*(p + 2) << " "
			          << (int)*(p + 3) << " "
			          << ") expected "
			          << std::dec << expected << ", actual " << size << std::endl;
			CPPUNIT_ASSERT_EQUAL(expected, size);
		}
		p += size;
		totalSize += size;
	}
	CPPUNIT_ASSERT_EQUAL(n, totalSize);
}

void
LoaderTest::testMicroDis2()
{
	// Now a special test:
	// 8048910:  0f be 00           movsbl (%eax),%eax
	// 8048913:  0f bf 00           movswl (%eax),%eax

	const unsigned char movsbl[3] = { 0x0f, 0xbe, 0x00 };
	const unsigned char movswl[3] = { 0x0f, 0xbf, 0x00 };
	size_t size = microX86Dis(movsbl);
	CPPUNIT_ASSERT_EQUAL((size_t)3, size);
	size = microX86Dis(movswl);
	CPPUNIT_ASSERT_EQUAL((size_t)3, size);
}
/** \} */

typedef unsigned (*elfHashFcn)(const char *);
extern "C" unsigned elf_hash(const char *);
void
LoaderTest::testElfHash()
{
#ifdef DYNAMIC
	void *dlHandle = dlopen(ELFBINFILE, RTLD_LAZY);
	CPPUNIT_ASSERT(dlHandle);
	// Use the handle to find the "elf_hash" function
	elfHashFcn pFcn = (elfHashFcn)dlsym(dlHandle, "elf_hash");
#else
	elfHashFcn pFcn = elf_hash;
#endif
	CPPUNIT_ASSERT(pFcn);

	// Call the function with the string "main"
	unsigned act = (*pFcn)("main");
	unsigned exp = 0x737fe;
	CPPUNIT_ASSERT_EQUAL(exp, act);
}
