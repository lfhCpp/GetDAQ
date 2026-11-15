include $(top_srcdir)/am/ice.am
SPPFLAGS = -I$(MDAQ_SLICEDIR)
AM_CPPFLAGS = $(MDAQ_CFLAGS) $(MDAQ_HWSERVER_CFLAGS) -I$(MDAQ_INCDIR)

slice_gen_sources = ECC.h ECC.cpp
slice_sources = ECC.ice

bin_PROGRAMS=eccServer eccClient eccLoad

eccServer_SOURCES= \
	EccServerMain.cpp \
	$(slice_gen_sources) \
	ElectronicsControlCoreImpl.cpp \
	ElectronicsControlCoreImpl.h \
	ServerDevices.cpp
eccServer_CPPFLAGS = \
	-I@top_builddir@/src \
	-I@top_srcdir@/src \
	$(ICE_CPPFLAGS) \
	$(MDAQ_CFLAGS) \
	$(MDAQ_HWSERVER_CFLAGS) \
	-I$(MDAQ_INCDIR)
eccServer_LDFLAGS =  $(ICE_LDFLAGS)
eccServer_LDADD = $(ICE_LIBS) $(MDAQ_LIBS) $(MDAQ_HWSERVER_LIBS)

eccClient_SOURCES= \
	EccClientMain.cpp \
	$(slice_gen_sources) \
	ElectronicsControlCoreImpl.cpp \
	ElectronicsControlCoreImpl.h
eccClient_CPPFLAGS = \
	-I@top_builddir@/src \
	-I@top_srcdir@/src \
	$(ICE_CPPFLAGS) \
	$(MDAQ_CFLAGS)	\
	$(MDAQ_HWSERVER_CFLAGS) \
	-I$(MDAQ_INCDIR)
eccClient_LDFLAGS =  $(ICE_LDFLAGS)
eccClient_LDADD = $(ICE_LIBS) $(MDAQ_LIBS) $(MDAQ_HWSERVER_LIBS)

eccLoad_SOURCES= \
	LoadConfig.cpp \
	$(slice_gen_sources) \
	ElectronicsControlCoreImpl.cpp \
	ElectronicsControlCoreImpl.h \
	FullConfigEcc.cpp \
	FullConfigEcc.h 
	
eccLoad_CPPFLAGS = \
	-I@top_builddir@/src \
	-I@top_srcdir@/src \
	$(ICE_CPPFLAGS) \
	$(MDAQ_CFLAGS) \
	$(MDAQ_HWSERVER_CFLAGS) \
	-I$(MDAQ_INCDIR)
eccLoad_LDFLAGS =  $(ICE_LDFLAGS)
eccLoad_LDADD = $(ICE_LIBS) $(MDAQ_LIBS) $(MDAQ_HWSERVER_LIBS)

BUILT_SOURCES	= $(slice_gen_sources)
CLEANFILES		= $(BUILT_SOURCES)
EXTRA_DIST 		= $(slice_sources)