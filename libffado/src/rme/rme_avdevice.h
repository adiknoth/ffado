/*
 * Copyright (C) 2005-2008 by Jonathan Woithe
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef RMEDEVICE_H
#define RMEDEVICE_H

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libutil/Configuration.h"

#include "fireface_def.h"
#include "libstreaming/rme/RmeReceiveStreamProcessor.h"
#include "libstreaming/rme/RmeTransmitStreamProcessor.h"

#include "rme_shm.h"

class ConfigRom;
class Ieee1394Service;

namespace Rme {

// Note: the values in this enum do not have to correspond to the unit
// version reported by the respective devices.  It just so happens that they
// currently do for the Fireface-800 and Fireface-400.
enum ERmeModel {
    RME_MODEL_NONE          = 0x0000,
    RME_MODEL_FIREFACE800   = 0x0001,
    RME_MODEL_FIREFACE400   = 0x0002,
};

class Device : public FFADODevice {
public:

    Device( DeviceManager& d,
               std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Device();

    virtual bool buildMixer();
    virtual bool destroyMixer();

    static bool probe( Util::Configuration& c, ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ));
    static int getConfigurationId( );
    virtual bool discover();

    virtual void showDevice();

    bool setDDSFrequency( int dds_freq );
    virtual bool setSamplingFrequency( int samplingFrequency );
    virtual int getSamplingFrequency( );
    virtual std::vector<int> getSupportedSamplingFrequencies();

    ClockSource dummyClockSource(void);
    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual int getStreamCount();
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool resetForStreaming();
    virtual bool prepare();
    virtual bool lock();
    virtual bool unlock();

    virtual enum FFADODevice::eStreamingState getStreamingState();
    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    signed int getNumChannels(void) { return num_channels; };
    signed int getFramesPerPacket(void);

    bool addPort(Streaming::StreamProcessor *s_processor,
             char *name, enum Streaming::Port::E_Direction direction,
             int position, int size);
    bool addDirPorts(enum Streaming::Port::E_Direction direction);

    unsigned int readRegister(fb_nodeaddr_t reg);
    signed int readBlock(fb_nodeaddr_t reg, quadlet_t *buf, unsigned int n_quads);
    signed int writeRegister(fb_nodeaddr_t reg, quadlet_t data);
    signed int writeBlock(fb_nodeaddr_t reg, quadlet_t *data, unsigned int n_quads);

    /* Device control functions */
    signed int getPhantom(unsigned int channel);
    signed int setPhantom(unsigned int channel, unsigned int status);
    signed int getInputLevel(void);
    signed int setInputLevel(unsigned int level);
    signed int getOutputLevel(void);
    signed int setOutputLevel(unsigned int level);
    signed int getPhonesLevel(void);
    signed int setPhonesLevel(unsigned int level);
    signed int getInputPadOpt(unsigned int channel);
    signed int setInputPadOpt(unsigned int channel, unsigned int status);
    signed int getInputInstrOpt(unsigned int channel);
    signed int setInputInstrOpt(unsigned int channel, unsigned int status);
    signed int getInputSource(unsigned int channel);
    signed int setInputSource(unsigned int channel, unsigned int src);
    signed int getSpdifInputMode(void);
    signed int setSpdifInputMode(signed int mode);
    signed int getSpdifOutputIsOptical(void);
    signed int setSpdifOutputIsOptical(signed int enable);
    signed int getSpdifOutputEmphasisOn(void);
    signed int setSpdifOutputEmphasisOn(signed int enable);
    signed int getSpdifOutputNonAudioOn(void);
    signed int setSpdifOutputNonAudioOn(signed int enable);
    signed int getSpdifOutputProOn(void);
    signed int setSpdifOutputProOn(signed int enable);
    signed int getAmpGain(unsigned int index);
    signed int setAmpGain(unsigned int index, signed int val);
    signed int getMixerGain(unsigned int ctype,
        unsigned int src_channel, unsigned int dest_channel);
    signed int setMixerGain(unsigned int ctype, 
        unsigned int src_channel, unsigned int dest_channel, signed int val);
    signed int getMixerFlags(unsigned int ctype,
        unsigned int src_channel, unsigned int dest_channel, unsigned int flagmask);
    signed int setMixerFlags(unsigned int ctype,
        unsigned int src_channel, unsigned int dest_channel, unsigned int flagmask, signed int val);
    signed int getClockMode(void);
    signed int setClockMode(signed int mode);
    signed int getSyncRef(void);
    signed int setSyncRef(signed int ref);
    signed int getBandwidthLimit(void);
    signed int setBandwidthLimit(signed int limit);

    /* General information functions */
    signed int getRmeModel(void) { return m_rme_model; }
    signed int getTcoPresent(void) { return dev_config->tco_present; }
    Streaming::RmeReceiveStreamProcessor *getRxSP(void) { return m_receiveProcessor; }

protected:
    enum ERmeModel m_rme_model;

    FF_software_settings_t *settings;
    FF_TCO_settings_t *tco_settings;

    rme_shm_t *dev_config, local_dev_config_obj;

    signed int num_channels;
    signed int frames_per_packet; // 1 frame includes 1 sample from each channel
    signed int speed800;

    signed int iso_tx_channel, iso_rx_channel;

    Streaming::RmeReceiveStreamProcessor *m_receiveProcessor;
    Streaming::RmeTransmitStreamProcessor *m_transmitProcessor;

private:
    unsigned long long int cmd_buffer_addr();
    unsigned long long int stream_init_reg();
    unsigned long long int stream_start_reg();
    unsigned long long int stream_end_reg();
    unsigned long long int flash_settings_addr();
    unsigned long long int flash_mixer_vol_addr();
    unsigned long long int flash_mixer_pan_addr();
    unsigned long long int flash_mixer_hw_addr();

    /* Low-level flash memory functions */
    signed int wait_while_busy(unsigned int init_delay);
    signed int get_revision(unsigned int *revision);
    signed int read_flash(fb_nodeaddr_t addr, quadlet_t *buf, unsigned int n_quads);
    signed int erase_flash(unsigned int flags);
    signed int write_flash(fb_nodeaddr_t addr, quadlet_t *buf, unsigned int n_quads);

    /* Upper level flash memory functions */
public:
    signed int read_device_flash_settings(FF_software_settings_t *dsettings);
    signed int write_device_flash_settings(FF_software_settings_t *dsettings);
    signed int read_device_mixer_settings(FF_software_settings_t *dsettings);
    signed int write_device_mixer_settings(FF_software_settings_t *dsettings);

private:
    /* Low-level hardware functions */
    unsigned int multiplier_of_freq(unsigned int freq);
    void config_lock(void);
    void config_unlock(void);
    signed int init_hardware(void);
    signed int get_hardware_status(unsigned int *stat0, unsigned int *stat1);
    signed int get_hardware_streaming_status(unsigned int *stat, unsigned int n);
public:
    signed int get_hardware_state(FF_state_t *state);
private:
    signed int set_hardware_params(FF_software_settings_t *use_settings = NULL);

    signed int read_tco(quadlet_t *tco_data, signed int size);
    signed int write_tco(quadlet_t *tco_data, signed int size);
    signed int hardware_is_streaming(void);

    signed int read_tco_state(FF_TCO_state_t *tco_state);
    signed int write_tco_settings(FF_TCO_settings_t *tco_settings);

    signed int set_hardware_dds_freq(signed int freq);

    signed int hardware_init_streaming(unsigned int sample_rate, unsigned int tx_channel);
    signed int hardware_start_streaming(unsigned int listen_channel);
    signed int hardware_stop_streaming(void);

    signed int set_hardware_ampgain(unsigned int index, signed int val);
    signed int set_hardware_mixergain(unsigned int ctype, 
        unsigned int src_channel, unsigned int dest_channel, signed int val);

    signed int set_hardware_channel_mute(signed int chan, signed int mute);
    signed int set_hardware_output_rec(signed int rec);

    signed int getMixerGainIndex(unsigned int src_channel, unsigned int dest_channel);

    Control::Container *m_MixerContainer;
    Control::Container *m_ControlContainer;
};

}

#endif
