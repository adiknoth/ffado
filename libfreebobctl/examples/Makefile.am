AM_CFLAGS = -Wall -I@top_srcdir@ $(LIBXML_CFLAGS)

noinst_PROGRAMS = test-freebobctl
noinst_HEADERS = 

test_freebobctl_SOURCES = test-freebobctl.c
test_freebobctl_LDADD   = @top_srcdir@/src/libfreebobctl.la $(LIBXML_LIBS)

#example_client_SOURCES = example_client.c
#example_client_LDADD   = @top_srcdir@/src/libfreebobctl.la
