/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

/* freebob_streaming.c
 *
 * Implementation of the FreeBob Streaming API
 *
 */

#include "libfreebob/freebob_streaming.h"
#include "messagebuffer.h"

freebob_device_t *freebob_streaming_init (freebob_device_info_t *device_info, freebob_options_t options) {
	freebob_messagebuffer_init();

	return NULL;

}

void freebob_streaming_finish(freebob_device_t *dev) {

	freebob_messagebuffer_exit();
	
	return;
}

int freebob_streaming_get_nb_capture_streams(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_get_nb_playback_streams(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_get_capture_stream_name(freebob_device_t *dev, int i, char* buffer, size_t buffersize) {
	return -1;
}

int freebob_streaming_get_playback_stream_name(freebob_device_t *dev, int i, char* buffer, size_t buffersize) {
	return -1;
}

freebob_streaming_stream_type freebob_streaming_get_capture_stream_type(freebob_device_t *dev, int i) {
	return freebob_stream_type_invalid;
}

freebob_streaming_stream_type freebob_streaming_get_playback_stream_type(freebob_device_t *dev, int i) {
	return freebob_stream_type_invalid;
}

int freebob_streaming_set_capture_stream_buffer(freebob_device_t *dev, int i, char *buff,  freebob_streaming_buffer_type t) {
	return -1;
}

int freebob_streaming_set_playback_stream_buffer(freebob_device_t *dev, int i, char *buff,  freebob_streaming_buffer_type t) {
	return -1;
}

int freebob_streaming_start(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_stop(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_reset(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_wait(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_transfer_capture_buffers(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_transfer_playback_buffers(freebob_device_t *dev) {
	return 0;
}

int freebob_streaming_transfer_buffers(freebob_device_t *dev) {
	return 0;
	
}


int freebob_streaming_write(freebob_device_t *dev, int i, freebob_sample_t *buffer, int nsamples) {
	return 0;
}

int freebob_streaming_read(freebob_device_t *dev, int i, freebob_sample_t *buffer, int nsamples) {
	return 0;
}

pthread_t freebob_streaming_get_packetizer_thread(freebob_device_t *dev) {
	return 0;
}


