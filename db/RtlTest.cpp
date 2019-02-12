/**
 * \file
 * \ingroup UnitTest
 * \brief Provides the implementation for the RtlTest class.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "RtlTest.h"

#include "decoder.h"
#include "exp.h"
#include "frontend.h"
#include "proc.h"
#include "prog.h"
#include "rtl.h"
#include "statement.h"
#include "visitor.h"

#include <sstream>
#include <list>
#include <string>

#define SWITCH_SPARC        "test/sparc/switch_cc"
#define SWITCH_PENT         "test/pentium/switch_cc"

/**
 * Test appendExp and printing of RTLs.
 */
void
RtlTest::testAppend()
{
	auto a = new Assign(Location::regOf(8),
	                    new Binary(opPlus, Location::regOf(9), new Const(99)));
	RTL r;
	r.appendStmt(a);
	std::string expected("00000000    0 *v* r8 := r9 + 99\n");
	CPPUNIT_ASSERT_EQUAL(expected, r.prints());
	// No! appendExp does not copy the expression, so deleting the RTL will
	// delete the expression(s) in it.
	// Not sure if that's what we want...
	//delete a;
}

/**
 * Test constructor from list of expressions; cloning of RTLs.
 */
void
RtlTest::testClone()
{
	auto a1 = new Assign(Location::regOf(8),
	                     new Binary(opPlus, Location::regOf(9), new Const(99)));
	auto a2 = new Assign(new IntegerType(16),
	                     Location::param("x"),
	                     Location::param("y"));
	auto r = new RTL(0x1234);
	r->appendStmt(a1);
	r->appendStmt(a2);
	RTL *r2 = r->clone();
	std::string act1(r->prints());
	delete r;  // And r2 should still stand!
	std::string act2(r2->prints());
	delete r2;
	std::string expected("00001234    0 *v* r8 := r9 + 99\n"
	                     "            0 *j16* x := y\n");

	CPPUNIT_ASSERT_EQUAL(expected, act1);
	CPPUNIT_ASSERT_EQUAL(expected, act2);
}

/**
 * \ingroup UnitTestStub
 * \brief Stub class to test.
 */
class StmtVisitorStub : public StmtVisitor {
public:
	bool a = false,
	     b = false,
	     c = false,
	     d = false,
	     e = false,
	     f = false,
	     g = false,
	     h = false;

	void clear() { a = b = c = d = e = f = g = h = false; }
	bool visit(            RTL *s) override { a = true; return false; }
	bool visit(  GotoStatement *s) override { b = true; return false; }
	bool visit(BranchStatement *s) override { c = true; return false; }
	bool visit(  CaseStatement *s) override { d = true; return false; }
	bool visit(  CallStatement *s) override { e = true; return false; }
	bool visit(ReturnStatement *s) override { f = true; return false; }
	bool visit(     BoolAssign *s) override { g = true; return false; }
	bool visit(         Assign *s) override { h = true; return false; }
};

/**
 * Test the accept function for correct visiting behaviour.
 */
void
RtlTest::testVisitor()
{
	StmtVisitorStub visitor;

	/* rtl */
	auto rtl = new RTL();
	rtl->accept(visitor);
	CPPUNIT_ASSERT(visitor.a);
	delete rtl;

	/* jump stmt */
	auto jump = new GotoStatement;
	jump->accept(visitor);
	CPPUNIT_ASSERT(visitor.b);
	delete jump;

	/* branch stmt */
	auto jcond = new BranchStatement;
	jcond->accept(visitor);
	CPPUNIT_ASSERT(visitor.c);
	delete jcond;

	/* nway jump stmt */
	auto nwayjump = new CaseStatement;
	nwayjump->accept(visitor);
	CPPUNIT_ASSERT(visitor.d);
	delete nwayjump;

	/* call stmt */
	auto call = new CallStatement;
	call->accept(visitor);
	CPPUNIT_ASSERT(visitor.e);
	delete call;

	/* return stmt */
	auto ret = new ReturnStatement;
	ret->accept(visitor);
	CPPUNIT_ASSERT(visitor.f);
	delete ret;

	/* "bool" assgn */
	auto scond = new BoolAssign(0);
	scond->accept(visitor);
	CPPUNIT_ASSERT(visitor.g);
	delete scond;

	/* assignment stmt */
	auto as = new Assign;
	as->accept(visitor);
	CPPUNIT_ASSERT(visitor.h);
	delete as;

	/* polymorphic */
	Statement *s = new CallStatement;
	s->accept(visitor);
	CPPUNIT_ASSERT(visitor.e);
	delete s;
}

/**
 * Test the isCompare function.
 */
void
RtlTest::testIsCompare()
{
	auto prog = Prog::open(SWITCH_SPARC);
	CPPUNIT_ASSERT(prog);

	auto fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	CPPUNIT_ASSERT(fe->getFrontEndId() == PLAT_SPARC);

	// Decode second instruction: "sub      %i0, 2, %o1"
	int iReg;
	Exp *eOperand = nullptr;
	DecodeResult inst = fe->decodeInstruction(0x10910);
	CPPUNIT_ASSERT(inst.rtl);
	CPPUNIT_ASSERT(!inst.rtl->isCompare(iReg, eOperand));

	// Decode fifth instruction: "cmp       %o1, 5"
	inst = fe->decodeInstruction(0x1091c);
	CPPUNIT_ASSERT(inst.rtl);
	CPPUNIT_ASSERT(inst.rtl->isCompare(iReg, eOperand));
	CPPUNIT_ASSERT_EQUAL(9, iReg);
	std::string expected("5");
	CPPUNIT_ASSERT_EQUAL(expected, eOperand->prints());
	delete prog;

	prog = Prog::open(SWITCH_PENT);
	CPPUNIT_ASSERT(prog);

	fe = prog->getFrontEnd();
	CPPUNIT_ASSERT(fe);
	CPPUNIT_ASSERT(fe->getFrontEndId() == PLAT_PENTIUM);

	// Decode fifth instruction: "cmp   $0x5,%eax"
	inst = fe->decodeInstruction(0x80488fb);
	CPPUNIT_ASSERT(inst.rtl);
	CPPUNIT_ASSERT(inst.rtl->isCompare(iReg, eOperand));
	CPPUNIT_ASSERT_EQUAL(24, iReg);
	CPPUNIT_ASSERT_EQUAL(expected, eOperand->prints());

	// Decode instruction: "add     $0x4,%esp"
	inst = fe->decodeInstruction(0x804890c);
	CPPUNIT_ASSERT(inst.rtl);
	CPPUNIT_ASSERT(!inst.rtl->isCompare(iReg, eOperand));
	delete prog;
}

void
RtlTest::testSetConscripts()
{
	// m[1000] = m[1000] + 1000
	Statement *s1 = new Assign(Location::memOf(new Const(1000), 0),
	                           new Binary(opPlus,
	                                      Location::memOf(new Const(1000), nullptr),
	                                      new Const(1000)));

	// "printf("max is %d", (local0 > 0) ? local0 : global1)
	auto s2 = new CallStatement();
	Proc *proc = new UserProc(new Prog(), "printf", 0x2000);  // Making a true LibProc is problematic
	s2->setDestProc(proc);
	s2->setCalleeReturn(new ReturnStatement);  // So it's not a childless call
	Exp *e1 = new Const("max is %d");
	Exp *e2 = new Ternary(opTern,
	                      new Binary(opGtr,
	                                 Location::local("local0", nullptr),
	                                 new Const(0)),
	                      Location::local("local0", nullptr),
	                      Location::global("global1", nullptr));
	StatementList args;
	args.append(new Assign(Location::regOf(8), e1));
	args.append(new Assign(Location::regOf(9), e2));
	s2->setArguments(args);

	auto rtl = new RTL(0x1000);
	rtl->appendStmt(s1);
	rtl->appendStmt(s2);
	rtl->setConscripts(0, false);
	std::string expected("00001000    0 *v* m[1000\\1\\] := m[1000\\2\\] + 1000\\3\\\n"
	                     "            0 CALL printf(\n"
	                     "                *v* r8 := \"max is %d\"\\4\\\n"
	                     "                *v* r9 := (local0 > 0\\5\\) ? local0 : global1\n"
	                     "              )\n"
	                     "              Reaching definitions: \n"
	                     "              Live variables: \n");

	CPPUNIT_ASSERT_EQUAL(expected, rtl->prints());
}
