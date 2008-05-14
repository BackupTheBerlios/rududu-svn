INCLUDES = -I$(top_srcdir)/lib
METASOURCES = AUTO
bin_PROGRAMS = ruddec rudenc
rudenc_SOURCES = rudenc.cpp
rudenc_LDADD = $(top_builddir)/lib/librududu.a -lavcodec -lavformat -lavutil
ruddec_SOURCES = ruddec.cpp
ruddec_LDADD = $(top_builddir)/lib/librududu.a -lSDL
