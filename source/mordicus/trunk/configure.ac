dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(ControlCore, 1.0)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

AM_SILENT_RULES([no])

# ------------------------------------------------------------------------------
# Check for tool support
# ------------------------------------------------------------------------------
AC_PROG_CXX
AC_PROG_LIBTOOL

# ------------------------------------------------------------------------------
# Check for C++ support
# ------------------------------------------------------------------------------
AC_LANG([C++])
AC_CHECK_HEADERS([algorithm bitset cassert cctype cmath cstdlib cstring \
   deque exception fstream functional iomanip ios iterator limits list locale \
   map memory ostream set sstream string typeinfo vector], [],
   [AC_MSG_ERROR([missing standard C++ header])])

# ------------------------------------------------------------------------------
# Detect compilation mode: debug or release
# ------------------------------------------------------------------------------
AX_DEBUG_OR_RELEASE_BUILD

# ------------------------------------------------------------------------------
# Check for Mdaq library
# ------------------------------------------------------------------------------
PKG_CHECK_MODULES([MDAQ], [mdaq$DEBUG_SUFFIX >= 1.0.0])
AC_SUBST([MDAQ_CFLAGS])
AC_SUBST([MDAQ_LIBS])
AC_PATH_PROG([PKG_CONFIG], pkg-config,  AC_MSG_ERROR([Could not find pkg-config.]), /usr/local/bin:/usr/bin:/bin)
MDAQ_SLICEDIR="`$PKG_CONFIG --variable=prefix mdaq$DEBUG_SUFFIX`/share/slice/mdaq"
AC_SUBST([MDAQ_SLICEDIR])
AC_MSG_CHECKING([for MDaq include path])
MDAQ_INCDIR="`$PKG_CONFIG --variable=prefix mdaq$DEBUG_SUFFIX`/include/mdaq/hw/server"
AC_SUBST([MDAQ_INCDIR])
AC_MSG_RESULT([$MDAQ_INCDIR])

PKG_CHECK_MODULES([MDAQ_HWSERVER], [mdaq-hwserver$DEBUG_SUFFIX >= 1.0.0])
AC_SUBST([MDAQ_HWSERVER_CFLAGS])
AC_SUBST([MDAQ_HWSERVER_LIBS])

# ------------------------------------------------------------------------------
# Enable Ice implementation
# ------------------------------------------------------------------------------
#AX_LIB_ICE_OR_ICEE
AX_LIB_ICE
if test "$have_ice" = "no" ; then
   AC_MSG_ERROR([ZeroC/Ice is required to build this library.])
fi

#-----------------------------------------------------------------------------
# Check whether simulation mode is enabled
#-------------------------------------------------------------------------------
AC_ARG_ENABLE([simulator],
   AS_HELP_STRING([--enable-simulator], [Turn on simulation mode]),
   [case "${enableval}" in
      yes) enable_simulator=yes ;;
      no)  enable_simulator=no ;;
      *)   AC_MSG_ERROR([bad value ${enableval} for --enable-simulator]) ;;
   esac],
   [enable_simulator=no]
)
AC_MSG_CHECKING([whether to enable simulation mode])
AC_MSG_RESULT([${enable_simulator}])

if test x"$enable_simulator" = x"yes"
then
	CPPFLAGS="$CPPFLAGS -DSIMUL"
fi

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT


