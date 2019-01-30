/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the TypeTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "TypeTest.h"

#include "boomerang.h"
#include "frontend.h"
#include "proc.h"
#include "prog.h"
#include "signature.h"
#include "type.h"

#include <string>

#define HELLO_WINDOWS       "test/windows/hello.exe"

/**
 * Test type unsigned long.
 */
void
TypeTest::testTypeLong()
{
	std::string expected("unsigned long long");
	IntegerType t(64, -1);
	std::string actual(t.getCtype());
	CPPUNIT_ASSERT_EQUAL(expected, actual);
}

/**
 * Test type inequality.
 */
void
TypeTest::testNotEqual()
{
	IntegerType t1(32, -1);
	IntegerType t2(32, -1);
	IntegerType t3(16, -1);
	CPPUNIT_ASSERT(!(t1 != t2));
	CPPUNIT_ASSERT(t2 != t3);
}

void
TypeTest::testCompound()
{
	auto prog = Prog::open(HELLO_WINDOWS);
	auto fe = prog->getFrontEnd();

	auto filelogger = new std::ofstream();
	filelogger->rdbuf()->pubsetbuf(nullptr, 0);
	filelogger->open(Boomerang::get()->getOutputPath() + "log");
	Boomerang::get()->setLogger(filelogger);  // May try to output some messages to LOG

	fe->readLibraryCatalog();  // Read definitions

	Signature *paintSig = fe->getLibSignature("BeginPaint");
	// Second argument should be an LPPAINTSTRUCT
	Type *ty = paintSig->getParamType(1);
	std::string expected("LPPAINTSTRUCT");
	std::string actual(ty->getCtype());
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Get the type pointed to
	ty = ty->asPointer()->getPointsTo();
	expected = "PAINTSTRUCT";
	actual = ty->getCtype();
	CPPUNIT_ASSERT_EQUAL(expected, actual);


	// Offset 8 should have a RECT
	Type *subTy = ty->asCompound()->getTypeAtOffset(8 * 8);
	expected = "RECT";
	actual = subTy->getCtype();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Name at offset C should be bottom
	expected = "bottom";
	actual = subTy->asCompound()->getNameAtOffset(0x0C * 8);
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Now figure out the name at offset 8+C
	expected = "rcPaint";
	actual = ty->asCompound()->getNameAtOffset((8 + 0x0C) * 8);
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Also at offset 8
	actual = ty->asCompound()->getNameAtOffset((8 + 0) * 8);
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Also at offset 8+4
	actual = ty->asCompound()->getNameAtOffset((8 + 4) * 8);
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// And at offset 8+8
	actual = ty->asCompound()->getNameAtOffset((8 + 8) * 8);
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	delete prog;
}

/**
 * Test the DataIntervalMap class.
 */
void
TypeTest::testDataInterval()
{
	DataIntervalMap dim;

	auto prog = new Prog;
	UserProc *proc = (UserProc *)prog->newProc("test", 0x123);
	std::string name("test");
	proc->setSignature(Signature::instantiate(PLAT_PENTIUM, CONV_C, name.c_str()));
	dim.setProc(proc);

	dim.addItem(0x1000, "first", new IntegerType(32, 1));
	dim.addItem(0x1004, "second", new FloatType(64));
	std::string actual(dim.prints());
	std::string expected("0x1000 first int\n"
	                     "0x1004 second double\n");
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	auto pdie = dim.find(0x1000);
	expected = "first";
	CPPUNIT_ASSERT(pdie);
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	pdie = dim.find(0x1003);
	CPPUNIT_ASSERT(pdie);
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	pdie = dim.find(0x1004);
	CPPUNIT_ASSERT(pdie);
	expected = "second";
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	pdie = dim.find(0x1007);
	CPPUNIT_ASSERT(pdie);
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	CompoundType ct;
	ct.addType(new IntegerType(16, 1), "short1");
	ct.addType(new IntegerType(16, 1), "short2");
	ct.addType(new IntegerType(32, 1), "int1");
	ct.addType(new FloatType(32), "float1");
	dim.addItem(0x1010, "struct1", &ct);

	auto &ctcl = ct.compForAddress(0x1012, dim);
	unsigned ua = ctcl.size();
	unsigned ue = 1;
	CPPUNIT_ASSERT_EQUAL(ue, ua);
	auto &ctc = ctcl.front();
	ue = 0;
	ua = ctc.isArray;
	CPPUNIT_ASSERT_EQUAL(ue, ua);
	expected = "short2";
	actual = ctc.u.memberName;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// An array of 10 struct1's
	ArrayType at(&ct, 10);
	dim.addItem(0x1020, "array1", &at);
	auto &ctcl2 = at.compForAddress(0x1020 + 0x3C + 8, dim);
	// Should be 2 components: [5] and .float1
	ue = 2;
	ua = ctcl2.size();
	CPPUNIT_ASSERT_EQUAL(ue, ua);
	auto &ctc0 = ctcl2.front();
	auto &ctc1 = ctcl2.back();
	ue = 1;
	ua = ctc0.isArray;
	CPPUNIT_ASSERT_EQUAL(ue, ua);
	ue = 5;
	ua = ctc0.u.index;
	CPPUNIT_ASSERT_EQUAL(ue, ua);
	ue = 0;
	ua = ctc1.isArray;
	CPPUNIT_ASSERT_EQUAL(ue, ua);
	expected = "float1";
	actual = ctc1.u.memberName;
	CPPUNIT_ASSERT_EQUAL(expected, actual);
	delete prog;
}

/**
 * Test the DataIntervalMap class with overlapping addItems.
 */
void
TypeTest::testDataIntervalOverlaps()
{
	DataIntervalMap dim;

	auto prog = new Prog;
	UserProc *proc = (UserProc *)prog->newProc("test", 0x123);
	std::string name("test");
	proc->setSignature(Signature::instantiate(PLAT_PENTIUM, CONV_C, name.c_str()));
	dim.setProc(proc);

	dim.addItem(0x1000, "firstInt", new IntegerType(32, 1));
	dim.addItem(0x1004, "firstFloat", new FloatType(32));
	dim.addItem(0x1008, "secondInt", new IntegerType(32, 1));
	dim.addItem(0x100C, "secondFloat", new FloatType(32));
	CompoundType ct;
	ct.addType(new IntegerType(32, 1), "int3");
	ct.addType(new FloatType(32), "float3");
	dim.addItem(0x1010, "existingStruct", &ct);

	// First insert a new struct over the top of the existing middle pair
	CompoundType ctu;
	ctu.addType(new IntegerType(32, 0), "newInt");  // This int has UNKNOWN sign
	ctu.addType(new FloatType(32), "newFloat");
	dim.addItem(0x1008, "replacementStruct", &ctu);

	auto pdie = dim.find(0x1008);
	std::string expected = "struct { int newInt; float newFloat; }";
	std::string actual = pdie->second.type->getCtype();
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Attempt a weave; should fail
	CompoundType ct3;
	ct3.addType(new FloatType(32), "newFloat3");
	ct3.addType(new IntegerType(32, 0), "newInt3");
	dim.addItem(0x1004, "weaveStruct1", &ct3);
	pdie = dim.find(0x1004);
	expected = "firstFloat";
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Totally unaligned
	dim.addItem(0x1001, "weaveStruct2", &ct3);
	pdie = dim.find(0x1001);
	expected = "firstInt";
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	dim.addItem(0x1004, "firstInt", new IntegerType(32, 1));  // Should fail
	pdie = dim.find(0x1004);
	expected = "firstFloat";
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Set up three ints
	dim.deleteItem(0x1004);
	dim.addItem(0x1004, "firstInt", new IntegerType(32, 1));  // Definately signed
	dim.deleteItem(0x1008);
	dim.addItem(0x1008, "firstInt", new IntegerType(32, 0));  // Unknown signedess
	// then, add an array over the three integers
	ArrayType at(new IntegerType(32, 0), 3);
	dim.addItem(0x1000, "newArray", &at);
	pdie = dim.find(0x1005);  // Check middle element
	expected = "newArray";
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);
	pdie = dim.find(0x1000);  // Check first
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);
	pdie = dim.find(0x100B);  // Check last
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	// Already have an array of 3 ints at 0x1000. Put a new array completely before, then with only one word overlap
	dim.addItem(0xF00, "newArray2", &at);
	pdie = dim.find(0x1000);  // Shouyld still be newArray at 0x1000
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	pdie = dim.find(0xF00);
	expected = "newArray2";
	actual = pdie->second.name;
	CPPUNIT_ASSERT_EQUAL(expected, actual);

	dim.addItem(0xFF8, "newArray3", &at);  // Should fail
	pdie = dim.find(0xFF8);
	unsigned ue = 0;  // Expect NULL
	unsigned ua = (unsigned)pdie;
	CPPUNIT_ASSERT_EQUAL(ue, ua);
	delete prog;
}
