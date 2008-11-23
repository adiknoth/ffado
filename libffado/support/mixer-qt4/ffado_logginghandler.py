#!/usr/bin/python
#
# Copyright (C) 2008 by Arnold Krille
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

from PyQt4.QtGui import QTextEdit, QAbstractSlider, QColor
from PyQt4.QtCore import QObject, SIGNAL, SLOT

import logging
log = logging.getLogger('logginghandler')

class QStatusLogger( QObject, logging.Handler ):
    def __init__( self, parent, statusbar, level=logging.NOTSET ):
        QObject.__init__( self, parent )
        logging.Handler.__init__( self, level )
        self.setFormatter( logging.Formatter( "%(name)s: %(message)s" ) )
        self.connect( self, SIGNAL("log(QString,int)"), statusbar, SLOT("showMessage(QString,int)") )

    def emit( self, record ):
        QObject.emit( self, SIGNAL("log(QString,int)"), "%s: %s" % (record.name, record.getMessage()), 5000 )

class QTextLogger( QTextEdit, logging.Handler ):
    def __init__( self, parent, level=logging.NOTSET ):
        QTextEdit.__init__( self, parent )
        logging.Handler.__init__( self, level )

        self.setReadOnly( True )
        self.setAcceptRichText( True )

    def emit( self, record ):
        color = QColor( "#000000" )
        if record.levelno > 20:
            color = QColor( "#ffff00" )
        if record.levelno > 30:
            color = QColor( "#ff0000" )
        if record.levelno <= 10:
            color = QColor( "#808080" )
        self.setTextColor( color )
        tmp = "%s %s: %s" % (record.asctime, record.name, record.getMessage())
        print tmp
        self.append( tmp )
        self.verticalScrollBar().triggerAction( QAbstractSlider.SliderToMaximum )

#
# vim: et
#
