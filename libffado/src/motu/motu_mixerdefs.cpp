/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2009 by Jonathan Woithe
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

/* This module contains definitions of mixers on devices which utilise the
 * original "pre-Mark3" mixer control protocol.
 */

#include "motu/motu_avdevice.h"
#include "motu/motu_mixerdefs.h"

namespace Motu {

// Mixer registers
const MatrixMixBus MixerBuses_Traveler[] = {
    {"Mix 1", 0x4000, },
    {"Mix 2", 0x4100, },
    {"Mix 3", 0x4200, },
    {"Mix 4", 0x4300, },
};

const MatrixMixChannel MixerChannels_Traveler[] = {
    {"Analog 1", MOTU_CTRL_STD_CHANNEL, 0x0000, },
    {"Analog 2", MOTU_CTRL_STD_CHANNEL, 0x0004, },
    {"Analog 3", MOTU_CTRL_STD_CHANNEL, 0x0008, },
    {"Analog 4", MOTU_CTRL_STD_CHANNEL, 0x000c, },
    {"Analog 5", MOTU_CTRL_STD_CHANNEL, 0x0010, },
    {"Analog 6", MOTU_CTRL_STD_CHANNEL, 0x0014, },
    {"Analog 7", MOTU_CTRL_STD_CHANNEL, 0x0018, },
    {"Analog 8", MOTU_CTRL_STD_CHANNEL, 0x001c, },
    {"AES/EBU 1", MOTU_CTRL_STD_CHANNEL, 0x0020, },
    {"AES/EBU 2", MOTU_CTRL_STD_CHANNEL, 0x0024, },
    {"SPDIF 1", MOTU_CTRL_STD_CHANNEL, 0x0028, },
    {"SPDIF 2", MOTU_CTRL_STD_CHANNEL, 0x002c, },
    {"ADAT 1", MOTU_CTRL_STD_CHANNEL, 0x0030, },
    {"ADAT 2", MOTU_CTRL_STD_CHANNEL, 0x0034, },
    {"ADAT 3", MOTU_CTRL_STD_CHANNEL, 0x0038, },
    {"ADAT 4", MOTU_CTRL_STD_CHANNEL, 0x003c, },
    {"ADAT 5", MOTU_CTRL_STD_CHANNEL, 0x0040, },
    {"ADAT 6", MOTU_CTRL_STD_CHANNEL, 0x0044, },
    {"ADAT 7", MOTU_CTRL_STD_CHANNEL, 0x0048, },
    {"ADAT 8", MOTU_CTRL_STD_CHANNEL, 0x004c, },
};

const MixerCtrl MixerCtrls_Traveler[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },
    {"Mainout_",  "MainOut ", "", MOTU_CTRL_MIX_FADER, 0x0c0c, },
    {"Phones_",   "Phones ",  "", MOTU_CTRL_MIX_FADER, 0x0c10, },

    /* For mic/line input controls, the "register" is the zero-based channel number */
    {"Control/Ana1_", "Analog 1 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 0},
    {"Control/Ana2_", "Analog 2 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 1},
    {"Control/Ana3_", "Analog 3 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 2},
    {"Control/Ana4_", "Analog 4 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 3},
    {"Control/Ana5_", "Analog 5 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 4},
    {"Control/Ana6_", "Analog 6 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 5},
    {"Control/Ana7_", "Analog 7 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 6},
    {"Control/Ana8_", "Analog 8 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 7},

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MatrixMixBus MixerBuses_Ultralite[] = {
    {"Mix 1", 0x4000, },
    {"Mix 2", 0x4100, },
    {"Mix 3", 0x4200, },
    {"Mix 4", 0x4300, },
};

const MatrixMixChannel MixerChannels_Ultralite[] = {
    {"Analog 1", MOTU_CTRL_STD_CHANNEL, 0x0000, },
    {"Analog 2", MOTU_CTRL_STD_CHANNEL, 0x0004, },
    {"Analog 3", MOTU_CTRL_STD_CHANNEL, 0x0008, },
    {"Analog 4", MOTU_CTRL_STD_CHANNEL, 0x000c, },
    {"Analog 5", MOTU_CTRL_STD_CHANNEL, 0x0010, },
    {"Analog 6", MOTU_CTRL_STD_CHANNEL, 0x0014, },
    {"Analog 7", MOTU_CTRL_STD_CHANNEL, 0x0018, },
    {"Analog 8", MOTU_CTRL_STD_CHANNEL, 0x001c, },
    {"AES/EBU 1", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"AES/EBU 2", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"SPDIF 1", MOTU_CTRL_STD_CHANNEL, 0x0020, },
    {"SPDIF 2", MOTU_CTRL_STD_CHANNEL, 0x0024, },
    {"ADAT 1", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 2", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 3", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 4", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 5", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 6", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 7", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 8", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
};

const MixerCtrl MixerCtrls_Ultralite[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },
    {"Mainout_",  "MainOut ", "", MOTU_CTRL_MIX_FADER, 0x0c0c, },
    {"Phones_",   "Phones ",  "", MOTU_CTRL_MIX_FADER, 0x0c10, },

    /* For mic/line input controls, the "register" is the zero-based channel number */
    {"Control/Ana1_", "Analog 1 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 0},
    {"Control/Ana2_", "Analog 2 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 1},
    {"Control/Ana3_", "Analog 3 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 2},
    {"Control/Ana4_", "Analog 4 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 3},
    {"Control/Ana5_", "Analog 5 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 4},
    {"Control/Ana6_", "Analog 6 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 5},
    {"Control/Ana7_", "Analog 7 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 6},
    {"Control/Ana8_", "Analog 8 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 7},
    {"Control/Spdif1_", "SPDIF 1 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 6},
    {"Control/Spdif2_", "SPDIF 2 input ", "", MOTU_CTRL_ULTRALITE_INPUT_CTRLS, 7},

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MatrixMixBus MixerBuses_896HD[] = {
    {"Mix 1", 0x4000, },
    {"Mix 2", 0x4100, },
    {"Mix 3", 0x4200, },
    {"Mix 4", 0x4300, },
};

const MatrixMixChannel MixerChannels_896HD[] = {
    {"Analog 1", MOTU_CTRL_STD_CHANNEL, 0x0000, },
    {"Analog 2", MOTU_CTRL_STD_CHANNEL, 0x0004, },
    {"Analog 3", MOTU_CTRL_STD_CHANNEL, 0x0008, },
    {"Analog 4", MOTU_CTRL_STD_CHANNEL, 0x000c, },
    {"Analog 5", MOTU_CTRL_STD_CHANNEL, 0x0010, },
    {"Analog 6", MOTU_CTRL_STD_CHANNEL, 0x0014, },
    {"Analog 7", MOTU_CTRL_STD_CHANNEL, 0x0018, },
    {"Analog 8", MOTU_CTRL_STD_CHANNEL, 0x001c, },
    {"AES/EBU 1", MOTU_CTRL_STD_CHANNEL, 0x0020, },
    {"AES/EBU 2", MOTU_CTRL_STD_CHANNEL, 0x0024, },
    {"SPDIF 1", MOTU_CTRL_STD_CHANNEL, 0x0048, },
    {"SPDIF 2", MOTU_CTRL_STD_CHANNEL, 0x004c, },
    {"ADAT 1", MOTU_CTRL_STD_CHANNEL, 0x0028, },
    {"ADAT 2", MOTU_CTRL_STD_CHANNEL, 0x002c, },
    {"ADAT 3", MOTU_CTRL_STD_CHANNEL, 0x0030, },
    {"ADAT 4", MOTU_CTRL_STD_CHANNEL, 0x0034, },
    {"ADAT 5", MOTU_CTRL_STD_CHANNEL, 0x0038, },
    {"ADAT 6", MOTU_CTRL_STD_CHANNEL, 0x003c, },
    {"ADAT 7", MOTU_CTRL_STD_CHANNEL, 0x0040, },
    {"ADAT 8", MOTU_CTRL_STD_CHANNEL, 0x0044, },
};

const MixerCtrl MixerCtrls_896HD[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },
    {"Mainout_",  "MainOut ", "", MOTU_CTRL_MIX_FADER, 0x0c0c, },
    {"Phones_",   "Phones ",  "", MOTU_CTRL_MIX_FADER, 0x0c10, },

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},

    /* For meter controls the "register" indicates which meter controls are available */
    {"Control/Meter_", "Meter ", "", MOTU_CTRL_METER,
      MOTU_CTRL_METER_PEAKHOLD | MOTU_CTRL_METER_CLIPHOLD | MOTU_CTRL_METER_AESEBU_SRC | 
      MOTU_CTRL_METER_PROG_SRC},
};

const MatrixMixBus MixerBuses_828Mk2[] = {
    {"Mix 1", 0x4000, },
    {"Mix 2", 0x4100, },
    {"Mix 3", 0x4200, },
    {"Mix 4", 0x4300, },
};

const MatrixMixChannel MixerChannels_828Mk2[] = {
    {"Analog 1", MOTU_CTRL_STD_CHANNEL, 0x0000, },
    {"Analog 2", MOTU_CTRL_STD_CHANNEL, 0x0004, },
    {"Analog 3", MOTU_CTRL_STD_CHANNEL, 0x0008, },
    {"Analog 4", MOTU_CTRL_STD_CHANNEL, 0x000c, },
    {"Analog 5", MOTU_CTRL_STD_CHANNEL, 0x0010, },
    {"Analog 6", MOTU_CTRL_STD_CHANNEL, 0x0014, },
    {"Analog 7", MOTU_CTRL_STD_CHANNEL, 0x0018, },
    {"Analog 8", MOTU_CTRL_STD_CHANNEL, 0x001c, },
    {"Mic 1", MOTU_CTRL_STD_CHANNEL, 0x0020, },
    {"Mic 2", MOTU_CTRL_STD_CHANNEL, 0x0024, },
    {"SPDIF 1", MOTU_CTRL_STD_CHANNEL, 0x0028, },
    {"SPDIF 2", MOTU_CTRL_STD_CHANNEL, 0x002c, },
    {"ADAT 1", MOTU_CTRL_STD_CHANNEL, 0x0030, },
    {"ADAT 2", MOTU_CTRL_STD_CHANNEL, 0x0034, },
    {"ADAT 3", MOTU_CTRL_STD_CHANNEL, 0x0038, },
    {"ADAT 4", MOTU_CTRL_STD_CHANNEL, 0x003c, },
    {"ADAT 5", MOTU_CTRL_STD_CHANNEL, 0x0040, },
    {"ADAT 6", MOTU_CTRL_STD_CHANNEL, 0x0044, },
    {"ADAT 7", MOTU_CTRL_STD_CHANNEL, 0x0048, },
    {"ADAT 8", MOTU_CTRL_STD_CHANNEL, 0x004c, },
};

const MixerCtrl MixerCtrls_828Mk2[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },
    {"Mainout_",  "MainOut ", "", MOTU_CTRL_MIX_FADER, 0x0c0c, },
    {"Phones_",   "Phones ",  "", MOTU_CTRL_MIX_FADER, 0x0c10, },

    /* For mic/line input controls, the "register" is the zero-based channel number */
    {"Control/Ana1_", "Analog 1 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 0},
    {"Control/Ana2_", "Analog 2 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 1},
    {"Control/Ana3_", "Analog 3 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 2},
    {"Control/Ana4_", "Analog 4 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 3},
    {"Control/Ana5_", "Analog 5 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 4},
    {"Control/Ana6_", "Analog 6 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 5},
    {"Control/Ana7_", "Analog 7 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 6},
    {"Control/Ana8_", "Analog 8 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 7},

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MatrixMixBus MixerBuses_8pre[] = {
    {"Mix 1", 0x4000, },
    {"Mix 2", 0x4100, },
    {"Mix 3", 0x4200, },
    {"Mix 4", 0x4300, },
};

const MatrixMixChannel MixerChannels_8pre[] = {
    {"Analog 1", MOTU_CTRL_STD_CHANNEL, 0x0000, },
    {"Analog 2", MOTU_CTRL_STD_CHANNEL, 0x0004, },
    {"Analog 3", MOTU_CTRL_STD_CHANNEL, 0x0008, },
    {"Analog 4", MOTU_CTRL_STD_CHANNEL, 0x000c, },
    {"Analog 5", MOTU_CTRL_STD_CHANNEL, 0x0010, },
    {"Analog 6", MOTU_CTRL_STD_CHANNEL, 0x0014, },
    {"Analog 7", MOTU_CTRL_STD_CHANNEL, 0x0018, },
    {"Analog 8", MOTU_CTRL_STD_CHANNEL, 0x001c, },
    //
    // The Ultralite doesn't include AES/EBU or SPDIF mixer controls, but
    // "pad" mixer entries are required so the index of the ADAT controls
    // within the various matrix mixers remain unchanged compared to other
    // interfaces.  This in turn means the python ffado-mixer code doesn't
    // have to deal with differing layouts within the matrix mixer controls.
    {"AES/EBU 1", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"AES/EBU 2", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"SPDIF 1", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"SPDIF 2", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    //
    {"ADAT 1", MOTU_CTRL_STD_CHANNEL, 0x0020, },
    {"ADAT 2", MOTU_CTRL_STD_CHANNEL, 0x0024, },
    {"ADAT 3", MOTU_CTRL_STD_CHANNEL, 0x0028, },
    {"ADAT 4", MOTU_CTRL_STD_CHANNEL, 0x002c, },
    {"ADAT 5", MOTU_CTRL_STD_CHANNEL, 0x0030, },
    {"ADAT 6", MOTU_CTRL_STD_CHANNEL, 0x0034, },
    {"ADAT 7", MOTU_CTRL_STD_CHANNEL, 0x0038, },
    {"ADAT 8", MOTU_CTRL_STD_CHANNEL, 0x003c, },
};

const MixerCtrl MixerCtrls_8pre[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MixerCtrl MixerCtrls_828Mk1[] = {
    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MotuMixer Mixer_Traveler = MOTUMIXER(
    MixerCtrls_Traveler, MixerBuses_Traveler, MixerChannels_Traveler);

const MotuMixer Mixer_Ultralite = MOTUMIXER(
    MixerCtrls_Ultralite, MixerBuses_Ultralite, MixerChannels_Ultralite);

const MotuMixer Mixer_828Mk2 = MOTUMIXER(
    MixerCtrls_828Mk2, MixerBuses_828Mk2, MixerChannels_828Mk2);

const MotuMixer Mixer_896HD = MOTUMIXER(
    MixerCtrls_896HD, MixerBuses_896HD, MixerChannels_896HD);

const MotuMixer Mixer_8pre = MOTUMIXER(
    MixerCtrls_8pre, MixerBuses_8pre, MixerChannels_8pre);

// Since we don't have channel or bus lists yet, the mixer definition for
// the 828Mk1 must be done without the use of the MOTUMIXER() macro.
const MotuMixer Mixer_828Mk1 = { 
  MixerCtrls_828Mk1, N_ELEMENTS(MixerCtrls_828Mk1),
  NULL, 0, NULL, 0};

}
