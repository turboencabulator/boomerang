AM_CPPFLAGS = \
	-I$(top_srcdir)/include


noinst_LTLIBRARIES = \
	libansi-c.la

libansi_c_la_SOURCES = \
	ansi-c-parser.ypp \
	ansi-c-parser.h \
	ansi-c-scanner.lpp \
	ansi-c-scanner.h


check_LTLIBRARIES = \
	libCTest.la

libCTest_la_SOURCES = \
	CTest.cpp \
	CTest.h
libCTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libCTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)


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
	$(YACCCOMPILE) -o $*.cpp -d $<
%.cpp %.h: %.lpp
	$(LEXCOMPILE) -o$*.cpp -h $<

BUILT_SOURCES = \
	ansi-c-parser.h \
	ansi-c-scanner.h

CLEANFILES = *.output
