#------------------------------------------------------------------------------
# Process this file with automake to produce Makefile.in.
#------------------------------------------------------------------------------

bin_PROGRAMS    = proz

AM_CPPFLAGS = @CPPFLAGS@ -fno-inline -DLOCALEDIR=\"$(datadir)/locale\"

proz_SOURCES = main.cpp misc.cpp download_win.cpp init.cpp prefs.cpp ftpsearch_win.cpp interface.c 
INCLUDES        = -I. -I.. -I../libprozilla/src -I../intl -I@includedir@ 
AM_CFLAGS          =  @CFLAGS@ -D_REENTRANT -W -Wall -ggdb  
AM_CFLAGS         += -DGLOBAL_CONF_FILE="\"@sysconfdir@/prozilla.conf\"" -DLOCALEDIR=\"$(datadir)/locale\"
#AM_CFLAGS	  += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
AM_LDFLAGS		= @LDFLAGS@ 
LDADD           = ../libprozilla/src/libprozilla.la -L@libdir@ 
LIBS            = @LIBS@ $(THREAD_LIBS) 
