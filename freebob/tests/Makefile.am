noinst_PROGRAMS = byte_order detect_avc_unit stream_format get-xml-description \
                  test-freebob-midi test-freebob-audio test-freebob-iso test-freebob-synctransfer csr1212_read \
                  test-freebob-bounce


byte_order_SOURCES = byte_order.c
detect_avc_unit_SOURCES = detect_avc_unit.c
stream_format_SOURCES = stream_format.cpp
test_freebob_midi_SOURCES = test-freebob-midi.c
test_freebob_audio_SOURCES = test-freebob-audio.c
test_freebob_iso_SOURCES = test-freebob-iso.c
test_freebob_bounce_SOURCES = test-freebob-bounce.c
test_freebob_synctransfer_SOURCES = test-freebob-synctransfer.c
csr1212_read_SOURCES = ../src/csr1212.c ../src/csr1212.h csr1212_read.c
get_xml_description_SOURCES = get-xml-description.c

INCLUDES = $(LIBSIGC_CFLAGS) $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) $(ALSA_CFLAGS) $(LIBLO_CFLAGS)
LDADD    = $(LIBSIGC_LIBS) $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC_LIBS) $(ALSA_LIBS) $(LIBLO_LIBS) -ljack
AM_CFLAGS = "-std=c99"

MAINTAINERCLEANFILES = Makefile.in
