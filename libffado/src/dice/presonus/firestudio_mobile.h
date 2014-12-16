/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2012 by Philippe Carriere
 * Copyright (C) 2014 by Philippe Carriere
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

#ifndef DICE_PRESONUS_FIRESTUDIO_MOBILE_H
#define DICE_PRESONUS_FIRESTUDIO_MOBILE_H

#include "dice/dice_avdevice.h"
#include "dice/dice_eap.h"

#include "libieee1394/configrom.h"

namespace Dice {
namespace Presonus {

class FirestudioMobile : public Dice::Device {
public:
    FirestudioMobile( DeviceManager& d,
                  std::auto_ptr<ConfigRom>( configRom ));
    virtual ~FirestudioMobile();

    bool discover();

    virtual void showDevice();

    bool canChangeNickname() { return false; }

private:
    class FirestudioMobileEAP : public Dice::EAP
    {
    public:
        FirestudioMobileEAP(Dice::Device& dev) : Dice::EAP(dev) {
        }

        void setupSources_low();
        void setupDestinations_low();
        void setupSources_mid();
        void setupDestinations_mid();
        void setupSources_high();
        void setupDestinations_high();
        void setupDefaultRouterConfig_low();
        void setupDefaultRouterConfig_mid();
        void setupDefaultRouterConfig_high();

    };
    Dice::EAP* createEAP();
};

}
}

#endif
// vim: et
