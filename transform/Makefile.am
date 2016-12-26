AM_CPPFLAGS = \
	-I$(top_srcdir)/include


noinst_LTLIBRARIES = \
	libtransform.la

libtransform_la_SOURCES = \
	transformer.cpp \
	$(top_srcdir)/include/transformer.h \
	rdi.cpp \
	rdi.h \
	generic.cpp \
	generic.h \
	transformation-parser.ypp \
	transformation-parser.h \
	transformation-scanner.lpp \
	transformation-scanner.h

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
	transformation-parser.h \
	transformation-scanner.h

CLEANFILES = *.output
