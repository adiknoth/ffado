
noinst_PROGRAMS = byte_order detect_avc_unit stream_format \
                  test-freebob-midi test-freebob-audio test-freebob-iso csr1212_read

byte_order_SOURCES = byte_order.c
detect_avc_unit_SOURCES = detect_avc_unit.c
stream_format_SOURCES = stream_format.cpp
test_freebob_midi_SOURCES = test-freebob-midi.c
test_freebob_audio_SOURCES = test-freebob-audio.c
test_freebob_iso_SOURCES = test-freebob-iso.c
csr1212_read_SOURCES = ../src/csr1212.c ../src/csr1212.h csr1212_read.c

INCLUDES = $(LIBSIGC_CFLAGS) $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) $(ALSA_CFLAGS) $(LIBLO_CFLAGS)
LDADD    = $(LIBSIGC_LIBS) $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC_LIBS) $(ALSA_LIBS) $(LIBLO_LIBS)
AM_CFLAGS = "-std=c99"

MAINTAINERCLEANFILES = Makefile.in
