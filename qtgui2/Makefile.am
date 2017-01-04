AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(QTGUI_CFLAGS)


bin_PROGRAMS = \
	boomerang-gui

boomerang_gui_SOURCES = \
	DecompilerThread.cpp \
	DecompilerThread.h \
	main.cpp \
	mainwindow.cpp \
	mainwindow.h \
	rtleditor.cpp \
	rtleditor.h \
	about.ui \
	boomerang.ui \
	boomerang.qrc
nodist_boomerang_gui_SOURCES = \
	moc_DecompilerThread.cpp \
	moc_mainwindow.cpp \
	moc_rtleditor.cpp \
	qrc_boomerang.cpp
boomerang_gui_LDADD = \
	$(top_builddir)/libboomerang.la \
	$(top_builddir)/frontend/libfrontend.la \
	$(top_builddir)/loader/libloader.la \
	$(QTGUI_LIBS)

if GC
AM_CPPFLAGS += \
	$(GC_CFLAGS)
boomerang_gui_SOURCES += \
	mypthread.cpp
boomerang_gui_LDADD += \
	$(GC_LIBS) \
	-ldl
endif

moc_%.cpp: %.h
	$(AM_V_GEN)$(MOC) $(QTGUI_CFLAGS) -o $@ $<
ui_%.h: %.ui
	$(AM_V_GEN)$(UIC) -o $@ $<
qrc_%.cpp: %.qrc
	$(AM_V_GEN)$(RCC) -name $* -o $@ $<

BUILT_SOURCES = \
	ui_boomerang.h \
	ui_about.h

RESOURCES = \
	images/closetab.png \
	images/decode.png \
	images/decompile.png \
	images/gencode.png \
	images/load1.png \
	images/rarrow.png
qrc_boomerang.cpp: $(RESOURCES)
dist_noinst_DATA = $(RESOURCES)

CLEANFILES = \
	moc_* \
	ui_* \
	qrc_*
