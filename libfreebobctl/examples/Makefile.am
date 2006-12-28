AM_CFLAGS = -Wall -I@top_srcdir@ -I@top_builddir@ $(LIBXML_CFLAGS) $(LIBLO_CFLAGS)

noinst_PROGRAMS = test-freebobctl test-freebobctl-osc
noinst_HEADERS = 

test_freebobctl_SOURCES = test-freebobctl.c
test_freebobctl_LDADD   = @top_builddir@/src/libfreebobctl.la $(LIBXML_LIBS) $(LIBLO_LIBS)

test_freebobctl_osc_SOURCES = test-freebobctl-osc.c
test_freebobctl_osc_LDADD   = @top_builddir@/src/libfreebobctl.la $(LIBXML_LIBS) $(LIBLO_LIBS)

#example_client_SOURCES = example_client.c
#example_client_LDADD   = @top_srcdir@/src/libfreebobctl.la
