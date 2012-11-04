/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2009 by Arnold Krille
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

#ifndef DICE_FOCUSRITE_FOCUSRITE_EAP_H
#define DICE_FOCUSRITE_FOCUSRITE_EAP_H

#include "dice/dice_eap.h"

#include "libieee1394/configrom.h"

/**
 *  Focusrite EAP application space
 *    Prescribed values common to all devices
 */
// The limit of the monitoring setting register space in the EAP application space
// (included)
// All subsequent addresses can not be beyond this limit
#define FOCUSRITE_EAP_REGISTER_APP_MONITORING_LIMIT 0x68

// To be sent after any message set 
#define FOCUSRITE_EAP_MESSAGE_SET_NO_MESSAGE 0

// Global switches
// One register for each
#define FOCUSRITE_EAP_GLOBAL_MUTE_SWITCH_VALUE 1
#define FOCUSRITE_EAP_GLOBAL_DIM_SWITCH_VALUE 1

// Per Line/Out switch monitor registers; bit encoding
//   Each register controls two (left and rigth) Line/Out
#define FOCUSRITE_EAP_SWITCH_BIT_1   1 // Activate left Line/Out
#define FOCUSRITE_EAP_SWITCH_BIT_2   2 // Activate right Line/Out
#define FOCUSRITE_EAP_SWITCH_BIT_1_2 3 // Activate both
#define FOCUSRITE_EAP_SWITCH_BIT_3   4 // Mute left Line/Out
#define FOCUSRITE_EAP_SWITCH_BIT_4   8 // Mute right Line/Out

// Per Line/Out mute, dim and mono register; bit encoding
//   One register controls all Line/Out
#define FOCUSRITE_EAP_SWITCH_CONTROL_MUTE_SHIFT 0
#define FOCUSRITE_EAP_SWITCH_CONTROL_DIM_SHIFT 10
#define FOCUSRITE_EAP_SWITCH_CONTROL_MONO_SHIFT 20
#define FOCUSRITE_EAP_GLOBAL_MUTE_SWITCH_VALUE 1
#define FOCUSRITE_EAP_GLOBAL_DIM_SWITCH_VALUE 1
#define FOCUSRITE_EAP_SWITCH_CONTROL_VALUE 1

// Per Line/Out volume monitor registers
// Each register controls two (left and rigth) Line/Out
// The two last bytes (little endian) of each Line/Out volume register
//   control the right and line line respectively 
#define FOCUSRITE_EAP_LINEOUT_VOLUME_SET_1   0
#define FOCUSRITE_EAP_LINEOUT_VOLUME_SET_2   8

// Per Line/Out Instrument/Line and Hi/Lo gain switches
//   Each register controls two (left and rigth) Line/Out
#define FOCUSRITE_EAP_LINEOUT_SWITCH_INST_SHIFT 16
#define FOCUSRITE_EAP_LINEOUT_SWITCH_GAIN_SHIFT 16

namespace Dice {
namespace Focusrite {

class FocusriteEAP : public Dice::EAP
{
public:
    /**
     * @brief A standard-switch for boolean.
     *
     * If you don't like True and False for the labels, subclass and return your own.
    */
    class Switch : public Control::Boolean
    {
    public:
        Switch(Dice::Focusrite::FocusriteEAP*, std::string name, size_t offset, int activevalue);

        bool selected();
        bool select(bool);

    private:
        Dice::Focusrite::FocusriteEAP* m_eap;
        bool m_selected;
        size_t m_offset;
        int m_activevalue;
        fb_quadlet_t m_state_tmp;
    };

    class Poti : public Control::Discrete
    {
        public:
            Poti(Dice::Focusrite::FocusriteEAP*, std::string name, size_t offset);

            int getValue(int) { return getValue(); }
            bool setValue(int, int n) { return setValue(n); }

            int getMinimum() { return -127; }
            int getMaximum() { return 0; }

            int getValue();
            bool setValue(int n);
        private:
            Dice::Focusrite::FocusriteEAP* m_eap;
            size_t m_offset;
            int m_value;
            quadlet_t m_tmp;
    };

    class MonitorSection : public Control::Container
    {
    public:
        MonitorSection(Dice::Focusrite::FocusriteEAP*, std::string name);

    private:
        Dice::Focusrite::FocusriteEAP* m_eap;
        Switch *m_mute, *m_dim;
        std::vector<Switch*> m_mute_affected;
        std::vector<Switch*> m_dim_affected;
        std::vector<Switch*> m_mono;
        Poti* m_monitorlevel;
        Poti* m_dimlevel;
    };


public:
    FocusriteEAP(Dice::Device&);

    bool readApplicationReg(unsigned offset, quadlet_t*);
    bool writeApplicationReg(unsigned offset, quadlet_t);

protected:
    virtual int commandToFix(unsigned offset) =0;
    virtual Poti* getMonitorPoti(std::string) { return 0; }
    virtual Poti* getDimPoti(std::string) { return 0; }
};

}
}

#endif
// vim: et
