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


# Installing libfrontend.so in the same directory as the modules
# has a side-effect of adding an RPATH to this directory for
# everything that links against libfrontend.so.
pkglib_LTLIBRARIES = \
	libfrontend.la

libfrontend_la_SOURCES = \
	frontend.cpp \
	$(top_srcdir)/include/frontend.h \
	njmcDecoder.cpp \
	$(top_srcdir)/include/decoder.h
libfrontend_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/c
libfrontend_la_LDFLAGS = -no-undefined -avoid-version

if DYNAMIC
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


%-names.c %-names.h:
	$(AM_V_GEN)cd $(dir $*) && $(TOOLS) -fieldnames $(notdir $*)-names $(notdir $^)
%-names.cpp: %-names.c
	$(AM_V_GEN)cp $< $@

machine/sparc/sparc-names.c machine/sparc/sparc-names.h: machine/sparc/core.spec machine/sparc/synth.spec machine/sparc/dis.spec
machine/hppa/hppa-names.c machine/hppa/hppa-names.h: machine/hppa/hppa.spec machine/hppa/hppadis.spec


machine/st20/st20.ssl: machine/st20/st20.ssl.m4
	$(AM_V_GEN)$(M4) $< > $@


check_LTLIBRARIES = \
	libFrontPentTest.la \
	libFrontSparcTest.la

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


EXTRA_DIST = \
	machine
