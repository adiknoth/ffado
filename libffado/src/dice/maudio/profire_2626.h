/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2012 by Philippe Carriere
 * Copyright (C) 2012 by Jano Svitok
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

#ifndef DICE_MAUDIO_PROFIRE_2626_H
#define DICE_MAUDIO_PROFIRE_2626_H

#include "dice/dice_avdevice.h"
#include "dice/dice_eap.h"

#include "libieee1394/configrom.h"

// Global monitor registers (application space)
#define MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_OFFSET 0x00
#define MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_SIZE 4
#define MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_VALUE 1
#define MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_SHIFT 0

namespace Dice {
namespace Maudio {

class Profire2626 : public Dice::Device {
public:
    Profire2626( DeviceManager& d,
                  std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Profire2626();

    bool discover();

    virtual void showDevice();

    bool canChangeNickname() { return false; }

    class Profire2626EAP : public Dice::EAP
    {
    public:
        Profire2626EAP(Dice::Device& dev);

        void setupSources_low();
        void setupDestinations_low();
        void setupSources_mid();
        void setupDestinations_mid();
        void setupSources_high();
        void setupDestinations_high();
        void setupDefaultRouterConfig_low();
        void setupDefaultRouterConfig_mid();
        void setupDefaultRouterConfig_high();

        bool readApplicationReg(unsigned, quadlet_t*);
        bool writeApplicationReg(unsigned, quadlet_t);

       /**
        * @brief A standard-switch for boolean.
        *
        * If you don't like True and False for the labels, subclass and return your own.
        * \internal copy&paste from focusrite_eap.h
        */
        class Switch : public Control::Boolean
        {
        public:
            Switch(Profire2626EAP*, std::string, size_t, int);
            bool selected();
            bool select(bool);
        private:
            Profire2626EAP* m_eap;
            std::string m_name;
            size_t m_offset;
            int m_activevalue;
        };

        class SettingsSection : public Control::Container
        {
        public:
            SettingsSection(Profire2626EAP*, std::string);
        private:
            Profire2626EAP * m_eap;
        };
    };

private:
    Dice::EAP* createEAP();
};

}
}

#endif
// vim: et
