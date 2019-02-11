AM_CPPFLAGS = \
	-I$(top_srcdir)/include


frontenddir = \
	$(pkgdatadir)/frontend

nobase_dist_frontend_DATA = \
	machine/mips/mips.ssl \
	machine/pentium/pentium.ssl \
	machine/ppc/ppc.ssl \
	machine/sparc/sparc.ssl \
	machine/st20/st20.ssl


if DYNAMIC
# Installing libfrontend.so in the same directory as the modules
# has a side-effect of adding an RPATH to this directory for
# everything that links against libfrontend.so.
pkglib_LTLIBRARIES = \
	libfrontend.la
else
noinst_LTLIBRARIES = \
	libfrontend.la
endif

libfrontend_la_SOURCES = \
	frontend.cpp \
	$(top_srcdir)/include/frontend.h \
	njmcDecoder.cpp \
	decoder.h \
	insnameelem.cpp \
	insnameelem.h \
	register.cpp \
	register.h \
	sslinst.cpp \
	sslinst.h \
	sslparser.ypp \
	sslparser.h \
	sslscanner.lpp \
	sslscanner.h
libfrontend_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/c

if DYNAMIC
libfrontend_la_LDFLAGS = -no-undefined -avoid-version
libfrontend_la_LIBADD = -ldl

pkglib_LTLIBRARIES += \
	mipsfrontend.la \
	pentiumfrontend.la \
	ppcfrontend.la \
	sparcfrontend.la \
	st20frontend.la

mipsfrontend_la_SOURCES = \
	mipsdecoder.cpp \
	mipsdecoder.h \
	mipsfrontend.cpp \
	mipsfrontend.h

pentiumfrontend_la_SOURCES = \
	pentiumdecoder.cpp \
	pentiumdecoder.h \
	pentiumfrontend.cpp \
	pentiumfrontend.h

ppcfrontend_la_SOURCES = \
	ppcdecoder.cpp \
	ppcdecoder.h \
	ppcfrontend.cpp \
	ppcfrontend.h

sparcfrontend_la_SOURCES = \
	sparcdecoder.cpp \
	sparcdecoder.h \
	sparcfrontend.cpp \
	sparcfrontend.h

st20frontend_la_SOURCES = \
	st20decoder.cpp \
	st20decoder.h \
	st20frontend.cpp \
	st20frontend.h

mipsfrontend_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
pentiumfrontend_la_LDFLAGS = -no-undefined -avoid-version -module -shared
ppcfrontend_la_LDFLAGS     = -no-undefined -avoid-version -module -shared
sparcfrontend_la_LDFLAGS   = -no-undefined -avoid-version -module -shared
st20frontend_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
else
libfrontend_la_SOURCES += \
	mipsdecoder.cpp \
	mipsfrontend.cpp \
	pentiumdecoder.cpp \
	pentiumfrontend.cpp \
	ppcdecoder.cpp \
	ppcfrontend.cpp \
	sparcdecoder.cpp \
	sparcfrontend.cpp \
	st20decoder.cpp \
	st20frontend.cpp
endif


# Decoders are built from matcher (.m) files...
%decoder.cpp: machine/%/decoder.m
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)

# ...and SLED (.spec) files.
mipsdecoder.cpp: machine/mips/core.spec machine/mips/synth.spec
pentiumdecoder.cpp: machine/pentium/core.spec machine/pentium/dis.spec
ppcdecoder.cpp: machine/ppc/core.spec machine/ppc/synth.spec
sparcdecoder.cpp: machine/sparc/core.spec machine/sparc/synth.spec machine/sparc/dis.spec
st20decoder.cpp: machine/st20/core.spec

# Other matchers (not currently used)
machine/hppa/decoder.cpp: machine/hppa/decoder.m machine/hppa/hppa.spec machine/hppa/hppadis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/hppa/decoder_low.cpp: machine/hppa/decoder_low.m machine/hppa/hppa.spec machine/hppa/hppadis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/hppa/disassembler.cpp: machine/hppa/disassembler.m machine/hppa/hppa.spec machine/hppa/hppadis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/hppa/hppa.pat.cpp: machine/hppa/hppa.pat.m machine/hppa/hppa.spec machine/hppa/hppadis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/hppa/simple.cpp: machine/hppa/simple.m machine/hppa/simple.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/mc68k/decoder.cpp: machine/mc68k/decoder.m machine/mc68k/mc68000.spec machine/mc68k/mc68000dis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/mc68k/decoder_low.cpp: machine/mc68k/decoder_low.m machine/mc68k/mc68000.spec machine/mc68k/mc68000dis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/mc68k/mc68k.pat.cpp: machine/mc68k/mc68k.pat.m machine/mc68k/mc68000.spec machine/mc68k/mc68000dis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/pentium/disassembler.cpp: machine/pentium/disassembler.m machine/pentium/core.spec machine/pentium/dis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/sparc/sparc.pat.cpp: machine/sparc/sparc.pat.m machine/sparc/core.spec machine/sparc/synth.spec machine/sparc/dis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)
machine/sparc/disassembler.cpp: machine/sparc/disassembler.m machine/sparc/core.spec machine/sparc/synth.spec machine/sparc/dis.spec
	$(AM_V_GEN)./mltk.sh $< -o $@ $(filter-out $<,$^)


# Generates an encoder from SLED files.
# Uses the Icon version of the toolkit.
%enc.c %enc.h:
	$(AM_V_GEN)cd $(dir $*) && $(TOOLS) -verbose -encoder enc $(notdir $^)

machine/hppa/enc.c machine/hppa/enc.h: machine/hppa/hppa.spec machine/hppa/hppadis.spec


# Generates a decoder/disassembler from a matcher and SLED files.
# Same as above, but uses the Icon version of the toolkit.
# Results will be somewhat different from the ML version of the toolkit.
# XXX:  Output filename should be something like $<.cpp instead of 'dec'.
%dec %dbm:
	$(AM_V_GEN)cd $(dir $*) && $(TOOLS) -verbose -debug-match dbm -matcher $(notdir $<) -decoder dec $(notdir $(filter-out $<,$^))
	cd $(dir $*) && $(UNGENERATE) dec

machine/hppa/dec machine/hppa/dbm: machine/hppa/disassembler.m machine/hppa/hppa.spec machine/hppa/hppadis.spec


# Some decoders/disassemblers need fieldname strings.
%-names.c %-names.h:
	$(AM_V_GEN)cd $(dir $*) && $(TOOLS) -fieldnames $(notdir $*)-names $(notdir $^)
%-names.cpp: %-names.c
	$(AM_V_GEN)cp $< $@

machine/sparc/sparc-names.c machine/sparc/sparc-names.h: machine/sparc/core.spec machine/sparc/synth.spec machine/sparc/dis.spec
machine/hppa/hppa-names.c machine/hppa/hppa-names.h: machine/hppa/hppa.spec machine/hppa/hppadis.spec


# SLED files are originally from the toolkit sources, generated using noweb.
# Files that have since been modified have their rules commented out.
machine/mips/core.spec machine/mips/synth.spec machine/mips/regs.spec machine/mips/trunc.spec machine/mips/names.spec machine/mips/check.spec: machine/mips/mips.nw
	$(AM_V_GEN)cd $(dir $@) && $(NOTANGLE) -L -Rmips-$(notdir $@) $(notdir $<) > $(notdir $@)
#machine/pentium/core.spec machine/pentium/synth.spec: machine/pentium/pentium.nw
#	$(AM_V_GEN)cd $(dir $@) && $(NOTANGLE) -L -Rpentium-$(notdir $@) $(notdir $<) > $(notdir $@)
#machine/ppc/core.spec machine/ppc/synth.spec: machine/ppc/ppc.nw
#	$(AM_V_GEN)cd $(dir $@) && $(NOTANGLE) -L -Rppc-$(notdir $@) $(notdir $<) > $(notdir $@)
#machine/sparc/core.spec machine/sparc/synth.spec machine/sparc/regs.spec: machine/sparc/sparc.nw
#	$(AM_V_GEN)cd $(dir $@) && $(NOTANGLE) -L -Rsparc-$(notdir $@) $(notdir $<) > $(notdir $@)

# Several SLED files squashed into one:
# (but it has been modified since, so comment this rule out for now)
#machine/mips/mips.spec: machine/mips/core.spec machine/mips/synth.spec machine/mips/regs.spec machine/mips/trunc.spec
#	$(AM_V_GEN)cat $^ > $@

# One more file generated from the noweb sources...
machine/mips/mips-checker.s: machine/mips/mips.nw
	$(AM_V_GEN)cd $(dir $@) && $(NOTANGLE) -R$(notdir $@) $(notdir $<) > $(notdir $@)


# These files are generated.
# Does anyone know what program was used to generate them?
#machine/hppa/hppa.pat.m machine/hppa/hppa.pat.h: machine/hppa/hppa.pat
#machine/mc68k/mc68k.pat.m machine/mc68k/mc68k.pat.h: machine/mc68k/mc68k.pat
#machine/sparc/sparc.pat.m machine/sparc/sparc.pat.h: machine/sparc/sparc.pat


# Also generated, using genemu 0.61 according to comments.
# Does anyone know where to find this program?
#machine/sparc/sparc.c machine/sparc/sparc.h: machine/sparc/sparc-core.spec machine/sparc/sparc.ssl


machine/st20/st20.ssl: machine/st20/st20.ssl.m4
	$(AM_V_GEN)$(M4) $< > $@


check_LTLIBRARIES = \
	libFrontPentTest.la \
	libFrontSparcTest.la \
	libParserTest.la

libFrontPentTest_la_SOURCES = \
	FrontPentTest.cpp \
	FrontPentTest.h
libFrontPentTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libFrontPentTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)

libFrontSparcTest_la_SOURCES = \
	FrontSparcTest.cpp \
	FrontSparcTest.h
libFrontSparcTest_la_LIBADD = \
	$(CPPUNIT_LIBS)
libFrontSparcTest_la_CPPFLAGS = \
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
	machine
