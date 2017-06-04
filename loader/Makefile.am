AM_CPPFLAGS = \
	-I$(top_srcdir)/include


noinst_LTLIBRARIES = \
	libmicroX86dis.la

libmicroX86dis_la_SOURCES = \
	microX86dis.c


# Installing libloader.so in the same directory as the modules
# has a side-effect of adding an RPATH to this directory for
# everything that links against libloader.so.
pkglib_LTLIBRARIES = \
	libloader.la

libloader_la_SOURCES = \
	BinaryFileFactory.cpp \
	BinaryFile.cpp \
	$(top_srcdir)/include/BinaryFile.h \
	SymTab.cpp \
	SymTab.h
libloader_la_LDFLAGS = -no-undefined -avoid-version

if DYNAMIC
libloader_la_LIBADD = -ldl

pkglib_LTLIBRARIES += \
	DOS4GWBinaryFile.la \
	ElfBinaryFile.la \
	ExeBinaryFile.la \
	HpSomBinaryFile.la \
	IntelCoffFile.la \
	MachOBinaryFile.la \
	PalmBinaryFile.la \
	Win32BinaryFile.la

DOS4GWBinaryFile_la_SOURCES = DOS4GWBinaryFile.cpp DOS4GWBinaryFile.h
ElfBinaryFile_la_SOURCES    = ElfBinaryFile.cpp    ElfBinaryFile.h
ExeBinaryFile_la_SOURCES    = ExeBinaryFile.cpp    ExeBinaryFile.h
HpSomBinaryFile_la_SOURCES  = HpSomBinaryFile.cpp  HpSomBinaryFile.h
IntelCoffFile_la_SOURCES    = IntelCoffFile.cpp    IntelCoffFile.h
MachOBinaryFile_la_SOURCES  = MachOBinaryFile.cpp  MachOBinaryFile.h
PalmBinaryFile_la_SOURCES   = PalmBinaryFile.cpp   PalmBinaryFile.h
Win32BinaryFile_la_SOURCES  = Win32BinaryFile.cpp  Win32BinaryFile.h

DOS4GWBinaryFile_la_LIBADD = libmicroX86dis.la
Win32BinaryFile_la_LIBADD  = libmicroX86dis.la

DOS4GWBinaryFile_la_LDFLAGS = -no-undefined -avoid-version -module -shared
ElfBinaryFile_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
ExeBinaryFile_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
HpSomBinaryFile_la_LDFLAGS  = -no-undefined -avoid-version -module -shared
IntelCoffFile_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
MachOBinaryFile_la_LDFLAGS  = -no-undefined -avoid-version -module -shared
PalmBinaryFile_la_LDFLAGS   = -no-undefined -avoid-version -module -shared
Win32BinaryFile_la_LDFLAGS  = -no-undefined -avoid-version -module -shared
else
libloader_la_SOURCES += \
	DOS4GWBinaryFile.cpp \
	ElfBinaryFile.cpp \
	ExeBinaryFile.cpp \
	HpSomBinaryFile.cpp \
	IntelCoffFile.cpp \
	MachOBinaryFile.cpp \
	PalmBinaryFile.cpp \
	Win32BinaryFile.cpp
libloader_la_LIBADD = \
	libmicroX86dis.la
endif


bin_PROGRAMS = \
	bffDump

bffDump_SOURCES = \
	bffDump.cpp
bffDump_LDADD = \
	libloader.la


check_LTLIBRARIES = \
	libLoaderTest.la \
	libBinaryFileStub.la

libLoaderTest_la_SOURCES = \
	LoaderTest.cpp \
	LoaderTest.h
libLoaderTest_la_LIBADD = \
	libloader.la \
	libmicroX86dis.la \
	$(CPPUNIT_LIBS)
libLoaderTest_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS) \
	-DTESTDIR=\"$(top_srcdir)/test/\"

libBinaryFileStub_la_SOURCES = \
	BinaryFileStub.cpp \
	$(top_srcdir)/include/BinaryFileStub.h
libBinaryFileStub_la_LIBADD = \
	$(top_builddir)/loader/libloader.la


TESTS = $(check_PROGRAMS)
check_PROGRAMS = \
	testLoader

testLoader_SOURCES = \
	testLoader.cpp
testLoader_LDADD = \
	libLoaderTest.la \
	$(CPPUNIT_LIBS)
testLoader_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPUNIT_CFLAGS)


noinst_HEADERS = \
	macho-apple.h \
	nlist.h \
	palmsystraps.h \
	syself.h

EXTRA_DIST = \
	ArchiveFile.cpp \
	ElfArchiveFile.cpp \
	mach-o
