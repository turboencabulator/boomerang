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
	managed.cpp \
	$(top_srcdir)/include/managed.h \
	proc.cpp \
	$(top_srcdir)/include/proc.h \
	prog.cpp \
	$(top_srcdir)/include/prog.h \
	rtl.cpp \
	$(top_srcdir)/include/rtl.h \
	signature.cpp \
	$(top_srcdir)/include/signature.h \
	statement.cpp \
	$(top_srcdir)/include/statement.h \
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


EXTRA_DIST = \
	testDbase.cpp \
	testExp.cpp \
	testRtl.cpp \
	testStmt.cpp
