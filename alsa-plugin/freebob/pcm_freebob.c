/*
 *  ALSA Freebob Plugin
 *
 *  Copyright (c) 2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   Based upon the JACK <-> ALSA plugin
 *    Copyright (c) 2003 by Maarten de Boer <mdeboer@iua.upf.es>
 *                  2005 Takashi Iwai <tiwai@suse.de>
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <byteswap.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#include <libfreebob/freebob.h>
#include <libfreebob/freebob_streaming.h>

#include <pthread.h>

#define FREEBOB_PLUGIN_VERSION "0.0.8"

#define FREEBOB_USE_RT 			1
#define FREEBOB_RT_PRIORITY_PACKETIZER 	60
// midi priority should be higher than the audio priority in order to
// make sure events are not only delivered on period boundarys
// but I think it should be smaller than the packetizer thread in order not 
// to lose any packets
#define FREEBOB_RT_PRIORITY_MIDI 	59

// undef this to disable midi support
#define FREEBOB_WITH_MIDI

#ifdef FREEBOB_WITH_MIDI

	#define ALSA_SEQ_BUFF_SIZE 1024
	#define MIDI_TRANSMIT_BUFFER_SIZE 1024
	#define MIDI_THREAD_SLEEP_TIME_USECS 1000
	
	typedef struct {
		int stream_nr;
		int seq_port_nr;
		snd_midi_event_t *parser;
		snd_seq_t *seq_handle;
	} freebob_midi_port_t;
	
	typedef struct _snd_pcm_freebob_midi_handle {
		freebob_device_t *dev;
		
		snd_seq_t *seq_handle;
		
		pthread_t queue_thread;
		pthread_t dequeue_thread;
		int queue_thread_realtime;
		int queue_thread_priority;
	
		int nb_input_ports;
		int nb_output_ports;
	
		freebob_midi_port_t **input_ports;
		freebob_midi_port_t **output_ports;
	
		freebob_midi_port_t **input_stream_port_map;
		int *output_port_stream_map;
	
	
	} snd_pcm_freebob_midi_handle_t;
	

#endif

// #define PRINT_FUNCTION_ENTRY (printf("entering %s\n",__FUNCTION__))
#define PRINT_FUNCTION_ENTRY
typedef struct {
	snd_pcm_ioplug_t io;

	int fd;
	int activated;

	unsigned int hw_ptr;
	unsigned int sample_bits;

	unsigned int channels;
	snd_pcm_channel_area_t *areas;

	freebob_device_t *streaming_device;
	freebob_handle_t fb_handle;

	// options
	freebob_options_t dev_options;
	
	snd_pcm_stream_t stream;

	// thread for polling
	pthread_t thread;

#ifdef FREEBOB_WITH_MIDI
	snd_pcm_freebob_midi_handle_t *midi_handle;
#endif

} snd_pcm_freebob_t;

#ifdef FREEBOB_WITH_MIDI
	static snd_pcm_freebob_midi_handle_t *snd_pcm_freebob_midi_init(snd_pcm_freebob_t *driver);
	static void snd_pcm_freebob_midi_finish (snd_pcm_freebob_midi_handle_t *m);
#endif

static int snd_pcm_freebob_hw_params(snd_pcm_ioplug_t *io, snd_pcm_hw_params_t *params) {

	PRINT_FUNCTION_ENTRY;

	snd_pcm_freebob_t *freebob = io->private_data;
	unsigned int i=0;

	freebob->channels=0;
	if (freebob->stream == SND_PCM_STREAM_PLAYBACK) {
		//fixme: assumes that the audio streams are always the first streams
		for (i=0;i < (unsigned int)freebob_streaming_get_nb_playback_streams(freebob->streaming_device);i++) {
			if(freebob_streaming_get_playback_stream_type(freebob->streaming_device,i)==freebob_stream_type_audio) {
				freebob->channels++;
			}
		}
		for (i=0;i<freebob->channels;i++) {
			// setup buffers, currently left at default

		}
	} else {
		//fixme: assumes that the audio streams are always the first streams
		for (i=0;i < (unsigned int)freebob_streaming_get_nb_capture_streams(freebob->streaming_device);i++) {
			if(freebob_streaming_get_capture_stream_type(freebob->streaming_device,i)==freebob_stream_type_audio) {
				freebob->channels++;
			}
		}
		for (i=0;i<freebob->channels;i++) {
			// setup buffers, currently left at default

		}
	}

	return 0;
}

static int
snd_pcm_freebob_pollfunction(snd_pcm_freebob_t *freebob)
{

//  	PRINT_FUNCTION_ENTRY;
	int retval;
	static char buf[1];
	snd_pcm_ioplug_t *io=&freebob->io;
	const snd_pcm_channel_area_t *areas;
	unsigned int i;

	assert(freebob);
	assert(freebob->streaming_device);

	retval = freebob_streaming_wait(freebob->streaming_device);
	if (retval < 0) {
		printf("Xrun\n");
		freebob_streaming_reset(freebob->streaming_device);
		return 0;
	}

	areas = snd_pcm_ioplug_mmap_areas(io);

	snd_pcm_uframes_t offset = freebob->hw_ptr;

	for (i=0;i<freebob->channels;i++) {
		freebob_sample_t *buff=(freebob_sample_t *)((char *)areas[i].addr + (areas[i].step * offset / 8));
// 		printf("%d: %p %d %d %p\n",i, areas[i].addr, offset, areas[i].step, buff);

		if (freebob->stream == SND_PCM_STREAM_PLAYBACK) {
			freebob_streaming_set_playback_stream_buffer(freebob->streaming_device, i, (char *)buff, freebob_buffer_type_uint24);
		} else {
			freebob_streaming_set_capture_stream_buffer(freebob->streaming_device, i, (char *)buff, freebob_buffer_type_uint24);
		}
	}

	if (freebob->stream == SND_PCM_STREAM_PLAYBACK) {
		freebob_streaming_transfer_playback_buffers(freebob->streaming_device);
	} else {
		freebob_streaming_transfer_capture_buffers(freebob->streaming_device);
	}

	freebob->hw_ptr += freebob->dev_options.period_size;
	freebob->hw_ptr %= io->buffer_size;

	write(freebob->fd, buf, 1); /* for polling */

	return 0;
}

static void * freebob_workthread(void *arg)
{
	PRINT_FUNCTION_ENTRY;
	snd_pcm_freebob_t *freebob = (snd_pcm_freebob_t *)arg;

	int oldstate;

	pthread_setcancelstate (PTHREAD_CANCEL_DEFERRED, &oldstate);

	while (1) {
		snd_pcm_freebob_pollfunction(freebob);
		pthread_testcancel();
	}

}

static int snd_pcm_freebob_poll_revents(snd_pcm_ioplug_t *io,
				     struct pollfd *pfds, unsigned int nfds,
				     unsigned short *revents)
{
	static char buf[1];

	PRINT_FUNCTION_ENTRY;
	
	assert(pfds && nfds == 1 && revents);

	read(pfds[0].fd, buf, 1);

	*revents = pfds[0].revents;
	return 0;
}

static void snd_pcm_freebob_free(snd_pcm_freebob_t *freebob)
{
	PRINT_FUNCTION_ENTRY;
	if (freebob) {
		if(freebob->fb_handle)
			freebob_destroy_handle(freebob->fb_handle );

		close(freebob->fd);
		close(freebob->io.poll_fd);
		free(freebob);
	}
}

static int snd_pcm_freebob_close(snd_pcm_ioplug_t *io)
{
	PRINT_FUNCTION_ENTRY;
	snd_pcm_freebob_t *freebob = io->private_data;

	freebob_streaming_finish(freebob->streaming_device);

#ifdef FREEBOB_WITH_MIDI
	snd_pcm_freebob_midi_finish(freebob->midi_handle);	
#endif	

	snd_pcm_freebob_free(freebob);
	return 0;
}

static snd_pcm_sframes_t snd_pcm_freebob_pointer(snd_pcm_ioplug_t *io)
{
	PRINT_FUNCTION_ENTRY;

	snd_pcm_freebob_t *freebob = io->private_data;

	return freebob->hw_ptr;
}

static int snd_pcm_freebob_start(snd_pcm_ioplug_t *io)
{
	int result = 0;
	snd_pcm_freebob_t *freebob = io->private_data;

	PRINT_FUNCTION_ENTRY;

	result = pthread_create (&freebob->thread, 0, freebob_workthread, freebob);
	if(result) return result;

	return freebob_streaming_start(freebob->streaming_device);
}

static int snd_pcm_freebob_stop(snd_pcm_ioplug_t *io)
{
	snd_pcm_freebob_t *freebob = io->private_data;

	PRINT_FUNCTION_ENTRY;

	if(pthread_cancel(freebob->thread)) {
		fprintf(stderr,"could not cancel thread!\n");
	}

	if(pthread_join(freebob->thread,NULL)) {
		fprintf(stderr,"could not join thread!\n");
	}

	return freebob_streaming_stop(freebob->streaming_device);
}

static int snd_pcm_freebob_prepare(snd_pcm_ioplug_t *io)
{
// 	snd_pcm_freebob_t *freebob = io->private_data;
	PRINT_FUNCTION_ENTRY;

	return 0;
}

static snd_pcm_ioplug_callback_t freebob_pcm_callback = {
	.close = snd_pcm_freebob_close,
	.start = snd_pcm_freebob_start,
	.stop = snd_pcm_freebob_stop,
	.pointer = snd_pcm_freebob_pointer,
	.hw_params = snd_pcm_freebob_hw_params,
 	.prepare = snd_pcm_freebob_prepare,
	.poll_revents = snd_pcm_freebob_poll_revents,
};

#define ARRAY_SIZE(ary)	(sizeof(ary)/sizeof(ary[0]))

static int freebob_set_hw_constraint(snd_pcm_freebob_t *freebob)
{
	PRINT_FUNCTION_ENTRY;
	unsigned int access_list[] = {
 		SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
		SND_PCM_ACCESS_RW_NONINTERLEAVED,
	};

	unsigned int *rate_list=NULL;

	unsigned int format = SND_PCM_FORMAT_S24;
	int err;
	int i;

	// FIXME: this restricts the plugin to the first discovered device!
	freebob->dev_options.node_id=freebob_get_device_node_id(freebob->fb_handle, 0);

	freebob_supported_stream_format_info_t* stream_info;
	
// 	printf("  port = %d, node_id = %d\n", freebob->dev_options.port, freebob->dev_options.node_id);

	// find the supported samplerate nb channels combinations
	stream_info = freebob_get_supported_stream_format_info( freebob->fb_handle, 
								freebob->dev_options.node_id, 
								freebob->stream == SND_PCM_STREAM_PLAYBACK ? 1 : 0 );

// 	debug statement
// 	freebob_print_supported_stream_format_info( stream_info );	

	if(!(rate_list=calloc(stream_info->nb_formats, sizeof(unsigned int)))) {
		SNDERR("Could not allocate sample rate list!\n");
		return -ENOMEM;
	}

	int min_channels=99999;
	int max_channels=0;

	for (i=0;i<stream_info->nb_formats;i++) {
		freebob_supported_stream_format_spec_t* format_spec = stream_info->formats[i];
	
		if ( format_spec ) {
			if (format_spec->nb_audio_channels < min_channels)
				min_channels=format_spec->nb_audio_channels;

			if (format_spec->nb_audio_channels > max_channels)
				max_channels=format_spec->nb_audio_channels;

			rate_list[i]=format_spec->samplerate;
		}
	}

// 	SNDERR("minchannels: %d \n",min_channels);
// 	SNDERR("maxchannels: %d \n",max_channels);

	freebob_free_supported_stream_format_info( stream_info );

	freebob->sample_bits = snd_pcm_format_physical_width(format);

	// setup the plugin capabilities
	if ((err = snd_pcm_ioplug_set_param_list(&freebob->io, SND_PCM_IOPLUG_HW_ACCESS,
						 ARRAY_SIZE(access_list), access_list)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_list(&freebob->io, SND_PCM_IOPLUG_HW_RATE,
						 ARRAY_SIZE(rate_list), rate_list)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_list(&freebob->io, SND_PCM_IOPLUG_HW_FORMAT,
						 1, &format)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_minmax(&freebob->io, SND_PCM_IOPLUG_HW_CHANNELS,
						   max_channels, max_channels)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_minmax(&freebob->io, SND_PCM_IOPLUG_HW_PERIOD_BYTES,
						   512*8, 512*8)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_minmax(&freebob->io, SND_PCM_IOPLUG_HW_PERIODS,
						   3, 64)) < 0)
		return err;

	free(rate_list);

	return 0;
}

static int snd_pcm_freebob_open(snd_pcm_t **pcmp, const char *name,
			     freebob_options_t *dev_options,
			     snd_pcm_stream_t stream, int mode)
{

	PRINT_FUNCTION_ENTRY;
	snd_pcm_freebob_t *freebob;
	int err;
	int fd[2];

	printf("FreeBob plugin for ALSA\n  version %s compiled %s %s\n  using %s\n", 
		FREEBOB_PLUGIN_VERSION, __DATE__, __TIME__, freebob_get_version());
	
	assert(pcmp);
	assert(dev_options);

	freebob = calloc(1, sizeof(*freebob));
	if (!freebob)
		return -ENOMEM;

	memcpy(&freebob->dev_options,dev_options,sizeof(freebob->dev_options));

	freebob->stream=stream;

	// discover the devices to discover the capabilities
	freebob->fb_handle = freebob_new_handle( freebob->dev_options.port );
	if ( !freebob->fb_handle ) {
	    SNDERR("Could not create freebob handle\n" );
	    return -ENOMEM;
	}
		
 	if ( freebob_discover_devices( freebob->fb_handle, 0 ) != 0 ) {
	    SNDERR("Could not discover devices\n" );
	    freebob_destroy_handle( freebob->fb_handle );
	    return -EINVAL;
	}

	freebob_device_info_t device_info;

 	freebob->dev_options.directions=0;
 
 	if(stream == SND_PCM_STREAM_PLAYBACK) {
 		freebob->dev_options.directions |= FREEBOB_IGNORE_CAPTURE;
 	} else {
 		freebob->dev_options.directions |= FREEBOB_IGNORE_PLAYBACK;
 	}

	freebob->streaming_device=freebob_streaming_init(&device_info,freebob->dev_options);

	if(!freebob->streaming_device) {
		SNDERR("FREEBOB: Error creating virtual device\n");
		freebob_destroy_handle( freebob->fb_handle );
		return -EINVAL;
	}

#ifdef FREEBOB_WITH_MIDI
	freebob->midi_handle=snd_pcm_freebob_midi_init(freebob);
	if(!freebob->midi_handle) {
		SNDERR("FREEBOB: Error creating midi device");
		freebob_destroy_handle( freebob->fb_handle );
		return -EINVAL;
	}
#endif

	socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
	
	freebob->fd = fd[0];

	freebob->io.version = SND_PCM_IOPLUG_VERSION;
	freebob->io.name = "FreeBob PCM Plugin";
	freebob->io.callback = &freebob_pcm_callback;
	freebob->io.private_data = freebob;
	freebob->io.mmap_rw = 1;
	freebob->io.poll_fd = fd[1];
	freebob->io.poll_events = stream == SND_PCM_STREAM_PLAYBACK ? POLLOUT : POLLIN;

	err = snd_pcm_ioplug_create(&freebob->io, name, stream, mode);
	if (err < 0) {
		snd_pcm_freebob_free(freebob);
		return err;
	}

	err = freebob_set_hw_constraint(freebob);
	if (err < 0) {
		snd_pcm_ioplug_delete(&freebob->io);
		return err;
	}

	*pcmp = freebob->io.pcm;

	return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(freebob)
{
	snd_config_iterator_t i, next;
	int err;
	long tmp_int;

	freebob_options_t dev_options;
	
	dev_options.node_id=0;
	dev_options.port=0;

	dev_options.sample_rate=44100;
	dev_options.period_size=512;
	dev_options.nb_buffers=3;

	/* packetizer thread options */

	dev_options.realtime=FREEBOB_USE_RT;
	dev_options.packetizer_priority=FREEBOB_RT_PRIORITY_PACKETIZER;

	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0)
			continue;
		if (strcmp(id, "port") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_INTEGER) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			if (snd_config_get_integer(n,&tmp_int)) {
				SNDERR("Could not get value for %s", id);
				return -EINVAL;
			}
			dev_options.port=tmp_int;
			continue;
		}
		if (strcmp(id, "node") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_INTEGER) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			if (snd_config_get_integer(n,&tmp_int)) {
				SNDERR("Could not get value for %s", id);
				return -EINVAL;
			}
			dev_options.node_id=tmp_int;
			continue;
		}

		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}

	err = snd_pcm_freebob_open(pcmp, name, &dev_options, stream, mode);

	return err;

}

SND_PCM_PLUGIN_SYMBOL(freebob);

// midi stuff
#ifdef FREEBOB_WITH_MIDI
/*
 * MIDI support
 */ 

// the thread that will queue the midi events from the seq to the stream buffers

void * snd_pcm_freebob_midi_queue_thread(void *arg)
{
	snd_pcm_freebob_midi_handle_t *m=(snd_pcm_freebob_midi_handle_t *)arg;
	assert(m);
	snd_seq_event_t *ev;
	unsigned char work_buffer[MIDI_TRANSMIT_BUFFER_SIZE];
	int bytes_to_send;
	int b;
	int i;

	SNDERR ("FREEBOB: MIDI queue thread started");

	while(1) {
		// get next event, if one is present
		while ((snd_seq_event_input(m->seq_handle, &ev) > 0)) {
			// get the port this event is originated from
			freebob_midi_port_t *port=NULL;
			for (i=0;i<m->nb_output_ports;i++) {
				if(m->output_ports[i]->seq_port_nr == ev->dest.port) {
					port=m->output_ports[i];
					break;
				}
			}
	
			if(!port) {
				SNDERR ("FREEBOB: Could not find target port for event: dst=%d src=%d\n", ev->dest.port, ev->source.port);

				break;
			}
			
			// decode it to the work buffer
			if((bytes_to_send = snd_midi_event_decode ( port->parser, 
				work_buffer,
				MIDI_TRANSMIT_BUFFER_SIZE, 
				ev))<0) 
			{ // failed
				SNDERR ("FREEBOB: Error decoding event for port %d (errcode=%d)\n", port->seq_port_nr,bytes_to_send);
				bytes_to_send=0;
				//return -1;
			}
	
			for(b=0;b<bytes_to_send;b++) {
				freebob_sample_t tmp_event=work_buffer[b];
				if(freebob_streaming_write(m->dev, port->stream_nr, &tmp_event, 1)<1) {
					SNDERR ("FREEBOB: Midi send buffer overrun\n");
				}
			}
	
		}

		// sleep for some time
		usleep(MIDI_THREAD_SLEEP_TIME_USECS);
	}
	return NULL;
}

// the dequeue thread (maybe we need one thread per stream)
void *snd_pcm_freebob_midi_dequeue_thread (void *arg) {
	snd_pcm_freebob_midi_handle_t *m=(snd_pcm_freebob_midi_handle_t *)arg;

	int i;
	int s;
	
	int samples_read;

	assert(m);

	while(1) {
		// read incoming events
	
		for (i=0;i<m->nb_input_ports;i++) {
			unsigned int buff[64];
	
			freebob_midi_port_t *port=m->input_ports[i];
		
			if(!port) {
				SNDERR("FREEBOB: something went wrong when setting up the midi input port map (%d)",i);
			}
		
			do {
				samples_read=freebob_streaming_read(m->dev, port->stream_nr, buff, 64);
			
				for (s=0;s<samples_read;s++) {
					unsigned int *byte=(buff+s) ;
					snd_seq_event_t ev;
					if ((snd_midi_event_encode_byte(port->parser,(*byte) & 0xFF, &ev)) > 0) {
						// a midi message is complete, send it out to ALSA
						snd_seq_ev_set_subs(&ev);  
						snd_seq_ev_set_direct(&ev);
						snd_seq_ev_set_source(&ev, port->seq_port_nr);
						snd_seq_event_output_direct(port->seq_handle, &ev);						
					}
				}
			} while (samples_read>0);
		}

		// sleep for some time
		usleep(MIDI_THREAD_SLEEP_TIME_USECS);
	}
	return NULL;
}

static snd_pcm_freebob_midi_handle_t *snd_pcm_freebob_midi_init(snd_pcm_freebob_t *freebob) {
// 	int err;

	char buf[256];
	int chn;
	int nchannels;
	int i=0;

	freebob_device_t *dev=freebob->streaming_device;

	assert(dev);

	snd_pcm_freebob_midi_handle_t *m=calloc(1,sizeof(snd_pcm_freebob_midi_handle_t));
	if (!m) {
		SNDERR("FREEBOB: not enough memory to create midi structure");
		return NULL;
	}

	if (snd_seq_open(&m->seq_handle, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) {
		SNDERR("FREEBOB: Error opening ALSA sequencer.");
		free(m);
		return NULL;
	}

	snd_seq_set_client_name(m->seq_handle, "FreeBob MIDI");

	// find out the number of midi in/out ports we need to setup
	nchannels=freebob_streaming_get_nb_capture_streams(dev);

	m->nb_input_ports=0;

	for (chn = 0; chn < nchannels; chn++) {	
		if(freebob_streaming_get_capture_stream_type(dev, chn) == freebob_stream_type_midi) {
			m->nb_input_ports++;
		}
	}

	m->input_ports=calloc(m->nb_input_ports,sizeof(freebob_midi_port_t *));
	if(!m->input_ports) {
		SNDERR("FREEBOB: not enough memory to create midi structure");
		free(m);
		return NULL;
	}

	i=0;
	for (chn = 0; chn < nchannels; chn++) {
		if(freebob_streaming_get_capture_stream_type(dev, chn) == freebob_stream_type_midi) {
			m->input_ports[i]=calloc(1,sizeof(freebob_midi_port_t));
			if(!m->input_ports[i]) {
				// fixme
				SNDERR("FREEBOB: Could not allocate memory for seq port");
				continue;
			}

	 		freebob_streaming_get_capture_stream_name(dev, chn, buf, sizeof(buf) - 1);
			SNDERR("FREEBOB: Register MIDI IN port %s\n", buf);

			m->input_ports[i]->seq_port_nr=snd_seq_create_simple_port(m->seq_handle, buf,
				SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
				SND_SEQ_PORT_TYPE_MIDI_GENERIC);

			if(m->input_ports[i]->seq_port_nr<0) {
				SNDERR("FREEBOB: Could not create seq port");
				m->input_ports[i]->stream_nr=-1;
				m->input_ports[i]->seq_port_nr=-1;
			} else {
				m->input_ports[i]->stream_nr=chn;
				m->input_ports[i]->seq_handle=m->seq_handle;
				if (snd_midi_event_new  ( ALSA_SEQ_BUFF_SIZE, &(m->input_ports[i]->parser)) < 0) {
					fprintf(stderr, "FREEBOB: could not init parser for MIDI IN port %d\n",i);
					m->input_ports[i]->stream_nr=-1;
					m->input_ports[i]->seq_port_nr=-1;
				}
			}

			i++;
		}
	}

	// playback
	nchannels=freebob_streaming_get_nb_playback_streams(dev);

	m->nb_output_ports=0;

	for (chn = 0; chn < nchannels; chn++) {	
		if(freebob_streaming_get_playback_stream_type(dev, chn) == freebob_stream_type_midi) {
			m->nb_output_ports++;
		}
	}

	m->output_ports=calloc(m->nb_output_ports,sizeof(freebob_midi_port_t *));
	if(!m->output_ports) {
		SNDERR("FREEBOB: not enough memory to create midi structure");
		for (i = 0; i < m->nb_input_ports; i++) {	
			free(m->input_ports[i]);
		}
		free(m->input_ports);
		free(m);
		return NULL;
	}

	i=0;
	for (chn = 0; chn < nchannels; chn++) {
		if(freebob_streaming_get_playback_stream_type(dev, chn) == freebob_stream_type_midi) {
			m->output_ports[i]=calloc(1,sizeof(freebob_midi_port_t));
			if(!m->output_ports[i]) {
				// fixme
				SNDERR("FREEBOB: Could not allocate memory for seq port");
				continue;
			}

	 		freebob_streaming_get_playback_stream_name(dev, chn, buf, sizeof(buf) - 1);
			SNDERR("FREEBOB: Register MIDI OUT port %s\n", buf);

			m->output_ports[i]->seq_port_nr=snd_seq_create_simple_port(m->seq_handle, buf,
				SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
              			SND_SEQ_PORT_TYPE_MIDI_GENERIC);


			if(m->output_ports[i]->seq_port_nr<0) {
				SNDERR("FREEBOB: Could not create seq port");
				m->output_ports[i]->stream_nr=-1;
				m->output_ports[i]->seq_port_nr=-1;
			} else {
				m->output_ports[i]->stream_nr=chn;
				m->output_ports[i]->seq_handle=m->seq_handle;
				if (snd_midi_event_new  ( ALSA_SEQ_BUFF_SIZE, &(m->output_ports[i]->parser)) < 0) {
					fprintf(stderr, "FREEBOB: could not init parser for MIDI OUT port %d\n",i);
					m->output_ports[i]->stream_nr=-1;
					m->output_ports[i]->seq_port_nr=-1;
				}
			}

			i++;
		}
	}


	// start threads
	m->dev=dev;

 	m->queue_thread_priority=FREEBOB_RT_PRIORITY_MIDI;
	m->queue_thread_realtime=FREEBOB_USE_RT;

	if (pthread_create (&m->queue_thread, 0, snd_pcm_freebob_midi_queue_thread, (void *)m)) {
		SNDERR("FREEBOB: cannot create midi queueing thread");
		free(m);
		return NULL;
	}
	
	if (pthread_create (&m->dequeue_thread, 0, snd_pcm_freebob_midi_dequeue_thread, (void *)m)) {
		SNDERR("FREEBOB: cannot create midi dequeueing thread");
		free(m);
		return NULL;
	}
	return m;
}

static void
snd_pcm_freebob_midi_finish (snd_pcm_freebob_midi_handle_t *m)
{
	assert(m);

	int i;

	pthread_cancel (m->queue_thread);
	pthread_join (m->queue_thread, NULL);

	pthread_cancel (m->dequeue_thread);
	pthread_join (m->dequeue_thread, NULL);


	for (i=0;i<m->nb_input_ports;i++) {
		free(m->input_ports[i]);

	}
	free(m->input_ports);

	for (i=0;i<m->nb_output_ports;i++) {
		free(m->output_ports[i]);
	}
	free(m->output_ports);

	free(m);
}
#endif	
