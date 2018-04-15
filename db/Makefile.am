AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/c \
	$(EXPAT_CFLAGS) \
	$(GC_CFLAGS)


noinst_LTLIBRARIES = \
	libdb.la

libdb_la_SOURCES = \
	basicblock.cpp \
	$(top_srcdir)/include/basicblock.h \
	cfg.cpp \
	$(top_srcdir)/include/cfg.h \
	dataflow.cpp \
	$(top_srcdir)/include/dataflow.h \
	exp.cpp \
	$(top_srcdir)/include/exp.h \
	insnameelem.cpp \
	insnameelem.h \
	managed.cpp \
	$(top_srcdir)/include/managed.h \
	proc.cpp \
	$(top_srcdir)/include/proc.h \
	prog.cpp \
	$(top_srcdir)/include/prog.h \
	register.cpp \
	$(top_srcdir)/include/register.h \
	rtl.cpp \
	$(top_srcdir)/include/rtl.h \
	signature.cpp \
	$(top_srcdir)/include/signature.h \
	sslinst.cpp \
	sslparser.ypp \
	sslparser.h \
	sslscanner.lpp \
	sslscanner.h \
	statement.cpp \
	$(top_srcdir)/include/statement.h \
	table.cpp \
	table.h \
	visitor.cpp \
	$(top_srcdir)/include/visitor.h \
	xmlprogparser.cpp \
	$(top_srcdir)/include/xmlprogparser.h
noinst_HEADERS = \
	$(top_srcdir)/include/cluster.h \
	$(top_srcdir)/include/exphelp.h \
	$(top_srcdir)/include/memo.h \
	$(top_srcdir)/include/operator.h \
	operstrings.h \
	$(top_srcdir)/include/sigenum.h \
	$(top_srcdir)/include/types.h
libdb_la_LIBADD = \
	$(EXPAT_LIBS) \
	$(GC_LIBS)


check_LTLIBRARIES = \
	libCfgTest.la \
	libDfaTest.la \
	libExpTest.la \
	libParserTest.la \
	libProcTest.la \
	libProgTest.la \
	libRtlTest.la \
	libStatementTest.la


libCfgTest_la_SOURCES = \
	CfgTest.cpp \
	CfgTest.h
libCfgTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libCfgTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libDfaTest_la_SOURCES = \
	DfaTest.cpp \
	DfaTest.h
libDfaTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libDfaTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libExpTest_la_SOURCES = \
	ExpTest.cpp \
	ExpTest.h
libExpTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libExpTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libParserTest_la_SOURCES = \
	ParserTest.cpp \
	ParserTest.h
libParserTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libParserTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libProcTest_la_SOURCES = \
	ProcTest.cpp \
	ProcTest.h
libProcTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libProcTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libProgTest_la_SOURCES = \
	ProgTest.cpp \
	ProgTest.h
libProgTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libProgTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libRtlTest_la_SOURCES = \
	RtlTest.cpp \
	RtlTest.h
libRtlTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libRtlTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libStatementTest_la_SOURCES = \
	StatementTest.cpp \
	StatementTest.h
libStatementTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libStatementTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)


TESTS = $(check_PROGRAMS)
check_PROGRAMS = \
	checkstrings

checkstrings_SOURCES = \
	checkstrings.cpp


# Various, fragile hacks to use flex++/bison++
# Tested with GNU automake 1.15, GNU make 4.1
#
# 1.  Set YACC and LEX to keep automake happy.  Otherwise it refuses to
#     finish, complaining that you should add AC_PROG_YACC and AC_PROG_LEX to
#     configure.ac.  Neither of those macros are useful to us.
#
# 2.  Input files must be named with a .ypp/.lpp suffix (or similar) so that
#     automake will know the generated file has a .cpp suffix (as opposed to
#     .c).  This will convince automake to compile it in C++ mode and also
#     distribute the .cpp file.
#
# 3.  Define our own rules for converting .ypp/.lpp to .cpp/.h.  Avoid ylwrap,
#     which is unnecessary since the output files don't need to be renamed.
#
# 4.  Convince make to generate .cpp/.h using our rules instead of .cpp/.hpp
#     using its built-in rules or the rules supplied by automake.  The order
#     of SUFFIXES is somehow important.  Move the -d flag from AM_YFLAGS
#     to the rule to prevent automake from generating a .hpp -> .cpp
#     dependency when it scans AM_YFLAGS for that flag.
#
# 5.  To bootstrap, add the .h files to BUILT_SOURCES.  Also add them to a
#     _SOURCES variable so they will be distributed.
#
YACC = $(BISONPP)
AM_YFLAGS = -v
LEX = $(FLEXPP)
AM_LFLAGS = -v

SUFFIXES = .cpp .h .ypp .lpp
%.cpp %.h: %.ypp
	$(AM_V_YACC)$(YACCCOMPILE) -o $*.cpp -d $<
%.cpp %.h: %.lpp
	$(AM_V_LEX)$(LEXCOMPILE) -o$*.cpp -h $<

BUILT_SOURCES = \
	sslparser.h \
	sslscanner.h

CLEANFILES = *.output

EXTRA_DIST = \
	signatureStubs.cpp \
	testDbase.cpp \
	testExp.cpp \
	testExpStubs.cpp \
	testRtl.cpp \
	testRtlStubs.cpp \
	testStmt.cpp \
	testStmtStubs.cpp \
	typeStubs.cpp \
	utilStubs.cpp
