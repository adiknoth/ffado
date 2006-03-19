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

#define FREEBOB_PLUGIN_VERSION "0.0.2"

#define PRINT_FUNCTION_ENTRY (printf("entering %s\n",__FUNCTION__))

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


} snd_pcm_freebob_t;

static snd_pcm_sframes_t snd_pcm_freebob_write(snd_pcm_ioplug_t *io,
				   const snd_pcm_channel_area_t *areas,
				   snd_pcm_uframes_t offset,
				   snd_pcm_uframes_t size)
{
	PRINT_FUNCTION_ENTRY;

	int retval=0;
	int i=0;
	
	snd_pcm_sframes_t frames;

	snd_pcm_freebob_t *freebob = io->private_data;
	const char *buf;
	ssize_t result;

	int sampleswritten;

	retval = freebob_streaming_wait(freebob->streaming_device);
	if (retval < 0) {
		fprintf(stderr,"Xrun\n");
		freebob_streaming_reset(freebob->streaming_device);
		return 0;
	}

	freebob_streaming_transfer_playback_buffers(freebob->streaming_device);

	for(i=0;i<freebob->channels;i++) {
		freebob_sample_t *buff=(freebob_sample_t *)((char *)areas->addr + (areas->first + areas->step * offset) / 8);
		sampleswritten=freebob_streaming_write(freebob->streaming_device, i, buff, freebob->dev_options.period_size);
	}

	frames = sampleswritten / freebob->channels;

	freebob->hw_ptr += frames;
	freebob->hw_ptr %= io->buffer_size;

	return frames ;
}

static int snd_pcm_freebob_hw_params(snd_pcm_ioplug_t *io, snd_pcm_hw_params_t *params) {

	PRINT_FUNCTION_ENTRY;

	snd_pcm_freebob_t *freebob = io->private_data;
	int i=0;

	freebob->channels=0;
	if (freebob->stream == SND_PCM_STREAM_PLAYBACK) {
		//fixme: assumes that the audio streams are always the first streams
		for (i=0;i<freebob_streaming_get_nb_playback_streams(freebob->streaming_device);i++) {
			if(freebob_streaming_get_playback_stream_type(freebob->streaming_device,i)) {
				freebob->channels++;
			}
		}
		for (i=0;i<freebob->channels;i++) {
			// setup buffers, currently left at default
		}
	} else {
		//TODO: implement capture
	}

	freebob_streaming_start(freebob->streaming_device);

	return 0;
}

static void snd_pcm_freebob_free(snd_pcm_freebob_t *freebob)
{
	PRINT_FUNCTION_ENTRY;
	if (freebob) {
// 		if (freebob->client)
// 			jack_client_close(freebob->client);
		if(freebob->fb_handle)
			freebob_destroy_handle(freebob->fb_handle );

		close(freebob->fd);
		close(freebob->io.poll_fd);
//		free(freebob->areas);
		free(freebob);
	}
}

static int snd_pcm_freebob_close(snd_pcm_ioplug_t *io)
{
	PRINT_FUNCTION_ENTRY;
	snd_pcm_freebob_t *freebob = io->private_data;
	snd_pcm_freebob_free(freebob);
	return 0;
}

static snd_pcm_sframes_t snd_pcm_freebob_pointer(snd_pcm_ioplug_t *io)
{
	PRINT_FUNCTION_ENTRY;

	snd_pcm_freebob_t *freebob = io->private_data;

	printf("%d\n", freebob->hw_ptr);
	return freebob->hw_ptr;
}

static int snd_pcm_freebob_start(snd_pcm_ioplug_t *io)
{
	PRINT_FUNCTION_ENTRY;

	snd_pcm_freebob_t *freebob = io->private_data;
	return freebob_streaming_start(freebob->streaming_device);
}

static int snd_pcm_freebob_stop(snd_pcm_ioplug_t *io)
{
	PRINT_FUNCTION_ENTRY;
	snd_pcm_freebob_t *freebob = io->private_data;

	return freebob_streaming_stop(freebob->streaming_device);
}

static snd_pcm_ioplug_callback_t freebob_pcm_callback = {
	.close = snd_pcm_freebob_close,
	.start = snd_pcm_freebob_start,
	.stop = snd_pcm_freebob_stop,
	.pointer = snd_pcm_freebob_pointer,
	.transfer = snd_pcm_freebob_write,
	.hw_params = snd_pcm_freebob_hw_params,
// 	.prepare = snd_pcm_freebob_prepare,
//	.poll_revents = snd_pcm_freebob_poll_revents,
};

#define ARRAY_SIZE(ary)	(sizeof(ary)/sizeof(ary[0]))

static int freebob_set_hw_constraint(snd_pcm_freebob_t *freebob)
{
	PRINT_FUNCTION_ENTRY;
	unsigned int access_list[] = {
		SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
//		SND_PCM_ACCESS_RW_NONINTERLEAVED
	};
/*
	unsigned int rate_list[] = {
		44100,
		48000,
		88200,
		96000,
	};
*/
	unsigned int *rate_list=NULL;
	unsigned int nb_rates;

	unsigned int format = SND_PCM_FORMAT_S24;
	int err;
	int i;

	// FIXME: this fixes the plugin to the first discovered device!
	freebob->dev_options.node_id=freebob_get_device_node_id(freebob->fb_handle, 0);

	freebob_supported_stream_format_info_t* stream_info;
	printf("  port = %d, node_id = %d\n", freebob->dev_options.port, freebob->dev_options.node_id);

	stream_info = freebob_get_supported_stream_format_info( freebob->fb_handle, 
								freebob->dev_options.node_id, 
								freebob->stream == SND_PCM_STREAM_PLAYBACK ? 1 : 0 );
	


	freebob_print_supported_stream_format_info( stream_info );	

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

	freebob_free_supported_stream_format_info( stream_info );

	freebob->sample_bits = snd_pcm_format_physical_width(format);

	if ((err = snd_pcm_ioplug_set_param_list(&freebob->io, SND_PCM_IOPLUG_HW_ACCESS,
						 ARRAY_SIZE(access_list), access_list)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_list(&freebob->io, SND_PCM_IOPLUG_HW_RATE,
						 ARRAY_SIZE(rate_list), rate_list)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_list(&freebob->io, SND_PCM_IOPLUG_HW_FORMAT,
						 1, &format)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_minmax(&freebob->io, SND_PCM_IOPLUG_HW_CHANNELS,
						   min_channels, max_channels)) < 0 ||
	    (err = snd_pcm_ioplug_set_param_minmax(&freebob->io, SND_PCM_IOPLUG_HW_PERIOD_BYTES,
						   64, 64*1024)) < 0 ||
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
	static unsigned int num = 0;

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
		
	if ( freebob_discover_devices( freebob->fb_handle, 1 ) != 0 ) {
	    SNDERR("Could not discover devices\n" );
	    freebob_destroy_handle( freebob->fb_handle );
	    return -EINVAL;
	}

	freebob_device_info_t device_info;

	freebob->streaming_device=freebob_streaming_init(&device_info,freebob->dev_options);

	if(!freebob->streaming_device) {
		SNDERR("FREEBOB: Error creating virtual device\n");
		freebob_destroy_handle( freebob->fb_handle );
		return -EINVAL;
	}

/*
	freebob->playback_channels = 0;
	if (freebob->channels == 0) {
		snd_pcm_freebob_free(freebob);
		return -EINVAL;
	}
*/
/*
	freebob->areas = calloc(freebob->channels, sizeof(snd_pcm_channel_area_t));
	if (! freebob->areas) {
		snd_pcm_freebob_free(jack);
		return -ENOMEM;
	}
*/


	socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
	
	freebob->fd = fd[0];

	freebob->io.version = SND_PCM_IOPLUG_VERSION;
	freebob->io.name = "FreeBob PCM Plugin";
	freebob->io.callback = &freebob_pcm_callback;
	freebob->io.private_data = freebob;
	freebob->io.mmap_rw = 0;
	freebob->io.poll_fd = freebob->fd;
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
// 	snd_config_t *playback_conf = NULL;
// 	snd_config_t *capture_conf = NULL;
	int err;
	long tmp_int;

	freebob_options_t dev_options;
	
	dev_options.node_id=0;
	dev_options.port=0;

	dev_options.sample_rate=44100;
	dev_options.period_size=512;
	dev_options.nb_buffers=3;
	dev_options.iso_buffers=100;
	dev_options.iso_prebuffers=0;
	dev_options.iso_irq_interval=4;

	/* packetizer thread options */

	dev_options.realtime=1;
	dev_options.packetizer_priority=60;

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
/*		if (strcmp(id, "playback_ports") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			playback_conf = n;
			continue;
		}
		if (strcmp(id, "capture_ports") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			capture_conf = n;
			continue;
		}
*/
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}

	err = snd_pcm_freebob_open(pcmp, name, &dev_options, stream, mode);

	return err;

}

SND_PCM_PLUGIN_SYMBOL(freebob);
