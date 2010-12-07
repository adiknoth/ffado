asound_module_pcm_freebob_LTLIBRARIES = libasound_module_pcm_freebob.la

asound_module_pcm_freebobdir = $(libdir)/alsa-lib

AM_CFLAGS = -Wall -g @ALSA_CFLAGS@ $(LIBFREEBOB_CFLAGS)
AM_LDFLAGS = -module -avoid-version -export-dynamic

libasound_module_pcm_freebob_la_SOURCES = pcm_freebob.c
libasound_module_pcm_freebob_la_LIBADD = @ALSA_LIBS@ $(LIBFREEBOB_LIBS)
