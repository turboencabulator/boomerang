AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(GC_CFLAGS)


noinst_LTLIBRARIES = \
	libtype.la

libtype_la_SOURCES = \
	constraint.cpp \
	$(top_srcdir)/include/constraint.h \
	dfa.cpp \
	type.cpp \
	$(top_srcdir)/include/type.h
libtype_la_LIBADD = \
	$(GC_LIBS)


check_LTLIBRARIES = \
	libTypeTest.la

libTypeTest_la_SOURCES = \
	TypeTest.cpp \
	TypeTest.h
libTypeTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libTypeTest_la_CPPFLAGS = \
	-I$(top_srcdir)/frontend \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)
