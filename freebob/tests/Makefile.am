
noinst_PROGRAMS = byte_order detect_avc_unit \
                  test-freebob-midi test-freebob-audio

byte_order_SOURCES = byte_order.c
detect_avc_unit_SOURCES = detect_avc_unit.c
test_freebob_midi_SOURCES = test-freebob-midi.c
test_freebob_audio_SOURCES = test-freebob-audio.c

INCLUDES = $(LIBSIGC_CFLAGS) $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) $(ALSA_CFLAGS)
LDADD    = $(LIBSIGC_LIBS) $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC_LIBS) $(ALSA_LIBS)
AM_CFLAGS = "-std=c99"

MAINTAINERCLEANFILES = Makefile.in
