AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(QTGUI_CFLAGS) \
	$(GC_CFLAGS)


bin_PROGRAMS = \
	boomerang-gui

boomerang_gui_SOURCES = \
	DecompilerThread.cpp \
	DecompilerThread.h \
	main.cpp \
	mainwindow.cpp \
	mainwindow.h \
	mypthread.cpp \
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
	$(QTGUI_LIBS) \
	$(GC_LIBS)

moc_%.cpp: %.h
	$(MOC) $(QTGUI_CFLAGS) -o $@ $<
ui_%.h: %.ui
	$(UIC) -o $@ $<
qrc_%.cpp: %.qrc
	$(RCC) -name $* -o $@ $<

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

EXTRA_DIST = \
	qtgui2-mingw.pro \
	qtgui2-unx.pro \
	qtgui2.pro
