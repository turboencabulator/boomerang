AM_CPPFLAGS = \
	-I$(top_srcdir)/include


noinst_LTLIBRARIES = \
	libmicroX86dis.la \
	libBinaryFile.la

libmicroX86dis_la_SOURCES = \
	microX86dis.c

libBinaryFile_la_SOURCES = \
	BinaryFile.cpp \
	$(top_srcdir)/include/BinaryFile.h \
	SymTab.cpp \
	SymTab.h


pkglib_LTLIBRARIES = \
	libBinaryFileFactory.la \
	DOS4GWBinaryFile.la \
	ElfBinaryFile.la \
	ExeBinaryFile.la \
	HpSomBinaryFile.la \
	IntelCoffFile.la \
	MachOBinaryFile.la \
	PalmBinaryFile.la \
	Win32BinaryFile.la

libBinaryFileFactory_la_SOURCES = BinaryFileFactory.cpp
libBinaryFileFactory_la_LIBADD = libBinaryFile.la -ldl
libBinaryFileFactory_la_LDFLAGS = -no-undefined -avoid-version

DOS4GWBinaryFile_la_SOURCES = DOS4GWBinaryFile.cpp DOS4GWBinaryFile.h
ElfBinaryFile_la_SOURCES    = ElfBinaryFile.cpp    ElfBinaryFile.h
ExeBinaryFile_la_SOURCES    = ExeBinaryFile.cpp    ExeBinaryFile.h
HpSomBinaryFile_la_SOURCES  = HpSomBinaryFile.cpp  HpSomBinaryFile.h
IntelCoffFile_la_SOURCES    = IntelCoffFile.cpp    IntelCoffFile.h
MachOBinaryFile_la_SOURCES  = MachOBinaryFile.cpp  MachOBinaryFile.h
PalmBinaryFile_la_SOURCES   = PalmBinaryFile.cpp   PalmBinaryFile.h
Win32BinaryFile_la_SOURCES  = Win32BinaryFile.cpp  Win32BinaryFile.h

DOS4GWBinaryFile_la_LIBADD = libBinaryFile.la libmicroX86dis.la
ElfBinaryFile_la_LIBADD    = libBinaryFile.la
ExeBinaryFile_la_LIBADD    = libBinaryFile.la
HpSomBinaryFile_la_LIBADD  = libBinaryFile.la
IntelCoffFile_la_LIBADD    = libBinaryFile.la
MachOBinaryFile_la_LIBADD  = libBinaryFile.la
PalmBinaryFile_la_LIBADD   = libBinaryFile.la
Win32BinaryFile_la_LIBADD  = libBinaryFile.la libmicroX86dis.la

DOS4GWBinaryFile_la_LDFLAGS = -no-undefined -avoid-version -module -shared
ElfBinaryFile_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
ExeBinaryFile_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
HpSomBinaryFile_la_LDFLAGS  = -no-undefined -avoid-version -module -shared
IntelCoffFile_la_LDFLAGS    = -no-undefined -avoid-version -module -shared
MachOBinaryFile_la_LDFLAGS  = -no-undefined -avoid-version -module -shared
PalmBinaryFile_la_LDFLAGS   = -no-undefined -avoid-version -module -shared
Win32BinaryFile_la_LDFLAGS  = -no-undefined -avoid-version -module -shared


bin_PROGRAMS = \
	bffDump

bffDump_SOURCES = bffDump.cpp
bffDump_LDADD = libBinaryFileFactory.la


check_LTLIBRARIES = \
	libLoaderTest.la \
	libBinaryFileStub.la

libLoaderTest_la_SOURCES = \
	LoaderTest.cpp \
	LoaderTest.h

libBinaryFileStub_la_SOURCES = \
	BinaryFileStub.cpp \
	$(top_srcdir)/include/BinaryFileStub.h


TESTS = $(check_PROGRAMS)
check_PROGRAMS = \
	testLoader

testLoader_SOURCES = \
	testLoader.cpp
testLoader_LDADD = \
	libLoaderTest.la \
	libBinaryFileFactory.la \
	libmicroX86dis.la \
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
	objc
