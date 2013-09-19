#
# Copyright (C) 2005-2008 by Pieter Palmers
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from PyQt4.QtCore import SIGNAL, SLOT, QObject
from PyQt4.QtGui import QWidget, QMessageBox
from math import log10
from ffado.config import *

import logging
log = logging.getLogger('YamahaGo')

class YamahaGo(QWidget):
	def __init__(self,parent = None):
		QWidget.__init__(self,parent)

	# startup
	def initValues(self):
		self.modelId = self.configrom.getModelId()
		# GO44
		if self.modelId == 0x0010000B:
			self.is46 = False
		# GO46
		elif self.modelId == 0x0010000C:
			self.is46 = True
		else:
			return

		if (not self.is46):
			uicLoad("ffado/mixer/yamahago44", self)
			self.setWindowTitle("Yamaha GO44 Control")
		else:
			uicLoad("ffado/mixer/yamahago46", self)
			self.setWindowTitle("Yamaha GO46 Control")

		# common
		self.VolumeControls = {
			'strmplbk12':	['/Mixer/Feature_Volume_3', self.sldStrmPlbk12],
			'strmplbk34':	['/Mixer/Feature_Volume_4', self.sldStrmPlbk34],
			'strmplbk56':	['/Mixer/Feature_Volume_5', self.sldStrmPlbk56],
			'anain12':	['/Mixer/Feature_Volume_6', self.sldAnaIn12],
			'digiin12':	['/Mixer/Feature_Volume_7', self.sldDigiIn12],
		}
		self.JackSourceSelectors = {
			'anaout12':	['/Mixer/Selector_1', self.cmbAnaOut12],
			'anaout34':	['/Mixer/Selector_2', self.cmbAnaOut34],
			'digiout12':	['/Mixer/Selector_3', self.cmbDigiOut12],
			'clksrc':	['/Mixer/Selector_4', self.cmbClkSrc]
			}

		if (self.is46):
			# volume for mixer output
			self.VolumeControls['master'] = ['/Mixer/Feature_Volume_2', self.sldMaster]
			# volume for analog output
			self.OutputVolumeControls = {
				'anaout12':	['/Mixer/Feature_Volume_1', 1, self.sldAnaOut12],
				'anaout34':	['/Mixer/Feature_Volume_1', 3, self.sldAnaOut34]
			}
		else:
			# volume for mixer output
			self.VolumeControls['master'] = ['/Mixer/Feature_Volume_1', self.sldMaster]


		# mixer master volume
		self.sldMaster.setEnabled(True)

		# gain control for mixer input
		for name, ctrl in self.VolumeControls.iteritems():
			decibel = self.hw.getContignuous(ctrl[0])
			vol = pow(10, decibel / 16384 + 2)
			log.debug("%s volume is %d" % (name , vol))
			ctrl[1].setValue(vol)

		# source selector for jack output
		for name, ctrl in self.JackSourceSelectors.iteritems():
			state = self.hw.getDiscrete(ctrl[0])
			log.debug("%s state is %d" % (name , state))
			ctrl[1].setCurrentIndex(state)

		if (self.is46):
			# volume for output
			for name, ctrl in self.OutputVolumeControls.iteritems():
				decibel = self.hw.getContignuous(ctrl[0], ctrl[1])
				vol = pow(10, decibel / 16384 + 2)
				log.debug("%s volume for %d is %d" % (name, ctrl[1], vol))
				ctrl[2].setValue(vol)

	def getDisplayTitle(self):
		if self.configrom.getModelId() == 0x0010000B:
			return "GO44"
		else:
			return "GO46"

	# helper functions
	def setVolume(self, name, vol):
		decibel = (log10(vol + 1) - 2) * 16384
		log.debug("setting %s volume to %d" % (name, decibel))
		self.hw.setContignuous(self.VolumeControls[name][0], decibel)
	def setOutVolume(self, id, vol):
		decibel = (log10(vol + 1) - 2) * 16384
		self.hw.setContignuous('/Mixer/Feature_Volume_1', decibel, id)
	def setSelector(self, a0, a1):
		name = a0
		state = a1
		log.debug("setting %s state to %d" % (name, state))
		self.hw.setDiscrete(self.JackSourceSelectors[name][0], state)
	def setAnaInLevel(self, a0):
		if a0 == 0:
			level = 0
			string = 'high'
		elif a0 == 1:
			level = -1535
			string = 'middle'
		else:
			level = -3583
			string = 'low'
		log.debug("setting front level to %s, %d" % (string, level))
		self.hw.setContignuous('/Mixer/Feature_Volume_2', level)

	# source control for mixer
	def setGainStrmPlbk12(self, a0):
		self.setVolume('strmplbk12', a0)
	def setGainStrmPlbk34(self, a0):
		self.setVolume('strmplbk34', a0)
	def setGainAnaIn12(self, a0):
		self.setVolume('anain12', a0)
	def setGainStrmPlbk56(self, a0):
		self.setVolume('strmplbk56', a0)
	def setGainDigiIn(self,a0):
		self.setVolume('digiin12', a0)
	def setVolumeMixer(self, a0):
		self.setVolume('master', a0)
	def setVolumeAnaOut12(self, a0):
		self.setOutVolume(1, a0)
		self.setOutVolume(2, a0)
	def setVolumeAnaOut34(self, a0):
		self.setOutVolume(3, a0)
		self.setOutVolume(4, a0)

	# Source selector for output jack
	def setAnaOut12(self, a0):
		self.setSelector('anaout12', a0)
	def setAnaOut34(self, a0):
		self.setSelector('anaout34', a0)
	def setDigiOut12(self, a0):
		self.setSelector('digiout12', a0)
	def setClkSrc(self, a0):
		# if streaming, this operation break connection
		ss = self.streamingstatus.selected()
		ss_txt = self.streamingstatus.getEnumLabel(ss)
		if (ss_txt == 'Idle'):
			self.setSelector('clksrc', a0)
		else:
			msg = QMessageBox()
			msg.question( msg, "Error", \
				"<qt>During streaming, this option is disable.</qt>", \
				QMessageBox.Ok)
			ctrl = self.JackSourceSelectors['clksrc']
			state = self.hw.getDiscrete(ctrl[0])
			ctrl[1].setCurrentIndex(state)
# vim: et
