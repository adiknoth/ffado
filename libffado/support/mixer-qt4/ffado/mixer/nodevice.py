#!/usr/bin/python
#
# Copyright (C) 2005-2008 by Pieter Palmers
#               2007-2008 by Arnold Krille
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from PyQt4.QtGui import QWidget
from mixer_nodeviceui import Ui_NoDeviceMixerUI

class NoDeviceMixer(QWidget, Ui_NoDeviceMixerUI):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        self.setupUi(self)

# vim: et
