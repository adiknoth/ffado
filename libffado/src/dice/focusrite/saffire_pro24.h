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

#ifndef DICE_FOCUSRITE_SAFFIRE_PRO24_H
#define DICE_FOCUSRITE_SAFFIRE_PRO24_H

#include "dice/dice_avdevice.h"

#include "libieee1394/configrom.h"

namespace Dice {
namespace Focusrite {

class SaffirePro24 : public Dice::Device {
public:
    SaffirePro24( DeviceManager& d,
                  std::auto_ptr<ConfigRom>( configRom ));
    ~SaffirePro24();

    bool discover();

    void showDevice();

    bool canChangeNickname() { return true; }
    bool setNickName( std::string name );
    std::string getNickName();

    /**
     * @brief A standard-switch for boolean.
     *
     * If you don't like True and False for the labels, subclass and return your own.
    */
    class Switch : public Control::Boolean
    {
    protected:
        friend class Dice::Focusrite::SaffirePro24;

        Switch(Dice::Device::EAP*, std::string name, size_t offset, int activevalue);
    public:
        bool selected();
        bool select(bool);
    private:
        Dice::Device::EAP* m_eap;
        bool m_selected;
        size_t m_offset;
        int m_activevalue;
        fb_quadlet_t m_state_tmp;
    };

    class MonitorSection : public Control::Container
    {
    protected:
        friend class Dice::Focusrite::SaffirePro24;

        MonitorSection(Dice::Device::EAP*, std::string name);
    private:
        Dice::Device::EAP* m_eap;
        Switch *m_mute, *m_dim;
        std::vector<Switch*> m_mute_affected;
        std::vector<Switch*> m_dim_affected;
        std::vector<Switch*> m_mono;
    };
private:
    class LineInstSwitch;
    LineInstSwitch *m_ch1, *m_ch2;
    class LevelSwitch;
    LevelSwitch *m_ch3, *m_ch4;
    MonitorSection *m_monitor;
};

}
}

#endif
// vim: et
