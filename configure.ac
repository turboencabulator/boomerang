#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Boomerang],[0.3.1])
AC_CONFIG_SRCDIR([boomerang.cpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([include/config.h])
AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])
LT_INIT([dlopen disable-static])

AC_ARG_ENABLE([modules],
	[AS_HELP_STRING(
		[--enable-modules],
		[enable loadable modules for the loader and frontend])],
	[],
	[AS_VAR_SET([enable_modules], [no])])

AC_ARG_ENABLE([gui],
	[AS_HELP_STRING(
		[--disable-gui],
		[do not build the GUI])],
	[],
	[AS_VAR_SET([enable_gui], [yes])])

AC_ARG_WITH([expat],
	[AS_HELP_STRING(
		[--with-expat],
		[enable XML loading and saving])],
	[],
	[AS_VAR_SET([with_expat], [no])])

AC_ARG_WITH([gc],
	[AS_HELP_STRING(
		[--with-gc],
		[enable the garbage collector])],
	[],
	[AS_VAR_SET([with_gc], [no])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
PKG_PROG_PKG_CONFIG

AX_CXX_COMPILE_STDCXX([11])
AC_PROG_CC_C99

AC_PATH_PROG([BISONPP],[bison++])
AC_PATH_PROG([FLEXPP],[flex++])

AS_IF([test "x$enable_gui" = "xyes"], [
	AC_PATH_PROG([MOC],[moc])
	AC_PATH_PROG([UIC],[uic])
	AC_PATH_PROG([RCC],[rcc])
])

# Two paths for frontend/mltk.sh.
AC_PATH_PROG([SML_TOOLKIT],[sml-toolkit],,
	[$PATH:$HOME/sml-nj/bin])
AC_PATH_PROG([UNGENERATE],[ungenerate],,
	[$PATH:$HOME/sml-nj/install])

# Checks for libraries.
AS_IF([test "x$with_expat" = "xyes"], [
	PKG_CHECK_MODULES([EXPAT],[expat])
])
AS_IF([test "x$with_gc" = "xyes"], [
	PKG_CHECK_MODULES([GC],[bdw-gc])
])
AS_IF([test "x$enable_gui" = "xyes"], [
	PKG_CHECK_MODULES([QTGUI],[QtGui])
])
PKG_CHECK_MODULES([CPPUNIT],[cppunit],
	[],
	[AC_MSG_WARN([$CPPUNIT_PKG_ERRORS, needed only for testing])])

# Checks for header files.
#AC_HEADER_STDC
#AC_CHECK_HEADERS(fcntl.h malloc.h sys/time.h unistd.h byteswap.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN

# Checks for library functions.
#AC_CHECK_FUNCS(mkdir rmdir)

AM_CONDITIONAL([DYNAMIC], [test "x$enable_modules" = "xyes"])
AM_CONDITIONAL([GUI], [test "x$enable_gui" = "xyes"])
AM_CONDITIONAL([XML], [test "x$with_expat" = "xyes"])
AM_CONDITIONAL([GC], [test "x$with_gc" = "xyes"])

AS_IF([test "x$enable_modules" = "xyes"], [
	AC_DEFINE([DYNAMIC], [1], [Define to 1 to enable support for modules (requires dlopen)])
	AC_DEFINE([MODPREFIX], [""], [Define to the module name prefix])
	AC_DEFINE([MODSUFFIX], [".so"], [Define to the module name suffix])
])
AS_IF([test "x$with_expat" = "xyes"], [
	AC_DEFINE([USE_XML], [1], [Define to 1 to enable XML loading and saving (requires expat)])
])
AS_IF([test "x$with_gc" = "xyes"], [
	AC_DEFINE([GARBAGE_COLLECTOR], [1], [Define to 1 to enable garbage collection (requires gc)])
])

AC_CONFIG_FILES([
	Doxyfile
	Makefile
	c/Makefile
	codegen/Makefile
	db/Makefile
	frontend/Makefile
	loader/Makefile
	qtgui2/Makefile
	signatures/Makefile
	transform/Makefile
	transformations/Makefile
	type/Makefile
	util/Makefile
])
AC_CONFIG_FILES([frontend/mltk.sh],[chmod +x frontend/mltk.sh])

AC_OUTPUT
