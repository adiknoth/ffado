
AM_CFLAGS = -Wall -I@top_srcdir@ $(LIBXML_CFLAGS)

lib_LTLIBRARIES = libfreebobctl.la

libfreebobctl_la_SOURCES = client.c freebobctl.c xmlparser.c
libfreebobctl_la_LIBADD  = -lpthread $(LIBXML_LIBS)

noinst_HEADERS =
