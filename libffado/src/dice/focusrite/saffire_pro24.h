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

#include "focusrite_eap.h"

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
    bool setNickname( std::string name );
    std::string getNickname();

private:

    class SaffirePro24EAP : public FocusriteEAP
    {
    public:
        SaffirePro24EAP(Dice::Device& dev) : FocusriteEAP(dev) {
        }

        int commandToFix(unsigned offset);

        Poti* getMonitorPoti(std::string);
        Poti* getDimPoti(std::string);

        void setupSources_low();
        void setupDestinations_low();
        void setupSources_mid();
        void setupDestinations_mid();
        void setupSources_high();
        void setupDestinations_high();
    };
    Dice::EAP* createEAP();

    class LineInstSwitch;
    LineInstSwitch *m_ch1, *m_ch2;
    class LevelSwitch;
    LevelSwitch *m_ch3, *m_ch4;
    Dice::Focusrite::FocusriteEAP::MonitorSection *m_monitor;
};

}
}

#endif
// vim: et
