
AM_CFLAGS = -Wall -I@top_srcdir@ -I@top_builddir@ $(LIBXML_CFLAGS) $(LIBLO_CFLAGS)

lib_LTLIBRARIES = libfreebobctl.la

libfreebobctl_la_SOURCES = client.c freebobctl.c xmlparser.c ipchandler.c
libfreebobctl_la_LIBADD  = -lpthread $(LIBXML_LIBS) $(LIBLO_LIBS)

noinst_HEADERS =
